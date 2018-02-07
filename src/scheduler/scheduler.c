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
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <datatypes/list.h>
#include <datatypes/msgchannel.h>
#include <communication/communication.h>
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
#include <datatypes/heap.h>

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
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>
#include <queues/xxhash.h>

// static __thread atomic_t notice_count;

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

/// This is a list to keep high-priority messages yet to be processed
static __thread list(msg_t) hi_prio_list;

// Timer per thread used to gather statistics on execution time for 
// controllers and processers in asymmetric executions
__thread timer timer_local_thread;

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
		#ifdef ENABLE_ULT
//		rsfree(LPS(i)->stack);
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
	#ifdef ENABLE_ULT
	context_save(&LPS(current_lp)->default_context);
	#endif

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
		#ifdef ENABLE_ULT
		context_switch(&LPS(current_lp)->context, &kernel_context);
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
void initialize_LP(LID_t lp) {
	unsigned int i;

	// Allocate LP stack
	#ifdef ENABLE_ULT
	LPS(lp)->stack = get_ult_stack(LP_STACK_SIZE);
	#endif


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
	LPS(lp)->retirement_queue = new_list(msg_t);
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
	#ifdef ENABLE_ULT
	context_create(&LPS(lp)->context, LP_main_loop, NULL, LPS(lp)->stack, LP_STACK_SIZE);
	#endif
}


void initialize_processing_thread(void) {
	communication_init_thread();
	hi_prio_list = new_list(msg_t);
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
		pack_msg(&init_event, gid, gid, INIT, 0.0, 0.0, model_parameters.size, model_parameters.arguments);
	        init_event->mark = generate_mark(lid);

		list_insert_head(LPS(lid)->queue_in, init_event);
		LPS(lid)->state_log_forced = true;

		return 0; // continue to next element
	}
	LPS_bound_foreach(__helper1, NULL);

	// Worker Threads synchronization barrier: they all should start working together
	if(rootsim_config.num_controllers == 0) {
		thread_barrier(&all_thread_barrier);
	} else {
		thread_barrier(&controller_barrier);
	}

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
	if(rootsim_config.num_controllers == 0) {
		thread_barrier(&all_thread_barrier);
	} else {
		thread_barrier(&controller_barrier);
	}

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

//	if(is_blocked_state(LPS(lp)->state)){
//		rootsim_error(true, "Critical condition: LP %d has a wrong state -> %d. Aborting...\n", gid_to_int(LidToGid(lp)), LPS(lp)->state);
//	}

	#ifdef ENABLE_ULT
	context_switch(&kernel_context, &LPS(lp)->context);
	#else
	LP_main_loop(NULL);
	#endif


//	#ifdef HAVE_PREEMPTION
//        if(!rootsim_config.disable_preemption)
//                disable_preemption();
//        #endif

	#ifdef HAVE_CROSS_STATE
	// Deactivate memory view for the current LP if no conflict has arisen
	if(!is_blocked_state(LPS(lp)->state)) {
//		printf("Deschedule %d\n",lp);
		lp_alloc_deschedule();
	}L
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
 * This is the core processing routine of PTs
 */
void asym_process(void) {
	msg_t *msg;
	msg_t *msg_hi;
	msg_t *hi_prio_msg;
	LID_t lid;


	timer_start(timer_local_thread);

	// We initially check for high priority msgs. If one is present,
	// we process it and then return. In this way, the next call to
	// asym_process() will again check for high priority events, making
	// them really high priority.
	msg = pt_get_hi_prio_msg();
	if(msg != NULL) {
		list_insert_tail(hi_prio_list, msg);
		// Never change this return to anything else: we call asym_process()
		// within asym_process() to forcely match a high priority queue when
		// there could be a priority inversion between hi and lo prio ports.
		return;
	}

	// No high priority message. Get an event to process.
	msg = pt_get_lo_prio_msg();

	// My queue might be empty...
	if(msg == NULL){
		//printf("Empty queue for PT %d\n", tid);
		return;
	}

	// Check if we picked a stale message
	hi_prio_msg = list_head(hi_prio_list);
	while(hi_prio_msg != NULL) {
		if(gid_equals(msg->receiver, hi_prio_msg->receiver) && !is_control_msg(msg->type) && msg->timestamp > hi_prio_msg->timestamp) {
			// TODO: switch to bool
			if(hi_prio_msg->event_content[0] == 0) {
				hi_prio_msg->event_content[0] = 1;

//				printf("Sending a ROLLBACK_ACK for LP %d at time %f\n", msg->receiver.id, msg->timestamp);
				// atomic_sub(&notice_count, 1);

				// Send back an ack to start processing the actual rollback operation
				pack_msg(&msg, msg->receiver, msg->receiver, ASYM_ROLLBACK_ACK, msg->timestamp, msg->timestamp, 0, NULL);
				msg->message_kind = control;
				pt_put_out_msg(msg);
			}
			return;
		}
		hi_prio_msg = list_next(hi_prio_msg);
	}

	// Match a ROLLBACK_NOTICE with a ROLLBACK_BUBBLE and remove it from the
	// local hi_priority_list.
	if(is_control_msg(msg->type) && msg->type == ASYM_ROLLBACK_BUBBLE) {
//		asym_process();

           ariprova:
		hi_prio_msg = list_head(hi_prio_list);
		while(hi_prio_msg != NULL) {
			if(msg->mark == hi_prio_msg->mark) {
//				msg_release(msg);

				// TODO: switch to bool
				if(hi_prio_msg->event_content[0] == 0) {
//g					printf("Sending a ROLLBACK_ACK (2)  for LP %d at time %f\n", msg->receiver.id, msg->timestamp);
					// atomic_sub(&notice_count, 1);

					// Send back an ack to start processing the actual rollback operation
					pack_msg(&msg, msg->receiver, msg->receiver, ASYM_ROLLBACK_ACK, msg->timestamp, msg->timestamp, 0, NULL);
					msg->message_kind = control;
					pt_put_out_msg(msg);
				}

				list_delete_by_content(hi_prio_list, hi_prio_msg);
//				msg_release(hi_prio_msg);
				return;
			}
			hi_prio_msg = list_next(hi_prio_msg);
		}

		msg_hi = pt_get_hi_prio_msg();
		while(msg_hi != NULL) {
			list_insert_tail(hi_prio_list, msg_hi);
			msg_hi = pt_get_hi_prio_msg();
		}
		goto ariprova;

		fprintf(stderr, "Cannot match a bubble!\n");
		abort();
	}

	lid = GidToLid(msg->receiver);

	// TODO: find a way to set the LP to RUNNING without incurring in a race condition with the CT

	// Process this event
	activate_LP(lid, msg->timestamp, msg, LPS(lid)->current_base_pointer);
	msg->unprocessed = false;

	// Send back to the controller the (possibly) generated events
	asym_send_outgoing_msgs(lid);

	// Log the state, if needed
	// TODO: we make the PT take a checkpoint. The optimality would be to let the CT
	// take a checkpoint, but we need some sort of synchronization which is out of the
	// scope of the current development phase here.
	LogState(lid);
	
	//printf("asym_process for PT %d completed in %d millisecond\n", tid, timer_value_milli(timer_local_thread));

}


/**
* This is the asymmetric scheduler. This function extracts a bunch of events
* to be processed by LPs bound to a controller and sends them to processing
* threads for later execution. Rollbacks are executed by the controller, and
* are triggered here in a lazy fashion.
*
* @author Alessandro Pellegrini
*/
void asym_schedule(void) {
	LID_t lid, curr_lid;
	msg_t *event, *curr_event;
	msg_t *rollback_control;
	unsigned int port_events_to_fill[n_cores];
	unsigned int port_current_size[n_cores];
	unsigned int i, j, thread_id_mask;
	unsigned int events_to_fill = 0;
	char first_encountered = 0;
	unsigned long long mark;
	int toAdd, delta_utilization;
	unsigned int sent_events = 0; 
	unsigned int sent_notice = 0;
 	heap_t events_heap; 
	events_heap.size = 0;
	events_heap.len = 0; 

	/* Testing heap 
	heap_t heap_test; 
	heap_test.size = 0;
	heap_test.len = 0;
	
	if(lps_blocks[1]->bound != NULL && lps_blocks[2]->bound != NULL){
		// DEBUG: testing heap implementantion
		printf("Adding to heap bound of LP 1 and LP2\n");
		heap_push(&heap_test, lps_blocks[1]->bound->timestamp, lps_blocks[1]->bound);
		heap_push(&heap_test, lps_blocks[2]->bound->timestamp, lps_blocks[2]->bound);
		printf("Retrieving LPS in order of timestamp bound\n");
		msg_t *first_dequeue = heap_pop(&heap_test);
		msg_t *second_dequeue = heap_pop(&heap_test);
		printf("First dequeue: %lf, second dequeue: %lf\n", first_dequeue->timestamp, second_dequeue->timestamp);
	}
	*/

//	printf("NOTICE COUNT: %d\n", atomic_read(&notice_count));

	timer_start(timer_local_thread);

	// Compute utilization rate of the input ports since the last call to asym_schedule
	// and resize the ports if necessary
	for(i = 0; i < Threads[tid]->num_PTs; i++){
		Thread_State* pt = Threads[tid]->PTs[i];
		int port_size = pt->port_batch_size; 
		port_current_size[pt->tid] = get_port_current_size(pt->input_port[PORT_PRIO_LO]);
		delta_utilization = port_size - port_current_size[pt->tid];
		if(delta_utilization < 0){
			delta_utilization = 0;
		}
		double utilization_rate = (double) delta_utilization / (double) port_size;
	
		// DEBUG
		//printf("Port current size: %u, port size %u, delta_utilization %d\n", port_current_size[pt->tid], port_size, delta_utilization); 
		//printf("Input port size of PT %u: %d (utilization factor: %f)\n", pt->tid, port_current_size[pt->tid], utilization_rate);
		// DEBUG

		// If utilization rate is too high, the size of the port should be increased
		if(utilization_rate > UPPER_PORT_THRESHOLD){
			if(pt->port_batch_size <= (MAX_PORT_SIZE - BATCH_STEP)){
				pt->port_batch_size+=BATCH_STEP;
			}else if(pt->port_batch_size < MAX_PORT_SIZE){
				pt->port_batch_size++;
			}
			//printf("Increased port size of PT %u to %d\n", pt->tid, pt->port_batch_size);
		}
		// If utilization rate is too low, the size of the port should be decreased
		else if (utilization_rate < LOWER_PORT_THRESHOLD){
			if(pt->port_batch_size > BATCH_STEP){
				pt->port_batch_size-=BATCH_STEP;
			}else if(pt->port_batch_size > 1){
				pt->port_batch_size--;
			}
			//printf("Reduced port size of PT %u to %d\n", pt->tid, pt->port_batch_size);
		}
	}
	
	//printf("asym_schedule: port resize completed for controller %d at millisecond %d\n", tid, timer_value_milli(timer_local_thread));


	// Compute the total number of events necessary to fill all
	// the input ports, considering the current batch value 
	// and the current number of events yet to be processed in 
	// each port
	for(i = 0; i < Threads[tid]->num_PTs; i++){
		Thread_State* pt = Threads[tid]->PTs[i];
		toAdd = pt->port_batch_size - port_current_size[pt->tid];
		// Might be negative as we reduced the port size, and it might 
		// have been already full
		if(toAdd > 0){	
			port_events_to_fill[pt->tid] = toAdd;
			events_to_fill += toAdd;
			//printf("Adding toAdd = %d to events_to_fill\n", toAdd);
		} else {
			port_events_to_fill[pt->tid] = 0;
		}
	}

	//printf("asym_schedule: events_to_fill computed for controller %d at millisecond %d\n", tid, timer_value_milli(timer_local_thread));


	// Create a copy of lps_bound_blocks in asym_lps_mask which will
	// be modified during scheduling in order to jump LPs bound to PT
	// for whom the input port is already filled
	memcpy(asym_lps_mask, lps_bound_blocks, sizeof(LP_State *) * n_prc_per_thread);
	for(i = 0; i < n_prc_per_thread; i++) {
		Thread_State *pt = Threads[asym_lps_mask[i]->processing_thread];
		if(port_current_size[pt->tid] >= pt->port_batch_size) {
			asym_lps_mask[i] = NULL;
		}
	}
	//printf("asym_schedule: asym_lps_mask computed for controller %d at millisecond %d\n", tid, timer_value_milli(timer_local_thread));

	// Put up to MAX_LP_EVENTS_PER_BATCH events for each LP in the priority
	// queue events_heap
	if(rootsim_config.scheduler == BATCH_LOWEST_TIMESTAMP){
		for(i = 0; i < n_prc_per_thread; i++){
			if(asym_lps_mask[i] != NULL && !is_blocked_state(asym_lps_mask[i]->state)){
				if(asym_lps_mask[i]->bound == NULL && !list_empty(asym_lps_mask[i]->queue_in)){
					curr_event = list_head(asym_lps_mask[i]->queue_in);
				}
				else{
					curr_event = list_next(asym_lps_mask[i]->bound);
				}

				j = 0;
				while(curr_event != NULL && j < MAX_LP_EVENTS_PER_BATCH){
					heap_push(&events_heap, curr_event->timestamp, curr_event);
					j++;
					curr_event = curr_event->next;
				}
			}
		}
	}

	// Set to 0 all the curr_scheduled_events array
	bzero(Threads[tid]->curr_scheduled_events, sizeof(int)*n_prc);

	//printf("events_to_fill=%d\n", events_to_fill);

	for(i = 0; i < events_to_fill; i++) {

//		printf("SCHED: mask: ");
//		int jk;
//		for(jk = 0; jk < n_prc_per_thread; jk++) {
//			printf("%p, ", asym_lps_mask[jk]);
//		}
//		puts("");

		#ifdef HAVE_CROSS_STATE
		bool resume_execution = false;
		#endif


		// Find next LP to be executed, depending on the chosen scheduler
		switch (rootsim_config.scheduler) {

			case SMALLEST_TIMESTAMP_FIRST:
				lid = asym_smallest_timestamp_first();
				break;

			case BATCH_LOWEST_TIMESTAMP:
				curr_event = heap_pop(&events_heap);
				int found = 0;
			       	lid = idle_process;	
				while(curr_event != NULL && !found){
					curr_lid = GidToLid(curr_event->sender);
					if(port_events_to_fill[LPS(curr_lid)->processing_thread] > 0 && LPS(curr_lid)->state != LP_STATE_WAIT_FOR_ROLLBACK_ACK){
						found = 1; 
						lid = curr_lid;
					}
					else{
						curr_event = heap_pop(&events_heap);
						lid = idle_process;
					}
				}
				break;
	
			default:
				lid = asym_smallest_timestamp_first();
		}

		// No logical process found with events to be processed
		if (lid_equals(lid, idle_process)) {
			statistics_post_lp_data(lid, STAT_IDLE_CYCLES, 1.0);
			continue;
		}

		// If we have to rollback
		if(LPS(lid)->state == LP_STATE_ROLLBACK) {
			// Get a mark for the current batch of control messages
			mark = generate_mark(lid);

			//printf("Sending a ROLLBACK_NOTICE for LP %d at time %f\n", lid.id, lvt(lid));
			sent_notice++;

			// atomic_add(&notice_count, 1);

			// Send rollback notice in the high priority port
			pack_msg(&rollback_control, LidToGid(lid), LidToGid(lid), ASYM_ROLLBACK_NOTICE, lvt(lid), lvt(lid), sizeof(char), &first_encountered);
			rollback_control->message_kind = control;
			rollback_control->mark = mark;
			pt_put_hi_prio_msg(LPS(lid)->processing_thread, rollback_control);

			// Set the LP to a blocked state
			LPS(lid)->state = LP_STATE_WAIT_FOR_ROLLBACK_ACK;

			// Notify the PT in charge of managing this LP that the rollback is complete and
			// events to the LP should not be discarded anymore
//			printf("Sending a ROLLBACK_BUBBLE for LP %d at time %f\n", lid.id, lvt(lid));

			pack_msg(&rollback_control, LidToGid(lid), LidToGid(lid), ASYM_ROLLBACK_BUBBLE, lvt(lid), lvt(lid), 0, NULL);
			rollback_control->message_kind = control;
			rollback_control->mark = mark;
			pt_put_lo_prio_msg(LPS(lid)->processing_thread, rollback_control);

			continue;
		}

		if(LPS(lid)->state == LP_STATE_ROLLBACK_ALLOWED) {
			// Rollback the LP and send antimessages
			LPS(lid)->state = LP_STATE_ROLLBACK;
			rollback(lid);
			LPS(lid)->state = LP_STATE_READY;
			//send_outgoing_msgs(lid);

			// Prune the retirement queue for this LP
			while(true) {
				event = list_head(LPS(lid)->retirement_queue);
				if(event == NULL) {
					break;
				}
				list_delete_by_content(LPS(lid)->retirement_queue, event);
				msg_release(event);
			}

			continue;
		}

		if(!is_blocked_state(LPS(lid)->state) && LPS(lid)->state != LP_STATE_READY_FOR_SYNCH){
			event = advance_to_next_event(lid);
		} else {
			event = LPS(lid)->bound;
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

		#ifdef HAVE_CROSS_STATE
		// TODO: we should change this by managing the state internally to activate_LP, as this
		// would uniform the code across symmetric/asymmetric implementations.
		// In case we are resuming an interrupted execution, we keep track of this.
		// If at the end of the scheduling the LP is not blocked, we can unblock all the remote objects
		if(is_blocked_state(LPS(lid)->state) || LPS(lid)->state == LP_STATE_READY_FOR_SYNCH) {
			resume_execution = true;
		}
		#endif

		thread_id_mask = LPS(lid)->processing_thread;

		// Put the event in the low prio queue of the associated PT
		event->unprocessed = true;
		pt_put_lo_prio_msg(thread_id_mask, event);
		sent_events++;

		// Modify port_events_to_fill to reflect last message sent
		port_events_to_fill[thread_id_mask]--; 

		unsigned int lp_id = lid_to_int(lid);
		
		if(rootsim_config.scheduler == SMALLEST_TIMESTAMP_FIRST){
			// Increase curr_scheduled_events, and set pointer to 
			// respective LP to NULL if exceeded MAX_LP_EVENTS_PER_BATCH
			Threads[tid]->curr_scheduled_events[lp_id] = Threads[tid]->curr_scheduled_events[lp_id]+1;
			//printf("curr_scheduled_events[%u] = %d\n", lp_id, Threads[tid]->curr_scheduled_events[lp_id]);
			if(Threads[tid]->curr_scheduled_events[lp_id] >= MAX_LP_EVENTS_PER_BATCH){
				for(i=0; i<n_prc_per_thread; i++){
					if(asym_lps_mask[i] != NULL && lid_equals(asym_lps_mask[i]->lid,lid)){
						asym_lps_mask[i] = NULL;
						//printf("Setting to NULL pointer to LP %d\n", lp_id);
						break;
					}
				}
			}

			//printf("asym_schedule: event sent for controller %d at millisecond %d\n", tid, timer_value_milli(timer_local_thread));

			// If one port becomes full, should set all pointers to LP
			// mapped to the PT of the respective port to NULL 
			// printf("thread_id_mask: %u\n", thread_id_mask);
			if(port_events_to_fill[thread_id_mask] == 0){
				for(i = 0; i<n_prc_per_thread; i++){
					if(asym_lps_mask[i] != NULL && asym_lps_mask[i]->processing_thread == thread_id_mask)
						asym_lps_mask[i] = NULL;
				}
			}

		}
		
		#ifdef HAVE_CROSS_STATE
		if(resume_execution && !is_blocked_state(LPS(lid)->state)) {
			printf("ECS event is finished mark %llu !!!\n", LPS(lid)->wait_on_rendezvous);
			fflush(stdout);
			unblock_synchronized_objects(lid);

			// This is to avoid domino effect when relying on rendezvous messages
			// TODO: I'm not quite sure if with asynchronous PTs' this way to code ECS-related checkpoints still holds
			force_LP_checkpoint(lid);
		}
		#endif
	}
	//printf("Sent events: %u\n", sent_events);
	//printf("Sent rollback notice: %u\n", sent_notice);
	//printf("asym_schedule for controller %d completed in %d milliseconds\n", tid, timer_value_milli(timer_local_thread));
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

		case SMALLEST_TIMESTAMP_FIRST:
			lid = smallest_timestamp_first();
			break;

		default:
			lid = smallest_timestamp_first();
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
	if(event == NULL) {
		rootsim_error(true, "Critical condition: LP %d seems to have events to be processed, but I cannot find them. Aborting...\n", lid);
	}

	if(!process_control_msg(event)) {
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
		printf("ECS event is finished at LP %d mark %llu !!!\n", lid_to_int(lid), LPS(lid)->wait_on_rendezvous);
		fflush(stdout);
		unblock_synchronized_objects(lid);

		// This is to avoid domino effect when relying on rendezvous messages
		force_LP_checkpoint(lid);
	}
#endif

	// Log the state, if needed
	LogState(lid);
}

