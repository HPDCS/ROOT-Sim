/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file state.c
* @brief The state module is responsible for managing LPs' simulation states. 
*	In particular, it allows to take a snapshot, to restore a previous snapshot,
*	and to silently re-execute a portion of simulation events to bring
*	a LP to a partiuclar LVT value for which no simulation state
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/


#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <core/core.h>
#include <core/timer.h>
#include <datatypes/list.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <mm/state.h>
#include <communication/communication.h>
#include <mm/dymelor.h>
#include <statistics/statistics.h>


/// Function pointer to switch between the parallel and serial version of SetState
void (*SetState)(void *new_state);


/**
* This function is used to create a state log to be added to the LP's log chain
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The Light Process Identifier
* @param last_event pointer to the last event executed just before the log
*/
void LogState(unsigned int lid) {

	bool take_snapshot = false;
	state_t new_state; // If inserted, list API makes a copy of this

	if(is_blocked_state(LPS[lid]->state)) {
		return;
	}
	
	// Keep track of the invocations to LogState
	LPS[lid]->from_last_ckpt++;
	
	if(LPS[lid]->state_log_forced) {
		LPS[lid]->state_log_forced = false;
		LPS[lid]->from_last_ckpt = 0;
		take_snapshot = true;
		goto skip_switch;
	}

	// Switch on the checkpointing mode.
	switch(rootsim_config.checkpointing) {

		case COPY_STATE_SAVING:
			take_snapshot = true;
			break;

		case PERIODIC_STATE_SAVING:
			if(LPS[lid]->from_last_ckpt >= LPS[lid]->ckpt_period) {
				take_snapshot = true;
				LPS[lid]->from_last_ckpt = 0;
			}
			break;

		default:
			rootsim_error(true, "State saving mode not supported.");
	}

    skip_switch:

	// Shall we take a log?
	if (take_snapshot) { 

		// Take a log and set the associated LVT
		new_state.log = log_state(lid);
		new_state.lvt = lvt(lid);
		new_state.last_event = LPS[lid]->bound;
		
		// We take as the buffer state the last one associated with a SetState() call, if any
		new_state.buffer_state = (LPS[lid]->state_bound != NULL ? LPS[lid]->state_bound->buffer_state : NULL);
		
		// list_insert() makes a copy of the payload, which is then returned. This is our state bound.
		LPS[lid]->state_bound = list_insert_tail(LPS[lid]->queue_states, &new_state);
		
	}
}



/**
* This function bring the state pointed by "state" to "final time" by re-executing all the events without sending any messages
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The id of the Light Process
* @param state The simulation state to be passed to the LP
* @param pointer The pointer to the element of the input event queue from which start the re-execution
* @param final_time The time where align the re-execution
* @param bound If not null, silent execution updates the passed bound pointer to the last reprocessed event
* 
* @return The number of events re-processed during the silent execution
*/
unsigned int silent_execution(unsigned int lid, void *state_buffer, msg_t *evt, simtime_t final_time, msg_t **bound) {
	unsigned int events = 0;
	unsigned short int old_state;
	
	// current state can be either idle READY, BLOCKED or ROLLBACK, so we save it and then put it back in place
	old_state = LPS[lid]->state;
	LPS[lid]->state = LP_STATE_SILENT_EXEC;
	
	// Reprocess events. Outgoing messages are explicitly discarded, as this part of
	// the simulation has been already executed at least once
	while(evt != NULL && evt->timestamp <= final_time) {


		if(!reprocess_control_msg(evt)) {
			evt = list_next(evt);
			continue;
		}

		events++;

		if(bound != NULL) {
			*bound = evt;
		}

		activate_LP(lid, evt->timestamp, evt, state_buffer);	
		evt = list_next(evt);
	}
	
	LPS[lid]->state = old_state;
	
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
* @param lid The Logical Process Id
*/
void rollback(unsigned int lid) {
	
	state_t *restore_state, *s;
	simtime_t restore_time;
	msg_t *msg_ptr, *msg_ptr_next;
	
	// Sanity check
	if(LPS[lid]->state != LP_STATE_ROLLBACK) {
		rootsim_error(false, "I'm asked to roll back LP %d's execution, but rollback_bound is not set. Ignoring...\n", LidToGid(lid));
		return;
	}

	statistics_post_lp_data(lid, STAT_ROLLBACK, 1.0);
	
	restore_time = LPS[lid]->bound->timestamp;

	// Send antimessages
	send_antimessages(lid, restore_time);

	// Find the state to be restored, and prune the wrongly computed states
	restore_state = list_tail(LPS[lid]->queue_states);
	while (restore_state != NULL && restore_state->lvt > restore_time) {
		s = restore_state;
		restore_state = list_prev(restore_state);
		log_delete(s->log);
		s->last_event = (void *)0xDEADC0DE;
		list_delete_by_content(LPS[lid]->queue_states, s);
	}
		
	// Restore the simulation state and correct the state bound
	log_restore(lid, restore_state);
	LPS[lid]->state_bound = restore_state;
	
	// Coasting forward, updating the bound

	// TODO: ma può essere davvero nullo questo? forse solo intorno a INIT...
	if(LPS[lid]->state_bound->last_event != NULL) {
		silent_execution(lid, restore_state->buffer_state, list_next(LPS[lid]->state_bound->last_event), restore_time, &LPS[lid]->bound);
	}

	rollback_control_message(lid, restore_time);
}




/**
* This function computes the time barrier
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The light Process Id
* @param gvt The global virtual time
* @return A pointer to the state that represents the time barrier
*/
state_t *find_time_barrier(int lid, simtime_t gvt) {

	state_t *barrier_state;

//	gvt *=0.5;

	if(D_EQUAL(gvt, 0.0)) {
		return list_head(LPS[lid]->queue_states);
	}

	barrier_state = list_tail(LPS[lid]->queue_states);
	
	// Sanity check. This might happen if the LP has not executed INIT at the time of the first GVT reduction
	if(barrier_state == NULL) {
		return NULL;
	}
	
	//~ barrier_state = curr;

	//~ printf("Find time barrier (GVT = %f, head = %f, curr: %p %f): ", gvt, list_head(LPS[lid]->queue_states)->lvt, curr, curr->lvt);
	// current must point to the state with lvt equals to or immediately under the gvt
	while (barrier_state != NULL && barrier_state->lvt > gvt) {
		//~ printf("%f, ", curr->lvt);
		//~ barrier_state = curr;
		//~ curr = list_prev(curr);
		barrier_state = list_prev(barrier_state);
		//~ printf("(curr %p %f), ", curr, curr->lvt);
  	}
  	//~ printf("\n");
  	
	// Sanity checks
	// TODO: This should not be necessary, but if removed sometimes it crashes (e.g., w/ fujimoto's gvt)
	if(barrier_state == NULL) {
		return list_head(LPS[lid]->queue_states);
	}
//	printf("%d: GVT = %f, bound: %f, TB = %f, p: %f, n: %f\n", lid, gvt, LPS[lid]->bound->timestamp, barrier_state->lvt, 
//		(list_prev(barrier_state) != NULL ? list_prev(barrier_state)->lvt : -1.0),
//		(list_next(barrier_state) != NULL ? list_next(barrier_state)->lvt : -1.0));
	if (barrier_state->lvt > gvt) {
		rootsim_error(true, "Time barrier=%f, found for LP %d. Greater than gvt=%f! Aborting...\n", barrier_state->lvt, lid, gvt);
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
void ParallelSetState(void *new_state) {
	
	// TODO: cosa succede se il modello chiama SetState durante la silent execution chiamata da CCGS? Si fa inspection di uno stato diverso...
	// If we are reprocessing events, then SetState was already called
	if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC) {
		return;
	}

/*	if(list_empty(LPS[current_lp]->queue_states)) {
		force_LP_checkpoint(current_lp);
		LogState(current_lp);
	}
*/	
	LPS[current_lp]->state_bound->buffer_state = new_state;
}







/**
* This function sets the checkpoint mode
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param ckpt_mode The new checkpoint mode
*/
void set_checkpoint_mode(int ckpt_mode) {
	rootsim_config.checkpointing = ckpt_mode;
}




/**
* This function sets the checkpoint period
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The Logical Process Id
* @param period The new checkpoint period
*/
void set_checkpoint_period(unsigned int lid, int period) {
	LPS[lid]->ckpt_period = period; 
}


/**
* This function tells the logging subsystem to take a LP state log
* upon the next invocation to <LogState>(), independently of the current
* checkpointing period
*
* @author Alessandro Pellegrini
*
* @param lid The Logical Process Id
* @param period The new checkpoint period
*/
void force_LP_checkpoint(unsigned int lid) {
	LPS[lid]->state_log_forced = true; 
}

