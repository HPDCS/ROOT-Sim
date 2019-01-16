/**
 * @file scheduler/scheduler.c
 *
 * @brief The ROOT-Sim scheduler main module
 *
 * This module implements the schedule() function, which is the main
 * entry point for all the schedulers implemented in ROOT-Sim, and
 * several support functions which allow to initialize worker threads.
 *
 * Also, the LP_main_loop() function, which is the function where all
 * the User-Level Threads associated with Logical Processes live, is
 * defined here.
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
 * @author Roberto Vitali
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <datatypes/list.h>
#include <datatypes/msgchannel.h>
#include <core/core.h>
#include <core/timer.h>
#include <arch/atomic.h>
#include <arch/ult.h>
#include <arch/thread.h>
#include <core/init.h>
#include <scheduler/binding.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <scheduler/stf.h>
#include <mm/state.h>
#include <communication/communication.h>

#ifdef HAVE_CROSS_STATE
#include <mm/ecs.h>
#endif

#include <mm/mm.h>
#include <statistics/statistics.h>
#include <arch/thread.h>
#include <communication/communication.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <arch/x86/linux/cross_state_manager/cross_state_manager.h>
#include <queues/xxhash.h>

/// This is used to keep track of how many LPs were bound to the current KLT
__thread unsigned int n_prc_per_thread;

/// This is a per-thread variable pointing to the block state of the LP currently scheduled
__thread struct lp_struct *current;

/**
 * This is a per-thread variable telling what is the event that should be executed
 * when activating an LP. It is incorrect to rely on current->bound, as there
 * are cases (such as the silent execution) in which we have a certain bound set,
 * but we execute a different event.
 *
 * @todo We should uniform this behaviour, and drop current_evt, as this might be
 *       misleading when reading the code.
 */
__thread msg_t *current_evt;

/*
* This function initializes the scheduler. In particular, it relies on MPI to broadcast to every simulation kernel process
* which is the actual scheduling algorithm selected.
*
* @author Francesco Quaglia
*
* @param sched The scheduler selected initially, but master can decide to change it, so slaves must rely on what master send to them
*/
void scheduler_init(void)
{
#ifdef HAVE_PREEMPTION
	preempt_init();
#endif
}

/**
* This function finalizes the scheduler
*
* @author Alessandro Pellegrini
*/
void scheduler_fini(void)
{
#ifdef HAVE_PREEMPTION
	preempt_fini();
#endif

	foreach_lp(lp) {
		rsfree(lp->queue_in);
		rsfree(lp->queue_out);
		rsfree(lp->queue_states);
		rsfree(lp->bottom_halves);
		rsfree(lp->rendezvous_queue);

		// Destroy stacks
		rsfree(lp->stack);

		rsfree(lp);
	}

	rsfree(lps_blocks);
	rsfree(lps_bound_blocks);
}

/**
* This is a LP main loop. It s the embodiment of the usrespace thread implementing the logic of the LP.
* Whenever an event is to be scheduled, the corresponding metadata are set by the schedule() function,
* which in turns calls activate_LP() to execute the actual context switch.
* This ProcessEvent wrapper explicitly returns control to simulation kernel user thread when an event
* processing is finished. In case the LP tries to access state data which is not belonging to its
* simulation state, a SIGSEGV signal is raised and the LP might be descheduled if it is not safe
* to perform the remote memory access. This is the only case where control is not returned to simulation
* thread explicitly by this wrapper.
*
* @param args arguments passed to the LP main loop. Currently, this is not used.
*/
void LP_main_loop(void *args)
{
#ifdef EXTRA_CHECKS
	unsigned long long hash1, hash2;
	hash1 = hash2 = 0;
#endif

	(void)args;		// this is to make the compiler stop complaining about unused args

	// Save a default context
	context_save(&current->default_context);

	while (true) {

#ifdef EXTRA_CHECKS
		if (current->bound->size > 0) {
			hash1 = XXH64(current_evt->event_content, current_evt->size, current->gid);
		}
#endif

		timer event_timer;
		timer_start(event_timer);

		// Process the event
		if(&abm_settings){
			ProcessEventABM();
		}else if (&topology_settings){
			ProcessEventTopology();
		}else{
			switch_to_application_mode();
			current->ProcessEvent(current->gid.to_int,
				      current_evt->timestamp, current_evt->type,
				      current_evt->event_content,
				      current_evt->size,
				      current->current_base_pointer);
			switch_to_platform_mode();
		}
		int delta_event_timer = timer_value_micro(event_timer);

#ifdef EXTRA_CHECKS
		if (current->bound->size > 0) {
			hash2 =
			    XXH64(current_evt->event_content, current_evt->size,
				  current->gid);
		}

		if (hash1 != hash2) {
			rootsim_error(true,
				      "Error, LP %d has modified the payload of event %d during its processing. Aborting...\n",
				      current->gid, current->bound->type);
		}
#endif

		statistics_post_data(current, STAT_EVENT, 1.0);
		statistics_post_data(current, STAT_EVENT_TIME,
				     delta_event_timer);

		// Give back control to the simulation kernel's user-level thread
		context_switch(&current->context, &kernel_context);
	}
}

void initialize_worker_thread(void)
{
	msg_t *init_event;

	// Divide LPs among worker threads, for the first time here
	rebind_LPs();
	if (master_thread() && master_kernel()) {
		printf("Initializing LPs... ");
		fflush(stdout);
	}
	// Worker Threads synchronization barrier: they all should start working together
	thread_barrier(&all_thread_barrier);

	if (master_thread() && master_kernel())
		printf("done\n");

	// Schedule an INIT event to the newly instantiated LP
	// We need two separate foreach_bound_lp here, because
	// in this way we are sure that there is at least one
	// event to be used as the bound and we do not have to make
	// any check on null throughout the scheduler code.
	foreach_bound_lp(lp) {
		pack_msg(&init_event, lp->gid, lp->gid, INIT, 0.0, 0.0, 0, NULL);
		init_event->mark = generate_mark(lp);
		list_insert_head(lp->queue_in, init_event);
		lp->state_log_forced = true;
	}

	thread_barrier(&all_thread_barrier);

	foreach_bound_lp(lp) {
		schedule_on_init(lp);
	}

	// Worker Threads synchronization barrier: they all should start working together
	thread_barrier(&all_thread_barrier);

#ifdef HAVE_PREEMPTION
	if (!rootsim_config.disable_preemption)
		enable_preemption();
#endif

}

/**
* This function is the application-level ProcessEvent() callback entry point.
* It allows to specify which lp must be scheduled, specifying its lvt, its event
* to be executed and its simulation state.
* This provides a general entry point to application-level code, to be used
* if the LP is in forward execution, in coasting forward or in initialization.
*
* @author Alessandro Pellegrini
*
* @date November 11, 2013
*
* @param next A pointer to the lp_struct of the LP which has to be activated
* @param evt A pointer to the event to be processed by the LP
*/
void activate_LP(struct lp_struct *next, msg_t * evt)
{

	// Notify the LP main execution loop of the information to be used for actual simulation
	current = next;
	current_evt = evt;

//      #ifdef HAVE_PREEMPTION
//      if(!rootsim_config.disable_preemption)
//              enable_preemption();
//      #endif

#ifdef HAVE_CROSS_STATE
	// Activate memory view for the current LP
	lp_alloc_schedule();
#endif

	if (unlikely(is_blocked_state(next->state))) {
		rootsim_error(true, "Critical condition: LP %d has a wrong state: %d. Aborting...\n",
			      next->gid.to_int, next->state);
	}

	context_switch(&kernel_context, &next->context);

//      #ifdef HAVE_PREEMPTION
//        if(!rootsim_config.disable_preemption)
//                disable_preemption();
//        #endif

#ifdef HAVE_CROSS_STATE
	// Deactivate memory view for the current LP if no conflict has arisen
	if (!is_blocked_state(next->state)) {
//              printf("Deschedule %d\n",lp);
		lp_alloc_deschedule();
	}
#endif

	current = NULL;
}

bool check_rendevouz_request(struct lp_struct *lp)
{
	msg_t *temp_mess;

	if (lp->state != LP_STATE_WAIT_FOR_SYNCH)
		return false;

	if (lp->bound != NULL && list_next(lp->bound) != NULL) {
		temp_mess = list_next(lp->bound);
		return temp_mess->type == RENDEZVOUS_START && lp->wait_on_rendezvous > temp_mess->rendezvous_mark;
	}

	return false;
}

/**
* This function checks wihch LP must be activated (if any),
* and in turn activates it. This is used only to support forward execution.
*
* @author Alessandro Pellegrini
*/
void schedule(void)
{
	struct lp_struct *next;
	msg_t *event;

#ifdef HAVE_CROSS_STATE
	bool resume_execution = false;
#endif

	// Find the next LP to be scheduled
	switch (rootsim_config.scheduler) {

	case SCHEDULER_STF:
		next = smallest_timestamp_first();
		break;

	default:
		rootsim_error(true, "unrecognized scheduler!");
	}

	// No logical process found with events to be processed
	if (next == NULL) {
		statistics_post_data(NULL, STAT_IDLE_CYCLES, 1.0);
		return;
	}
	// If we have to rollback
	if (next->state == LP_STATE_ROLLBACK) {
		rollback(next);
		next->state = LP_STATE_READY;
		send_outgoing_msgs(next);
		return;
	}

	if (!is_blocked_state(next->state)
	    && next->state != LP_STATE_READY_FOR_SYNCH) {
		event = advance_to_next_event(next);
	} else {
		event = next->bound;
	}

	// Sanity check: if we get here, it means that lid is a LP which has
	// at least one event to be executed. If advance_to_next_event() returns
	// NULL, it means that lid has no events to be executed. This is
	// a critical condition and we abort.
	if (unlikely(event == NULL)) {
		rootsim_error(true,
			      "Critical condition: LP %d seems to have events to be processed, but I cannot find them. Aborting...\n",
			      next->gid);
	}

	if (unlikely(!process_control_msg(event))) {
		return;
	}
#ifdef HAVE_CROSS_STATE
	// In case we are resuming an interrupted execution, we keep track of this.
	// If at the end of the scheduling the LP is not blocked, we can unblock all the remote objects
	if (is_blocked_state(next->state) || next->state == LP_STATE_READY_FOR_SYNCH) {
		resume_execution = true;
	}
#endif

	// Schedule the LP user-level thread
	if (next->state == LP_STATE_READY_FOR_SYNCH)
		next->state = LP_STATE_RUNNING_ECS;
	else
		next->state = LP_STATE_RUNNING;

	activate_LP(next, event);

	if (!is_blocked_state(next->state)) {
		next->state = LP_STATE_READY;
		send_outgoing_msgs(next);
	}
#ifdef HAVE_CROSS_STATE
	if (resume_execution && !is_blocked_state(next->state)) {
		//printf("ECS event is finished mark %llu !!!\n", next->wait_on_rendezvous);
		fflush(stdout);
		unblock_synchronized_objects(next);

		// This is to avoid domino effect when relying on rendezvous messages
		force_LP_checkpoint(next);
	}
#endif

	// Log the state, if needed
	LogState(next);
}

void schedule_on_init(struct lp_struct *next)
{
	msg_t *event;

#ifdef HAVE_CROSS_STATE
	bool resume_execution = false;
#endif

	event = list_head(next->queue_in);
	next->bound = event;


	// Sanity check: if we get here, it means that lid is a LP which has
	// at least one event to be executed. If advance_to_next_event() returns
	// NULL, it means that lid has no events to be executed. This is
	// a critical condition and we abort.
	if (unlikely(event == NULL) || event->type != INIT) {
		rootsim_error(true,
			      "Critical condition: LP %d should have an INIT event but I cannot find it. Aborting...\n",
			      next->gid);
	}

#ifdef HAVE_CROSS_STATE
	// In case we are resuming an interrupted execution, we keep track of this.
	// If at the end of the scheduling the LP is not blocked, we can unblock all the remote objects
	if (is_blocked_state(next->state) || next->state == LP_STATE_READY_FOR_SYNCH) {
		resume_execution = true;
	}
#endif

	next->state = LP_STATE_RUNNING;

	activate_LP(next, event);

	if (!is_blocked_state(next->state)) {
		next->state = LP_STATE_READY;
		send_outgoing_msgs(next);
	}
#ifdef HAVE_CROSS_STATE
	if (resume_execution && !is_blocked_state(next->state)) {
		//printf("ECS event is finished mark %llu !!!\n", next->wait_on_rendezvous);
		fflush(stdout);
		unblock_synchronized_objects(next);

		// This is to avoid domino effect when relying on rendezvous messages
		force_LP_checkpoint(next);
	}
#endif

	// Log the state, if needed
	LogState(next);
}
