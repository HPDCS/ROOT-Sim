/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
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
* @file scheduler.c
* @brief Re-entrant scheduler for LPs on worker threads
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
#include <scheduler/binding.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <scheduler/stf.h>
#include <mm/state.h>
#include <communication/communication.h>

#define _INIT_FROM_MAIN
#include <core/init.h>

#ifdef HAVE_CROSS_STATE
#include <mm/ecs.h>
#endif

#include <mm/dymelor.h>
#include <statistics/statistics.h>
#include <arch/thread.h>
#include <communication/communication.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <arch/x86/linux/cross_state_manager/cross_state_manager.h>
#include <queues/xxhash.h>

/// This is used to keep track of how many LPs were bound to the current KLT
__thread unsigned int n_prc_per_thread;

/// This global variable tells the simulator what is the local id of the LP currently being scheduled on the current worker thread
__thread LID_t current_lp;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread simtime_t current_lvt;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread msg_t *current_evt;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread void *current_state;

/*
* This function initializes the scheduler. In particular, it relies on MPI to broadcast to every simulation kernel process
* which is the actual scheduling algorithm selected.
*
* @author Francesco Quaglia
*
* @param sched The scheduler selected initially, but master can decide to change it, so slaves must rely on what master send to them
*/
void scheduler_init(void) {
	initialize_control_blocks();

	#ifdef HAVE_PREEMPTION
	preempt_init();
	#endif
}



static void destroy_LPs(void) {
	register unsigned int i;

	for(i = 0; i < n_prc; i++) {
//		rsfree(LPS(i)->queue_in);
//		rsfree(LPS(i)->queue_out);
//		rsfree(LPS(i)->queue_states);
//		rsfree(LPS(i)->bottom_halves);
//		rsfree(LPS(i)->rendezvous_queue);

		// Destroy stacks
//		rsfree(LPS(i)->stack);
	}

}



/**
* This function finalizes the scheduler
*
* @author Alessandro Pellegrini
*/
void scheduler_fini(void) {
	register unsigned int i;

	#ifdef HAVE_PREEMPTION
	preempt_fini();
	#endif

	destroy_LPs();

	for (i = 0; i < n_prc; i++) {
//		rsfree(LPS(i));
	}
//	rsfree(LPS); // These are now names of functions

//	rsfree(LPS_bound); // These are now names of functions
}



/**
* This is a LP main loop. It s the embodiment of the usrespace thread implementing the logic of the LP.
* Whenever an event is to be scheduled, the corresponding metadata are set by the <schedule>() function,
* which in turns calls <activate_LP>() to execute the actual context switch.
* This ProcessEvent wrapper explicitly returns control to simulation kernel user thread when an event
* processing is finished. In case the LP tries to access state data which is not belonging to its
* simulation state, a SIGSEGV signal is raised and the LP might be descheduled if it is not safe
* to perform the remote memory access. This is the only case where control is not returned to simulation
* thread explicitly by this wrapper.
*
* @author Francesco Quaglia
*
* @param args arguments passed to the LP main loop. Currently, this is not used.
*/
static void LP_main_loop(void *args) {

	#ifdef EXTRA_CHECKS
	unsigned long long hash1, hash2;
	hash1 = hash2 = 0;
	#endif

	(void)args; // this is to make the compiler stop complaining about unused args

	// Save a default context
	context_save(&LPS(current_lp)->default_context);

	while(true) {

		#ifdef EXTRA_CHECKS
		if(current_evt->size > 0) {
			hash1 = XXH64(current_evt->event_content, current_evt->size, LidToGid(current_lp));
		}
		#endif

		timer event_timer;
		timer_start(event_timer);

		// Process the event
		switch_to_application_mode();
		ProcessEvent[lid_to_int(current_lp)](gid_to_int(LidToGid(current_lp)), current_evt->timestamp, current_evt->type, current_evt->event_content, current_evt->size, current_state);
		switch_to_platform_mode();

		int delta_event_timer = timer_value_micro(event_timer);

		#ifdef EXTRA_CHECKS
		if(current_evt->size > 0) {
			hash2 = XXH64(current_evt->event_content, current_evt->size, LidToGid(current_lp));
		}

		if(hash1 != hash2) {
                        rootsim_error(true, "Error, LP %d has modified the payload of event %d during its processing. Aborting...\n", LidToGid(current_lp), current_evt->type);
		}
		#endif

		statistics_post_lp_data(current_lp, STAT_EVENT, 1.0);
		statistics_post_lp_data(current_lp, STAT_EVENT_TIME, delta_event_timer);

		// Give back control to the simulation kernel's user-level thread
		context_switch(&LPS(current_lp)->context, &kernel_context);
	}
}






/**
 * This function initializes a LP execution context. It allocates page-aligned memory for efficiency
 * reasons, and then calls <context_create>() which does the final trick.
 * <context_create>() uses global variables: LPs must therefore be intialized before creating new kernel threads
 * for supporting concurrent execution of LPs.
 *
 * @author Alessandro Pellegrini
 *
 * @date November 8, 2013
 *
 * @param lp the idex of the LP in the LPs descriptor table to be initialized
 */
void initialize_LP(LID_t lp) {
	unsigned int i;

	// Allocate LP stack
	LPS(lp)->stack = get_ult_stack(LP_STACK_SIZE);

	// Set the initial checkpointing period for this LP.
	// If the checkpointing period is fixed, this will not change during the
	// execution. Otherwise, new calls to this function will (locally) update
	// this.
	set_checkpoint_period(lp, rootsim_config.ckpt_period);


	// Initially, every LP is ready
	LPS(lp)->state = LP_STATE_READY;

	// There is no current state layout at the beginning
	LPS(lp)->current_base_pointer = NULL;

	// Initialize the queues
	LPS(lp)->queue_in = new_list(msg_t);
	LPS(lp)->queue_out = new_list(msg_hdr_t);
	LPS(lp)->queue_states = new_list(state_t);
	LPS(lp)->rendezvous_queue = new_list(msg_t);

	// Initialize the LP lock
	spinlock_init(&LPS(lp)->lock);

	// No event has been processed so far
	LPS(lp)->bound = NULL;

	LPS(lp)->outgoing_buffer.min_in_transit = rsalloc(sizeof(simtime_t) * n_cores);
	for(i = 0; i < n_cores; i++) {
		LPS(lp)->outgoing_buffer.min_in_transit[i] = INFTY;
	}

	#ifdef HAVE_CROSS_STATE
	// No read/write dependencies open so far for the LP. The current lp is always opened
	LPS(lp)->ECS_index = 0;
	LPS(lp)->ECS_synch_table[0] = LidToGid(lp); // LidToGid for distributed ECS
	#endif

	// Create user thread
	context_create(&LPS(lp)->context, LP_main_loop, NULL, LPS(lp)->stack, LP_STACK_SIZE);
}




void initialize_worker_thread(void) {
	communication_init_thread();

	// Divide LPs among worker threads, for the first time here
	rebind_LPs();
	if(master_thread() && master_kernel()) {
		printf("Initializing LPs... ");
		fflush(stdout);
	}

	// Initialize the LP control block for each locally hosted LP
	// and schedule the special INIT event
	int __helper1(LID_t lid, GID_t gid, unsigned int num, void *data) {
		msg_t *init_event;
		(void)num;
		(void)data;

		// Create user level thread for the current LP and initialize LP control block
		initialize_LP(lid);

		// Schedule an INIT event to the newly instantiated LP
		pack_msg(&init_event, gid, gid, INIT, 0.0, 0.0, 0, NULL);
	        init_event->mark = generate_mark(lid);

		list_insert_head(LPS(lid)->queue_in, init_event);
		LPS(lid)->state_log_forced = true;

		return 0; // continue to next element
	}
	LPS_bound_foreach(__helper1, NULL);

	// Worker Threads synchronization barrier: they all should start working together
	thread_barrier(&all_thread_barrier);

	if(master_thread() && master_kernel())
		printf("done\n");

	int __helper2(LID_t lid, GID_t gid, unsigned int num, void *data) {
		(void)lid;
		(void)gid;
		(void)num;
		(void)data;

		schedule();

		return 0; // continue to next element
	}
	LPS_bound_foreach(__helper2, NULL);

	// Worker Threads synchronization barrier: they all should start working together
	thread_barrier(&all_thread_barrier);

        #ifdef HAVE_PREEMPTION
        if(!rootsim_config.disable_preemption)
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
* @param lp The id of the LP to be scheduled
* @param lvt The lvt at which the LP is scheduled
* @param evt A pointer to the event to be processed by the LP
* @param state The simulation state to be passed to the LP
*/
void activate_LP(LID_t lp, simtime_t lvt, void *evt, void *state) {

	// Notify the LP main execution loop of the information to be used for actual simulation
	current_lp = lp;
	current_lvt = lvt;
	current_evt = evt;
	current_state = state;

//	#ifdef HAVE_PREEMPTION
//	if(!rootsim_config.disable_preemption)
//		enable_preemption();
//	#endif

	#ifdef HAVE_CROSS_STATE
	// Activate memory view for the current LP
	lp_alloc_schedule();
	#endif

	if(unlikely(is_blocked_state(LPS(lp)->state))) {
		rootsim_error(true, "Critical condition: LP %d has a wrong state -> %d. Aborting...\n", gid_to_int(LidToGid(lp)), LPS(lp)->state);
	}

	context_switch(&kernel_context, &LPS(lp)->context);

//	#ifdef HAVE_PREEMPTION
//        if(!rootsim_config.disable_preemption)
//                disable_preemption();
//        #endif

	#ifdef HAVE_CROSS_STATE
	// Deactivate memory view for the current LP if no conflict has arisen
	if(!is_blocked_state(LPS(lp)->state)) {
//		printf("Deschedule %d\n",lp);
		lp_alloc_deschedule();
	}
	#endif

	current_lp = idle_process;
	current_lvt = -1.0;
	current_evt = NULL;
	current_state = NULL;
}



bool check_rendevouz_request(LID_t lid){
	msg_t *temp_mess;

	if(LPS(lid)->state != LP_STATE_WAIT_FOR_SYNCH)
		return false;

	if(LPS(lid)->bound != NULL && list_next(LPS(lid)->bound) != NULL){
		temp_mess = list_next(LPS(lid)->bound);
		return temp_mess->type == RENDEZVOUS_START && LPS(lid)->wait_on_rendezvous > temp_mess->rendezvous_mark;
	}

	return false;

}

/**
* This function checks wihch LP must be activated (if any),
* and in turn activates it. This is used only to support forward execution.
*
* @author Alessandro Pellegrini
*/
void schedule(void) {

	LID_t lid;
	msg_t *event;
	void *state;

	#ifdef HAVE_CROSS_STATE
	bool resume_execution = false;
	#endif

	// Find next LP to be executed, depending on the chosen scheduler
	switch (rootsim_config.scheduler) {

		case SCHEDULER_STF:
			lid = smallest_timestamp_first();
			break;

		default:
			rootsim_error(true, "unrecognized scheduler!");
	}

	// No logical process found with events to be processed
	if (lid_equals(lid, idle_process)) {
		statistics_post_lp_data(lid, STAT_IDLE_CYCLES, 1.0);
		return;
	}

//	if(lid == 1 && LPS[lid]->state != LP_STATE_READY)
//		printf("state of lid 1 is %d\n",LPS[lid]->state);

	// If we have to rollback
	if(LPS(lid)->state == LP_STATE_ROLLBACK) {
		rollback(lid);
		LPS(lid)->state = LP_STATE_READY;
		send_outgoing_msgs(lid);
		return;
	}

	if(!is_blocked_state(LPS(lid)->state) && LPS(lid)->state != LP_STATE_READY_FOR_SYNCH){
		event = advance_to_next_event(lid);
	}
	else {
		event = LPS(lid)->bound;
	}


	// Sanity check: if we get here, it means that lid is a LP which has
	// at least one event to be executed. If advance_to_next_event() returns
	// NULL, it means that lid has no events to be executed. This is
	// a critical condition and we abort.
	if(unlikely(event == NULL)) {
		rootsim_error(true, "Critical condition: LP %d seems to have events to be processed, but I cannot find them. Aborting...\n", lid);
	}

	if(unlikely(!process_control_msg(event))) {
		return;
	}

	state = LPS(lid)->current_base_pointer;


#ifdef HAVE_CROSS_STATE
	// In case we are resuming an interrupted execution, we keep track of this.
	// If at the end of the scheduling the LP is not blocked, we can unblock all the remote objects
	if(is_blocked_state(LPS(lid)->state) || LPS(lid)->state == LP_STATE_READY_FOR_SYNCH) {
		resume_execution = true;
	}
#endif


	// Schedule the LP user-level thread
	if(LPS(lid)->state == LP_STATE_READY_FOR_SYNCH)
		LPS(lid)->state = LP_STATE_RUNNING_ECS;
	else
		LPS(lid)->state = LP_STATE_RUNNING;
	
	activate_LP(lid, lvt(lid), event, state);

	if(!is_blocked_state(LPS(lid)->state)) {
		LPS(lid)->state = LP_STATE_READY;
		send_outgoing_msgs(lid);
	}

#ifdef HAVE_CROSS_STATE
	if(resume_execution && !is_blocked_state(LPS(lid)->state)) {
		//printf("ECS event is finished mark %llu !!!\n", LPS(lid)->wait_on_rendezvous);
		//fflush(stdout);
		unblock_synchronized_objects(lid);

		// This is to avoid domino effect when relying on rendezvous messages
		force_LP_checkpoint(lid);
	}
#endif

	// Log the state, if needed
	LogState(lid);
}

