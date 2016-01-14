/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#include <arch/atomic.h>
#include <arch/ult.h>
#include <arch/thread.h>
#include <scheduler/binding.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <scheduler/stf.h>
#include <mm/state.h>

#ifdef HAVE_CROSS_STATE
#include <mm/ecs.h>
#endif

#include <mm/dymelor.h>
#include <statistics/statistics.h>
#include <arch/thread.h>
#include <communication/communication.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>
#include <queues/xxhash.h>

/// Maintain LPs' simulation and execution states
LP_state **LPS = NULL;

/// This is used to keep track of how many LPs were bound to the current KLT
__thread unsigned int n_prc_per_thread;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread unsigned int current_lp;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread simtime_t current_lvt;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread msg_t *current_evt;

/// This global variable tells the simulator what is the LP currently being scheduled on the current worker thread
__thread void *current_state;

//TODO MN
#ifdef HAVE_GLP_SCH_MODULE
/// Maintain groups' state 
GLP_state **GLPS = NULL;
#endif

/*
* This function initializes the scheduler. In particular, it relies on MPI to broadcast to every simulation kernel process
* which is the actual scheduling algorithm selected.
*
* @author Francesco Quaglia
*
* @param sched The scheduler selected initially, but master can decide to change it, so slaves must rely on what master send to them
*/
void scheduler_init(void) {

	register unsigned int i;

	// TODO: implementare con delle broadcast!!
/*	if(n_ker > 1) {
		if (master_kernel()) {
			for (i = 1; i < n_ker; i++) {
				comm_send(&rootsim_config.scheduler, sizeof(rootsim_config.scheduler), MPI_CHAR, i, MSG_INIT_MPI, MPI_COMM_WORLD);
			}
		} else {
			comm_recv(&rootsim_config.scheduler, sizeof(rootsim_config.scheduler), MPI_CHAR, 0, MSG_INIT_MPI, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}
*/

	// Allocate LPS control blocks
	LPS = (LP_state **)rsalloc(n_prc * sizeof(LP_state *));
	for (i = 0; i < n_prc; i++) {
		LPS[i] = (LP_state *)rsalloc(sizeof(LP_state));
		bzero(LPS[i], sizeof(LP_state));

		// Allocate memory for the outgoing buffer 
		LPS[i]->outgoing_buffer.max_size = INIT_OUTGOING_MSG;
		LPS[i]->outgoing_buffer.outgoing_msgs = rsalloc(sizeof(msg_t) * INIT_OUTGOING_MSG);

		// That's the only sequentially executed place where we can set the lid
		LPS[i]->lid = i;
	
		//TODO MN
		#ifdef HAVE_GLP_SCH_MODULE
		// Allocate ECS_stat_table
		LPS[i]->updated_counter = false;
		LPS[i]->ECS_stat_table = rsalloc(n_grp * sizeof(ECS_stat *));
		unsigned int j;
		for (j = 0; j < n_prc; j++) {
			LPS[i]->ECS_stat_table[j] = rsalloc(sizeof(ECS_stat));
			bzero(LPS[i]->ECS_stat_table[j], sizeof(ECS_stat));
			
			//NOTE: each entry of ECS_stat_table must be initialise otherwise it can figure 
			//      out problem during the first access
			LPS[i]->ECS_stat_table[j]->last_access = -1.0;
			
		}
		#endif
	}

	//TODO MN
	#ifdef HAVE_GLP_SCH_MODULE
	// Allocate GLPS control blocks
	GLPS = rsalloc(n_grp * sizeof(GLP_state *));
	for (i = 0; i < n_grp; i++) {
		GLPS[i] = rsalloc(sizeof(GLP_state));
		bzero(GLPS[i], sizeof(GLP_state));

		GLPS[i]->local_LPS = rsalloc(n_prc * sizeof(LP_state *));
		/*
		unsigned int j;
		for (j = 0; j < n_prc; j++) {
			GLPS[i]->local_LPS[j] = NULL;
		}
		*/

	        //Initialise current group
	        LPS[i]->current_group = i;

        	//Initialise GROUPS
	        spinlock_init(&GLPS[i]->lock);
		GLPS[i]->id = i;
        	GLPS[i]->local_LPS[0] = LPS[i];
	        GLPS[i]->tot_LP = 1;
		GLPS[i]->initial_group_time = (msg_t *)rsalloc(sizeof(msg_t));
		GLPS[i]->initial_group_time->timestamp = -1.0;
		GLPS[i]->initial_group_time->mark = 0;
		GLPS[i]->counter_rollback = 0;
		GLPS[i]->lvt = NULL;
		GLPS[i]->counter_synch = 0;
		GLPS[i]->state = GLP_STATE_WAIT_FOR_GROUP;

	}
        #endif
	
	#ifdef HAVE_PREEMPTION
	preempt_init();
	#endif
}



static void destroy_LPs(void) {
	register unsigned int i;

	for(i = 0; i < n_prc; i++) {
//		rsfree(LPS[i]->queue_in);
//		rsfree(LPS[i]->queue_out);
//		rsfree(LPS[i]->queue_states);
//		rsfree(LPS[i]->bottom_halves);
//		rsfree(LPS[i]->rendezvous_queue);

		// Destroy stacks
		#ifdef ENABLE_ULT
		rsfree(LPS[i]->stack);
		#endif
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
		rsfree(LPS[i]);
	}
	rsfree(LPS);

	rsfree(LPS_bound);
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
	#ifdef ENABLE_ULT
	context_save(&LPS[current_lp]->default_context);
	#endif

	while(true) {

		#ifdef EXTRA_CHECKS
		if(current_evt->size > 0) {
			hash1 = XXH64(current_evt->event_content, current_evt->size, current_lp);
		}
		#endif

		// Process the event
		timer event_timer;
		timer_start(event_timer);
		
		switch_to_application_mode();
		
//		printf("Timestamp: %f\n",current_evt->timestamp);
		
		ProcessEvent[current_lp](LidToGid(current_lp), current_evt->timestamp, current_evt->type, current_evt->event_content, current_evt->size, current_state);
		
	
		switch_to_platform_mode();

		int delta_event_timer = timer_value_micro(event_timer);

		#ifdef EXTRA_CHECKS
		if(current_evt->size > 0) {
			hash2 = XXH64(current_evt->event_content, current_evt->size, current_lp);
		}

		if(hash1 != hash2) {
                        rootsim_error(true, "Error, LP %d has modified the payload of event %d during its processing. Aborting...\n", current_lp, current_evt->type);
		}
		#endif

		statistics_post_lp_data(current_lp, STAT_EVENT, 1.0);
		statistics_post_lp_data(current_lp, STAT_EVENT_TIME, delta_event_timer);

		// Give back control to the simulation kernel's user-level thread
		#ifdef ENABLE_ULT
		context_switch(&LPS[current_lp]->context, &kernel_context);
		#else
		return;
		#endif
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
void initialize_LP(unsigned int lp) {
	unsigned int i;
	
	// Allocate LP stack
	#ifdef ENABLE_ULT
	LPS[lp]->stack = get_ult_stack(lp, LP_STACK_SIZE);
	#endif
	
	
	// Set the initial checkpointing period for this LP.
	// If the checkpointing period is fixed, this will not change during the
	// execution. Otherwise, new calls to this function will (locally) update
	// this.
	set_checkpoint_period(lp, rootsim_config.ckpt_period);


	// Initially, every LP is ready
	LPS[lp]->state = LP_STATE_READY;

	// There is no current state layout at the beginning
	LPS[lp]->current_base_pointer = NULL;

	// Initialize the queues
	LPS[lp]->queue_in = new_list(lp, msg_t);
	LPS[lp]->queue_out = new_list(lp, msg_hdr_t);
	LPS[lp]->queue_states = new_list(lp, state_t);
	LPS[lp]->bottom_halves = new_list(lp, msg_t);
	LPS[lp]->rendezvous_queue = new_list(lp, msg_t);

	// Initialize the LP lock
	spinlock_init(&LPS[lp]->lock);

	// No event has been processed so far
	LPS[lp]->bound = NULL;

	LPS[lp]->outgoing_buffer.min_in_transit = rsalloc(sizeof(simtime_t) * n_cores);
	for(i = 0; i < n_cores; i++) {
		LPS[lp]->outgoing_buffer.min_in_transit[i] = INFTY;
	}

	#ifdef HAVE_CROSS_STATE
	// No read/write dependencies open so far for the LP. The current lp is always opened
	LPS[lp]->ECS_index = 0;
	LPS[lp]->ECS_synch_table[0] = lp;
	#endif

	// Create user thread
	#ifdef ENABLE_ULT
	context_create(&LPS[lp]->context, LP_main_loop, NULL, LPS[lp]->stack, LP_STACK_SIZE);
	#endif
}


void initialize_worker_thread(void) {
	register unsigned int t;

	// Divide LPs among worker threads, for the first time here
	rebind_LPs();
	if(master_thread() && master_kernel()) {
		printf("Initializing LPs... ");
		fflush(stdout);
	}

	// Initialize the LP control block for each locally hosted LP
	// and schedule the special INIT event
	for (t = 0; t < n_prc_per_thread; t++) {

		// Create user level thread for the current LP and initialize LP control block
		initialize_LP(LPS_bound[t]->lid);
		
		// Schedule an INIT event to the newly instantiated LP
		msg_t init_event = {
			sender: LidToGid(LPS_bound[t]->lid),
			receiver: LidToGid(LPS_bound[t]->lid),
			type: INIT,
			timestamp: 0.0,
			send_time: 0.0,
			mark: generate_mark(LidToGid(LPS_bound[t]->lid)),
			size: model_parameters.size,
			message_kind: positive,
		};

		
		// Copy the relevant string pointers to the INIT event payload
		if(model_parameters.size > 0) {
			memcpy(init_event.event_content, model_parameters.arguments, model_parameters.size * sizeof(char *));
		}

		(void)list_insert_head(LPS_bound[t]->lid, LPS_bound[t]->queue_in, &init_event);
		LPS_bound[t]->state_log_forced = true;
	}
	// Worker Threads synchronization barrier: they all should start working together
	thread_barrier(&all_thread_barrier);

	if(master_thread() && master_kernel())
		printf("done\n");

	register unsigned int i;
	for(i = 0; i < n_prc_per_thread; i++) {
		schedule();
	}

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
void activate_LP(unsigned int lp, simtime_t lvt, void *evt, void *state) {

	// Notify the LP main execution loop of the information to be used for actual simulation
	current_lp = lp;
	current_lvt = lvt;
	current_evt = evt;
	current_state = state;

//	#ifdef HAVE_PREEMPTION
//	if(!rootsim_config.disable_preemption)
//		enable_preemption();
//	#endif
	
//	printf(" LP %d current_evt->timestap:%f\n",lp,current_evt->timestamp);
	#ifdef HAVE_CROSS_STATE
	// Activate memory view for the current LP
	lp_alloc_schedule();
	#endif

	//printf("Schedule %d\n",lp);
	//printf("LP[%d] state: %lu\n",lp,LPS[lp]->state);
	if(is_blocked_state(LPS[lp]->state)){
		printf("wait_on_rendezvoud:%llu tid:%d wait_object:%d \n ",LPS[lp]->wait_on_rendezvous,tid,LPS[lp]->wait_on_object);
		rootsim_error(true, "Critical condition: LP[%d] has a wrong state -> %d. Aborting...\n", lp,LPS[lp]->state);
	}

	#ifdef ENABLE_ULT
	context_switch(&kernel_context, &LPS[lp]->context);
	#else
	LP_main_loop(NULL);
	#endif
	
	
//	#ifdef HAVE_PREEMPTION
//        if(!rootsim_config.disable_preemption)
//                disable_preemption();
//        #endif

	#ifdef HAVE_CROSS_STATE
	// Deactivate memory view for the current LP if no conflict has arisen
	if(!is_blocked_state(LPS[lp]->state)) {	
//		printf("Deschedule %d\n",lp);
		lp_alloc_deschedule();
	}
	#endif

	current_lp = IDLE_PROCESS;
	current_lvt = -1.0;
	current_evt = NULL;
	current_state = NULL;
}



bool check_rendevouz_request(unsigned int lid){
	msg_t *temp_mess;	

	if(LPS[lid]->state != LP_STATE_WAIT_FOR_SYNCH)
		return false;
	
	printf("CHECK LP: %d\n",lid);
	
	if(LPS[lid]->bound != NULL && list_next(LPS[lid]->bound) != NULL){
		temp_mess = list_next(LPS[lid]->bound);
		printf("\t \t Randezvous_mark: %llu LPS[%d]->wait_on_rendezvous:%llu\n",temp_mess->rendezvous_mark, lid,LPS[lid]->wait_on_rendezvous);
		return temp_mess->type == RENDEZVOUS_START && LPS[lid]->wait_on_rendezvous > temp_mess->rendezvous_mark;
	}
	
	return false;
	
}





#ifndef HAVE_GLP_SCH_MODULE
/**
* This function checks wihch LP must be activated (if any),
* and in turn activates it. This is used only to support forward execution.
*
* @author Alessandro Pellegrini
*/
void schedule(void) {

	unsigned int lid;
	msg_t *event;
	void *state;
	
	#ifdef HAVE_CROSS_STATE
	bool resume_execution = false;
	#endif

	// Find next LP to be executed, depending on the chosen scheduler
	switch (rootsim_config.scheduler) {

		case SMALLEST_TIMESTAMP_FIRST:
			lid = smallest_timestamp_first();
			break;

		default:
			lid = smallest_timestamp_first();
	}
	
	// No logical process found with events to be processed
	if (lid == IDLE_PROCESS) {
		statistics_post_lp_data(lid, STAT_IDLE_CYCLES, 1.0);
      		return;
    	}


	// If we have to rollback
    	if(LPS[lid]->state == LP_STATE_ROLLBACK) {
		rollback(lid);
				
		LPS[lid]->state = LP_STATE_READY;
		send_outgoing_msgs(lid);

		return;
	}

	if(!is_blocked_state(LPS[lid]->state) && LPS[lid]->state != LP_STATE_READY_FOR_SYNCH){
		event = advance_to_next_event(lid);
	}
	else {
		event = LPS[lid]->bound;
	}
	

	// Sanity check: if we get here, it means that lid is a LP which has
	// at least one event to be executed. If advance_to_next_event() returns
	// NULL, it means that lid has no events to be executed. This is
	// a critical condition and we abort.
	if(event == NULL) {
		rootsim_error(true, "Critical condition: LP %d seems to have events to be processed, but I cannot find them. Aborting...\n", lid);
	}

	if(!process_control_msg(event)) {
		return;
	}

	state = LPS[lid]->current_base_pointer;

	#ifdef HAVE_CROSS_STATE
	// In case we are resuming an interrupted execution, we keep track of this.
	// If at the end of the scheduling the LP is not blocked, we can unblokc all the remote objects
	if(LPS[lid]->state == LP_STATE_READY_FOR_SYNCH) {
		resume_execution = true;
	}
	#endif

	
	// Schedule the LP user-level thread
	LPS[lid]->state = LP_STATE_RUNNING;
	activate_LP(lid, lvt(lid), event, state);

	
	if(!is_blocked_state(LPS[lid]->state)) {
		LPS[lid]->state = LP_STATE_READY;
		send_outgoing_msgs(lid);
	}


	#ifdef HAVE_CROSS_STATE
	if(resume_execution && !is_blocked_state(LPS[lid]->state)) {
		unblock_synchronized_objects(lid);
		
		// This is to avoid domino effect when relying on rendezvous messages
		force_LP_checkpoint(lid);
	}
	#endif
	

	// Log the state, if needed
	LogState(lid);
	

}

#else
/**
* This function checks wihch GLP must be activated (if any),
* and in turn activates it. This is used only to support forward execution.
*
* @author Nazzareno Marziale
* @author Francesco Nobilia
*/
void schedule(void) {
	
	unsigned int lid;
        msg_t *event;
        void *state;
        bool result_log;
        bool need_log_group = true;

        bool resume_execution = false;
        bool have_group = false;
        GLP_state *current_group;


        // Find next LP to be executed, depending on the chosen scheduler
        switch (rootsim_config.scheduler) {

                case SMALLEST_TIMESTAMP_FIRST:
                        lid = smallest_timestamp_first();
                        break;

                default:
                        lid = smallest_timestamp_first();
        }

        // No logical process found with events to be processed
        if (lid == IDLE_PROCESS) {
                statistics_post_lp_data(lid, STAT_IDLE_CYCLES, 1.0);
                return;
        }

        current_group = GLPS[LPS[lid]->current_group];

        if(check_start_group(lid) && verify_time_group(lvt(lid))){
//		printf("[%d] HAVE GROUP state:%d state_LP:%d\n",lid,current_group->state,LPS[lid]->state);
		if(LPS[lid]->state == LP_STATE_WAIT_FOR_GROUP || LPS[lid]->state == LP_STATE_WAIT_FOR_LOG)
			LPS[lid]->state = LP_STATE_READY;
		have_group = true;
	}
        
	// If we have to rollback
        if(LPS[lid]->state == LP_STATE_ROLLBACK) {
		
		if((LPS[lid]->target_rollback != NULL) || !have_group)
                	rollback(lid);
		else{
			LPS[lid]->target_rollback = LPS[lid]->bound;
			PRINT_DEBUG_GLP{
				printf("LP[%d] H_G:%d \n",lid,have_group);
			}
		}
		
		if(have_group){
			//TODO MN da rivedere perchè il contatore va decrementato al termine della silent execution
			LPS[lid]->state = LP_STATE_SILENT_EXEC;
			current_group->counter_rollback--;
			if(current_group->counter_rollback == 0)
				current_group->state = GLP_STATE_SILENT_EXEC;
		}
		else{
			LPS[lid]->state = LP_STATE_READY;
	                send_outgoing_msgs(lid);	
		}

                return;
        }
	
	// This is needed because if the only event of SILENT_EXECUTION it is exactly the bound
	if(LPS[lid]->state == LP_STATE_SILENT_EXEC && LPS[lid]->bound==LPS[lid]->target_rollback){ 
		current_group->counter_silent_ex--;
                if(current_group->counter_silent_ex == 0){
                        current_group->state = GLP_STATE_READY;
		}
		
		PRINT_DEBUG_GLP{
			printf("Complete silent execution with current bound LP[%d]\n",lid);
		}

		LPS[lid]->state = LP_STATE_READY;
               	send_outgoing_msgs(lid);
		return;
	}

//      if( (!is_blocked_state(LPS[lid]->state) && LPS[lid]->state != LP_STATE_READY_FOR_SYNCH) || check_rendevouz_request(lid) ) {
        if(!is_blocked_state(LPS[lid]->state) && LPS[lid]->state != LP_STATE_READY_FOR_SYNCH){
                event = advance_to_next_event(lid);
        }
        else {
                event = LPS[lid]->bound;
	}

        if((current_group->state != GLP_STATE_SILENT_EXEC && check_start_group(lid) && verify_time_group(lvt(lid))) || (event->type == CLOSE_GROUP)){
                PRINT_DEBUG_GLP{
		  printf("UPDATE lvt_group lid:%d sender:%d  msg_type:%d timestamp:%f G_state:%d IGT:%f S-IGT:%d R-IGT:%d Type:%d\n",
			lid,
			event->sender,
			event->type,
			event->timestamp,
			current_group->state,
			current_group->initial_group_time->timestamp,
			current_group->initial_group_time->sender,
			current_group->initial_group_time->receiver,
			current_group->initial_group_time->type);
		}
		
		current_group->lvt = event;
	}
	else{
               PRINT_DEBUG_GLP{
		 printf("NOT UPDATE group-state:%d CSG:%d VTG:%d lid:%d sender:%d state:%d msg_type:%d msg_timestamp:%f\n",
		current_group->state,check_start_group(lid),verify_time_group(lvt(lid)),lid,event->sender,
		LPS[lid]->state,event->type,event->timestamp);
		}
	}


/* TODO CHECK IF IT IS CORRECT REMOVE THIS CODE

	if(!check_start_group(lid) && current_group->lvt==event){
		current_group->counter_synch++;
		printf("Bound_Event_Synch Counter:%d Lid:%d GLP:%d\n",current_group->counter_synch,lid,LPS[lid]->current_group);
		if(current_group->counter_synch == current_group->tot_LP){
			current_group->counter_synch = 0;
			current_group->state = GLP_STATE_READY;
		}
	//	force_LP_checkpoint(lid);
		need_log_group = false;
	}
*/

        // Sanity check: if we get here, it means that lid is a LP which has
        // at least one event to be executed. If advance_to_next_event() returns
        // NULL, it means that lid has no events to be executed. This is
        // a critical condition and we abort.
        if(event == NULL) {
                rootsim_error(true, "Critical condition: LP %d seems to have events to be processed, but I cannot find them. Aborting...\n", lid);
        }

//	printf("[%d] event->type:%lu event->timestamp:%f\n",lid,event->type,event->timestamp);
        if(!process_control_msg(event)) {
                return;
        }

        state = LPS[lid]->current_base_pointer;

        // In case we are resuming an interrupted execution, we keep track of this.
        // If at the end of the scheduling the LP is not blocked, we can unblokc all the remote objects
        if(LPS[lid]->state == LP_STATE_READY_FOR_SYNCH) {
//		printf("[%d] RESUME exec\n",lid);
                resume_execution = true;
        }

        // Schedule the LP user-level thread
        if(LPS[lid]->state != LP_STATE_SILENT_EXEC)
                LPS[lid]->state = LP_STATE_RUNNING;

        activate_LP(lid, lvt(lid), event, state);

	// Check if it is the last event of silent execution. This is needed because if the LP 
	// does not have other messages after bound never go out from SILENT_EXECUTION.
	if(LPS[lid]->state == LP_STATE_SILENT_EXEC){
		if(LPS[lid]->bound==LPS[lid]->target_rollback){
                	current_group->counter_silent_ex--;
			if(current_group->counter_silent_ex == 0)
				current_group->state = GLP_STATE_READY;
			PRINT_DEBUG_GLP{
				printf("Complete silent execution LP[%d]\n",lid);
			}
			LPS[lid]->state = LP_STATE_READY;
			send_outgoing_msgs(lid);
		}
		return;
	}

        if(!is_blocked_state(LPS[lid]->state)) {
                LPS[lid]->state = LP_STATE_READY;
                send_outgoing_msgs(lid);
        }

        if(resume_execution && !is_blocked_state(LPS[lid]->state)) {
                unblock_synchronized_objects(lid);
		
		if(check_start_group(lid) && verify_time_group(lvt(lid)))
	                GLPS[LPS[lid]->current_group]->state = GLP_STATE_READY;

                // This is to avoid domino effect when relying on rendezvous messages
                force_LP_checkpoint(lid);
        }

       /* if(!resume_execution && !verify_time_group(lvt(lid)) && have_group && !is_blocked_state(LPS[lid]->state)){
                force_LP_checkpoint(lid);
		if(current_group->tot_LP>1){
			GLPS[LPS[lid]->current_group]->state = GLP_STATE_WAIT_FOR_LOG;
			GLPS[LPS[lid]->current_group]->counter_log = GLPS[LPS[lid]->current_group]->tot_LP;
			force_checkpoint_group(lid);
			send_outgoing_msgs(lid);
		}
	}*///Created control message CLOSE_GROUP

        // Log the state, if needed
        if(current_group->state != GLP_STATE_SILENT_EXEC)
		result_log = LogState(lid);
	
        if(need_log_group && result_log && check_start_group(lid) && verify_time_group(lvt(lid)) && !is_blocked_state(LPS[lid]->state) && current_group->tot_LP>1){
                GLPS[LPS[lid]->current_group]->state = GLP_STATE_WAIT_FOR_LOG;
                GLPS[LPS[lid]->current_group]->counter_log = GLPS[LPS[lid]->current_group]->tot_LP;
//		printf("FCKG inside scheduler lid:%d lvt:%f type:%lu have_group:%d\n",lid,lvt(lid),LPS[lid]->bound->type,have_group);
		force_checkpoint_group(lid);
		send_outgoing_msgs(lid);
	}
}

#endif

unsigned int get_first_LP(void){
	LP_state *lp;
	unsigned int i;
	
	lp = LPS[0];
	for(i=0; i<n_prc; i++){
		if(lvt(lp->lid) > lvt(LPS[i]->lid))
			lp = LPS[i];
	}
	
	return lp->lid;
}
