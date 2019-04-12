/**
* @file mm/state.c
*
* @brief LP state management
*
* The state module is responsible for managing LPs' simulation states.
* In particular, it allows to take a snapshot, to restore a previous snapshot,
* and to silently re-execute a portion of simulation events to bring
* a LP to a partiuclar LVT value for which no simulation state is available
* in the log chain.
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#include <datatypes/list.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <mm/state.h>
#include <communication/communication.h>
#include <mm/mm.h>
#include <statistics/statistics.h>

/**
* This function is used to create a state log to be added to the LP's log chain
*
* @param lp A pointer to the lp_struct of the LP for which a checkpoint
*           is to be taken.
*/
bool LogState(struct lp_struct *lp)
{
	bool take_snapshot = false;
	state_t *new_state;

	if (unlikely(is_blocked_state(lp->state))) {
		return take_snapshot;
	}
	// Keep track of the invocations to LogState
	lp->from_last_ckpt++;

	if (lp->state_log_forced) {
		lp->state_log_forced = false;
		lp->from_last_ckpt = 0;

		take_snapshot = true;
		goto skip_switch;
	}
	// Switch on the checkpointing mode
	switch (rootsim_config.checkpointing) {

	case STATE_SAVING_COPY:
		take_snapshot = true;
		break;

	case STATE_SAVING_PERIODIC:
		if (lp->from_last_ckpt >= lp->ckpt_period) {
			take_snapshot = true;
			lp->from_last_ckpt = 0;
		}
		break;

	default:
		rootsim_error(true, "State saving mode not supported.");
	}

 skip_switch:

	// Shall we take a log?
	if (take_snapshot) {

		// Allocate the state buffer
		new_state = rsalloc(sizeof(*new_state));

		// Associate the checkpoint with current LVT and last-executed event
		new_state->lvt = lvt(lp);
		new_state->last_event = lp->bound;

		// Log simulation model buffers
		new_state->log = log_state(lp);

		// Log members of lp_struct which must be restored
		new_state->state = lp->state;
		new_state->base_pointer = lp->current_base_pointer;

		// Log library-related states
		memcpy(&new_state->numerical, &lp->numerical,
		       sizeof(numerical_state_t));

		if(&topology_settings && topology_settings.write_enabled){
			new_state->topology = rsalloc(topology_global.chkp_size);
			memcpy(new_state->topology, lp->topology,
					topology_global.chkp_size);
		}

		if(&abm_settings){
			new_state->region_data = abm_do_checkpoint(lp->region);
		}

		// Link the new checkpoint to the state chain
		list_insert_tail(lp->queue_states, new_state);

	}

	return take_snapshot;
}

void RestoreState(struct lp_struct *lp, state_t * restore_state)
{
	// Restore simulation model buffers
	log_restore(lp, restore_state);

	// Restore members of lp_struct which have been checkpointed
	lp->current_base_pointer = restore_state->base_pointer;
	lp->state = restore_state->state;

	// Restore library-related states
	memcpy(&lp->numerical, &restore_state->numerical,
	       sizeof(numerical_state_t));

	if(&topology_settings && topology_settings.write_enabled){
		memcpy(lp->topology, restore_state->topology,
				topology_global.chkp_size);
	}

	if(&abm_settings)
		abm_restore_checkpoint(restore_state->region_data, lp->region);

#ifdef HAVE_CROSS_STATE
	lp->ECS_index = 0;
	lp->wait_on_rendezvous = 0;
	lp->wait_on_object = 0;
#endif
}

/**
* This function bring the state pointed by "state" to "final time" by re-executing all the events without sending any messages
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lp A pointer to the LP's lp_struct for which we want to silently
*           reprocess already-executed events
* @param evt A pointer to the event from which start the re-execution
* @param final_evt A pointer to the first event which should *not* be reprocessed in silent execution
*
* @return The number of events re-processed during the silent execution
*/
unsigned int silent_execution(struct lp_struct *lp, msg_t *evt, msg_t *final_evt)
{
	unsigned int events = 0;
	unsigned short int old_state;

	// current state can be either idle READY, BLOCKED or ROLLBACK, so we save it and then put it back in place
	old_state = lp->state;
	lp->state = LP_STATE_SILENT_EXEC;

	// This is true if the restored state was taken exactly after the new bound
	if (evt == final_evt)
		goto out;

	evt = list_next(evt);
	final_evt = list_next(final_evt);

	// Reprocess events. Outgoing messages are explicitly discarded, as this part of
	// the simulation has been already executed at least once
	while (evt != NULL && evt != final_evt) {

		if (unlikely(!reprocess_control_msg(evt))) {
			evt = list_next(evt);
			continue;
		}

		events++;
		activate_LP(lp, evt);
		evt = list_next(evt);
	}

 out:
	lp->state = old_state;
	return events;
}

/**
* This function rolls back the execution of a certain LP. The point where the
* execution is rolled back is identified by the event pointed by the rollback_bound
* entry in the LP control block.
* For a rollback operation to take place, that pointer must be set before calling
* this function.
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lp A pointer to the lp_struct of the LP to rollback
*/
void rollback(struct lp_struct *lp)
{
	state_t *restore_state, *s;
	msg_t *last_correct_event;
	msg_t *last_restored_event;
	unsigned int reprocessed_events;

	// Sanity check
	if (unlikely(lp->state != LP_STATE_ROLLBACK)) {
		rootsim_error(false, "I'm asked to roll back LP %d's execution, but rollback_bound is not set. Ignoring...\n",
			      lp->gid.to_int);
		return;
	}

	// Discard any possible execution state related to a blocked execution
	memcpy(&lp->context, &lp->default_context, sizeof(LP_context_t));

	statistics_post_data(lp, STAT_ROLLBACK, 1.0);

	last_correct_event = lp->bound;
	// Send antimessages
	send_antimessages(lp, last_correct_event->timestamp);

	// Find the state to be restored, and prune the wrongly computed states
	restore_state = list_tail(lp->queue_states);
	while (restore_state != NULL && restore_state->lvt > last_correct_event->timestamp) {	// It's > rather than >= because we have already taken into account simultaneous events
		s = restore_state;
		restore_state = list_prev(restore_state);
		log_delete(s->log);
#ifndef NDEBUG
		s->last_event = (void *)0xBABEBEEF;
#endif
		list_delete_by_content(lp->queue_states, s);
	}
	// Restore the simulation state and correct the state base pointer
	RestoreState(lp, restore_state);

	last_restored_event = restore_state->last_event;
	reprocessed_events = silent_execution(lp, last_restored_event, last_correct_event);
	statistics_post_data(lp, STAT_SILENT, (double)reprocessed_events);

	// TODO: silent execution resets the LP state to the previous
	// value, so it should be the last function to be called within rollback()
	// Control messages must be rolled back as well
	rollback_control_message(lp, last_correct_event->timestamp);
}

/**
* This function computes a time barrier, namely the first state snapshot
* which is associated with a simulation time <= than the simtime value
* passed as an argument.
* The time barrier, in the runtime environment, is used to safely install
* a new computed GVT.
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lp A pointer to the lp_struct of the LP for which we are looking
*           for the current time barrier
* @param simtime The simulation time to be associated with a state barrier
* @return A pointer to the state that represents the time barrier
*/
state_t *find_time_barrier(struct lp_struct *lp, simtime_t simtime)
{
	state_t *barrier_state;

	if (unlikely(D_EQUAL(simtime, 0.0))) {
		return list_head(lp->queue_states);
	}

	barrier_state = list_tail(lp->queue_states);

	// Must point to the state with lvt immediately before the GVT
	while (barrier_state != NULL && barrier_state->lvt >= simtime) {
		barrier_state = list_prev(barrier_state);
	}
	if (barrier_state == NULL) {
		barrier_state = list_head(lp->queue_states);
	}

/*
	// TODO Search for the first full log before the gvt
	while(true) {
		if(is_incremental(current->log) == false)
			break;
	  	current = list_prev(current);
	}
*/

	return barrier_state;
}

/**
* This function sets the buffer of the current LP's state
*
* @author Francesco Quaglia
*
* @param new_state The new buffer
*
* @todo malloc wrapper
*/
void SetState(void *new_state)
{
	current->current_base_pointer = new_state;
}

/**
* This function sets the checkpoint mode
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param ckpt_mode The new checkpoint mode
*/
void set_checkpoint_mode(int ckpt_mode)
{
	rootsim_config.checkpointing = ckpt_mode;
}

/**
* This function sets the checkpoint period
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lp A pointer to the LP's lp_struct which should have its
*           checkpoint period changed.
* @param period The new checkpoint period
*/
void set_checkpoint_period(struct lp_struct *lp, int period)
{
	lp->ckpt_period = period;
}

/**
* This function tells the logging subsystem to take a LP state log
* upon the next invocation to LogState(), independently of the current
* checkpointing period
*
* @author Alessandro Pellegrini
*
* @param lp A pointer to the lp_struct of the LP for which the checkpoint
*           shold be forced after the next call to LogState()
*/
void force_LP_checkpoint(struct lp_struct *lp)
{
	lp->state_log_forced = true;
}
