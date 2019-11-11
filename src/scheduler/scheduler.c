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
#include <score/score.h>


/// This is used to keep track of how many LPs were bound to the current KLT
__thread unsigned int n_lp_per_thread;

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

// Timer per thread used to gather statistics on execution time for
// controllers and processers in asymmetric executions
//static __thread timer timer_local_thread;

// Pointer to an array of longs which are used as an accumulator of time
// spent idle in asym_schedule or asym_process
long *total_idle_microseconds;
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
* This is a LP main loop. It s the embodiment of the userspace thread implementing the logic of the LP.
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
void LP_main_loop(void *args) {
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
			hash1 = XXH64(current_evt->event_content, current_evt->size, current->gid.to_int);
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
				      current_evt->timestamp,
				      current_evt->type,
				      current_evt->event_content,
				      current_evt->size,
				      current->current_base_pointer);
			switch_to_platform_mode();
		}
		int delta_event_timer = timer_value_micro(event_timer);

#ifdef EXTRA_CHECKS
		if (current->bound->size > 0) {
			hash2 =
			    XXH64(current_evt->event_content, current_evt->size, current->gid.to_int);
		}

		if (hash1 != hash2) {
			rootsim_error(true,
				      "Error, LP %src/scheduler/.deps/scheduler.Tpod has modified the payload of event %d during its processing. Aborting...\n",
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

void initialize_worker_thread(void) {
    msg_t *init_event;

	// Divide LPs among worker threads, for the first time here
	rebind_LPs();
	if (master_thread() && master_kernel()) {
		printf("Initializing LPs... ");
		fflush(stdout);
	}

    if(rootsim_config.num_controllers == 0) {
        thread_barrier(&all_thread_barrier);
    } else {
        thread_barrier(&controller_barrier);
    }

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

    // Worker Threads synchronization barrier: they all should start working together
	if(rootsim_config.num_controllers == 0) {
		thread_barrier(&all_thread_barrier);
	} else {
		thread_barrier(&controller_barrier);
	}

    foreach_bound_lp(lp) {
        schedule_on_init(lp);
    }

	if(rootsim_config.num_controllers == 0) {
		thread_barrier(&all_thread_barrier);
	} else {
		thread_barrier(&controller_barrier);
	}


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
* @param next_LP A pointer to the lp_struct of the LP which has to be activated
* @param next_evt A pointer to the event to be processed by the LP
*/
void activate_LP(struct lp_struct *next_LP, msg_t *next_evt) {

	// Notify the LP main execution loop of the information to be used for actual simulation
	/*if(next_evt->timestamp<get_last_gvt()){
	    dump_msg_content(next_evt);
	    printf("%d\n",get_last_gvt);
	}*/
	current = next_LP;
	current_evt = next_evt;

     //#ifdef HAVE_PREEMPTION
    //if(!rootsim_config.disable_preemption)
    //  enable_preemption();
    //#endif

    #ifdef HAVE_CROSS_STATE
	// Activate memory view for the current LP
	lp_alloc_schedule();
    #endif
    context_switch(&kernel_context, &next_LP->context);

    //#ifdef HAVE_PREEMPTION
    //if(!rootsim_config.disable_preemption)
    //        disable_preemption();
    //#endif

    #ifdef HAVE_CROSS_STATE
	// Deactivate memory view for the current LP if no conflict has arisen
	if (!is_blocked_state(next_LP->state)) {
//              printf("Deschedule %d\n",lp);
		lp_alloc_deschedule();
	}
    #endif
	if(rootsim_config.num_controllers > 0){
	    next_LP->next_last_processed = next_evt;
	}
	else
	    next_LP->last_processed = next_evt;


    next_evt->unprocessed = false;      ///CONTROLLARE

    current = NULL;
    current_evt = NULL;
}


bool check_rendevouz_request(struct lp_struct *lp) {
	msg_t *temp_mess;

	if (lp->state != LP_STATE_WAIT_FOR_SYNCH)
		return false;

	if (lp->bound != NULL && list_next(lp->bound) != NULL) {
		temp_mess = list_next(lp->bound);
		return temp_mess->type == RENDEZVOUS_START && lp->wait_on_rendezvous > temp_mess->rendezvous_mark;
	}

	return false;
}


void asym_process_one_event(msg_t *msg) {
    struct lp_struct *LP;
    LP = find_lp_by_gid(msg->receiver);

    //spin_lock(&LP->bound_lock); //Process this event
    activate_LP(LP, msg);
    //spin_unlock(&LP->bound_lock);

    asym_send_outgoing_msgs(LP); //Send back to the controller the (possibly) generated events
    LogState(LP);
}


void find_a_match(msg_t *lo_prio_msg) {

    msg_t *hi_prio_msg;
    msg_t *rb_ack;
    int type;

    while(1) {
        hi_prio_msg = pt_get_hi_prio_msg();
        validate_msg(hi_prio_msg);

            type = hi_prio_msg->type;
            if (is_control_msg(type)){
                if(type != ASYM_ROLLBACK_NOTICE){
                    fprintf(stderr, "\tERROR: Type %d CONTROL message SHOULDN'T stay in the HI_PRIO queue!\n",
                            hi_prio_msg->type);
                    dump_msg_content(hi_prio_msg);
                    fflush(stdout);
                    abort();
                }
                else {   //IT IS A NOTICE

                    if(lo_prio_msg->receiver.to_int != hi_prio_msg->receiver.to_int){   //DIFFERENT RECEIVERS
                        printf("\tWARNING: lo/hi prio messages have DIFFERENT receivers\n");
                        dump_msg_content(lo_prio_msg);
                        dump_msg_content(hi_prio_msg);
                        abort();
                    }
                    if(lo_prio_msg->mark != hi_prio_msg->mark) {    //SAME RECEIVERS BUT DIFFERENT MARKS
                        fprintf(stderr, "\tWARNING: same receiver but BUBBLE/NOTICE priority INVERSION\n");
                        dump_msg_content(lo_prio_msg);
                        dump_msg_content(hi_prio_msg);
                        abort();
                    }
                    else {      //BUBBLE MATCHED
                        pack_msg(&rb_ack, lo_prio_msg->receiver, lo_prio_msg->receiver, ASYM_ROLLBACK_ACK,
                                 lo_prio_msg->timestamp, lo_prio_msg->timestamp, 0, NULL);
                        rb_ack->message_kind = control;
                        rb_ack->mark = hi_prio_msg->mark;
                        pt_put_out_msg(rb_ack);
                        msg_release(lo_prio_msg);   //FREE THE BUBBLE
                        msg_release(hi_prio_msg);   //FREE THE NOTICE
                        return;
                    }
                }
            }
            else {  //NOT A CONTROL MESSAGE

                fprintf(stderr, "\tNON-CONTROL msg in hi_prio channel\n");
                dump_msg_content(lo_prio_msg);
                dump_msg_content(hi_prio_msg);
                fflush(stdout);
                abort();
        }
    }
}

/**
* This is a new and simplified version of the asymmetric scheduler. This function extracts a bunch of events
* to be processed by LPs bound to a controller and sends them to processing
* threads for later execution. Rollbacks are executed by the controller, and
* are triggered here in a lazy fashion.
*/
void asym_process(void) {

    msg_t *lo_prio_msg;
    msg_t *hi_prio_msg;
    msg_t *rb_ack;
    int type;

    while((hi_prio_msg = pt_get_hi_prio_msg()) !=  NULL) {
        validate_msg(hi_prio_msg);

        do {
            while ((lo_prio_msg = pt_get_lo_prio_msg()) == NULL);
            validate_msg(lo_prio_msg);

            type = lo_prio_msg->type;
            if (is_control_msg(type)){
                if(type != ASYM_ROLLBACK_BUBBLE){
                    fprintf(stderr, "\tERROR: Type %d CONTROL message SHOULDN'T stay in the LO_PRIO queue!\n",
                            lo_prio_msg->type);
                    dump_msg_content(lo_prio_msg);
                    fflush(stdout);
                    abort();
                }
                else{  //IT IS A BUBBLE
                    if(lo_prio_msg->receiver.to_int != hi_prio_msg->receiver.to_int){   //DIFFERENT RECEIVERS
                        printf("\tERROR: lo/hi prio messages have DIFFERENT receivers\n");
                        dump_msg_content(lo_prio_msg);
                        dump_msg_content(hi_prio_msg);
                        abort();
                    }
                    if(lo_prio_msg->mark != hi_prio_msg->mark) {    //SAME RECEIVERS BUT DIFFERENT MARKS
                        fprintf(stderr, "\tWARNING: same receiver but BUBBLE/NOTICE priority INVERSION\n");
                        dump_msg_content(lo_prio_msg);
                        dump_msg_content(hi_prio_msg);
                        fflush(stdout);
                        abort();
                    }
                    else {  //BUBBLE-NOTICE MATCHED!
                        pack_msg(&rb_ack, lo_prio_msg->receiver, lo_prio_msg->receiver, ASYM_ROLLBACK_ACK,
                                lo_prio_msg->timestamp, lo_prio_msg->timestamp, 0, NULL);
                        rb_ack->message_kind = control;
                        rb_ack->mark = hi_prio_msg->mark;
                        debug("Message ROLLBACK ACK SENT -> LP%u, ts %f\n", lo_prio_msg->receiver.to_int,
                                lo_prio_msg->timestamp);
                        pt_put_out_msg(rb_ack);
                        msg_release(lo_prio_msg);   //FREE THE BUBBLE
                        msg_release(hi_prio_msg);   //FREE THE NOTICE
                        return;
                    }
                }
            }
            else {  //NOT A CONTROL MESSAGE
                if (lo_prio_msg->receiver.to_int != hi_prio_msg->receiver.to_int || lo_prio_msg->timestamp < hi_prio_msg->timestamp ) {
                        asym_process_one_event(lo_prio_msg);
                        continue;
                }
                else {  ///TO BE DISCARDED (ts>bubble_ts) >>>CONTROLLARE<<<
                    lo_prio_msg->unprocessed = false;
                }
            }
        } while (true);
    }

    lo_prio_msg = pt_get_lo_prio_msg();

    if(lo_prio_msg == NULL)
         return;

    type = lo_prio_msg->type;

    if(is_control_msg(lo_prio_msg->type)){

        if(type == ASYM_ROLLBACK_BUBBLE){
            find_a_match(lo_prio_msg);
            return;
        }

        else if (type == ASYM_ROLLBACK_NOTICE) {
            printf("\tERROR: I've found a NOTICE in a lo_prio channel\n");
            dump_msg_content((lo_prio_msg));
            abort();
        }
    }
    asym_process_one_event(lo_prio_msg);
}


void asym_schedule(void) {
    unsigned int i;
    int EventsToAdd = 0;
    int delta_utilization = 0;
    int sent_events = 0;
    unsigned int port_current_size[n_cores];
    unsigned int events_to_fill_PT_port[n_cores];
    unsigned int tot_events_to_schedule = 0;
    unsigned int n_PTs = Threads[tid]->num_PTs;  //PTs assigned to THIS CT
    unsigned long long mark;
    struct lp_struct *chosen_LP;
    msg_t *chosen_EVT;
    msg_t *rb_management;
    msg_t *evt_to_prune, *evt_to_prune_next;

    //timer_start(timer_local_thread);

    for (i = 0; i < n_PTs; i++) {
        Thread_State *PT = Threads[tid]->PTs[i];
        port_current_size[PT->tid] = get_port_current_size(PT->input_port[PORT_PRIO_LO]);
        delta_utilization = PT->port_batch_size - port_current_size[PT->tid];
        if (delta_utilization < 0) { delta_utilization = 0; }
        double utilization_rate = 1.0 - ((double) delta_utilization / (double) PT->port_batch_size);

        ///The bigger the utilization rate is, the smaller amount of free space the port can offer
        //   printf("port_current_size[PT->tid]: %d, utilization_rate: %f, port_batch_size: %d \n",port_current_size[PT->tid], utilization_rate,PT->port_batch_size);
        if (utilization_rate > UPPER_PORT_THRESHOLD) {
            modify_score(UPPER_THRESHOLD_MODIFIER);
            if (PT->port_batch_size <= (MAX_PORT_SIZE - BATCH_STEP)) {
                PT->port_batch_size += BATCH_STEP;
            } else if (PT->port_batch_size < MAX_PORT_SIZE) {
                PT->port_batch_size++;
            }
        } else if (utilization_rate < LOWER_PORT_THRESHOLD) {
            modify_score(LOWER_THRESHOLD_MODIFIER);
            if (PT->port_batch_size > BATCH_STEP) {
                PT->port_batch_size -= BATCH_STEP;
            } else if (PT->port_batch_size > 1) {
                PT->port_batch_size--;
            }
        }

        EventsToAdd = PT->port_batch_size - port_current_size[PT->tid];
        if (EventsToAdd > 0) {
            events_to_fill_PT_port[PT->tid] = EventsToAdd;
            tot_events_to_schedule += EventsToAdd;
        } else {
            events_to_fill_PT_port[PT->tid] = 0;
        }
    }

    memcpy(asym_lps_mask, lps_bound_blocks, sizeof(struct lp_struct *) * n_lp_per_thread);
    for (i = 0; i < n_lp_per_thread; i++) {
        Thread_State *PT = Threads[asym_lps_mask[i]->processing_thread];  //PT assigned to that lp "i"
        if (port_current_size[PT->tid] >= PT->port_batch_size) {
            asym_lps_mask[i] = NULL;
        }
    }

    // Pointer to an array of chars used by controllers as a counter of the number of events scheduled for
    // each LP during the execution of asym_schedule.
    bzero(Threads[tid]->curr_scheduled_events, sizeof(int) * n_prc);

    for (i = 0; i < tot_events_to_schedule; i++) {
        if (rootsim_config.scheduler == SCHEDULER_STF) {
            chosen_LP = asym_smallest_timestamp_first();
        } else {
            fprintf(stderr, "\tWARNING: asym scheduler supports only the STF scheduler by now\n");
            abort();
        }

        if (unlikely(chosen_LP == NULL)) {
            //  statistics_post_data(NULL, STAT_IDLE_CYCLES, 1.0);
            return;
        }

        if (chosen_LP->state == LP_STATE_ROLLBACK) { // = LP received an out-of-order msg and needs a rollback
            mark = generate_mark(chosen_LP);

            if (chosen_LP->rollback_status == REQUESTED)
                chosen_LP->rollback_status = PROCESSING;

            else {
                printf("\tERROR: Impossible rollback_status\n");
                abort();
            }

            chosen_LP->rollback_mark = mark;

            pack_msg(&rb_management, chosen_LP->gid, chosen_LP->gid, ASYM_ROLLBACK_NOTICE, chosen_LP->bound->timestamp,
                     chosen_LP->bound->timestamp, 0, NULL);// Send rollback notice in the high priority port
            rb_management->message_kind = control;
            rb_management->mark = mark;
            chosen_LP->start = clock();   ///Start the turnaround timer
            pt_put_hi_prio_msg(chosen_LP->processing_thread, rb_management);

            chosen_LP->state = LP_STATE_WAIT_FOR_ROLLBACK_ACK;  //BLOCKED STATE

            pack_msg(&rb_management, chosen_LP->gid, chosen_LP->gid, ASYM_ROLLBACK_BUBBLE, chosen_LP->bound->timestamp,
                     chosen_LP->bound->timestamp, 0, NULL);
            rb_management->message_kind = control;
            rb_management->mark = mark;
            pt_put_lo_prio_msg(chosen_LP->processing_thread, rb_management);

            continue;
        }

        if (chosen_LP->state == LP_STATE_ROLLBACK_ALLOWED) { // = extracted ASYM_ROLLBACK_ACK from PT output queue for chosen_LP
            chosen_LP->state = LP_STATE_ROLLBACK;
            rollback(chosen_LP);
            chosen_LP->state = LP_STATE_READY;

            evt_to_prune = list_head(chosen_LP->retirement_queue);
            while (evt_to_prune != NULL) {
                evt_to_prune_next = list_next(evt_to_prune);
                if (evt_to_prune->unprocessed == false && chosen_LP->last_processed != evt_to_prune) {
                    list_delete_by_content(chosen_LP->retirement_queue, evt_to_prune);
                    msg_release(evt_to_prune);
                }
                evt_to_prune = evt_to_prune_next;
            }
        }

        if (chosen_LP->state != LP_STATE_READY_FOR_SYNCH && !is_blocked_state(chosen_LP->state)) {
            chosen_EVT = advance_to_next_event(chosen_LP);
        } else {
            chosen_EVT = chosen_LP->bound;
        }

        if (unlikely(chosen_EVT == NULL)) {
            rootsim_error(true,"Critical condition: LP %d seems to have events to be processed, but I cannot find them. Aborting...\n", chosen_LP->gid);
        }

        if (unlikely(!to_be_sent_to_LP(chosen_EVT))) {    //NOT a message to be passed to the LP (a control msg)
            return;
        }
        if(are_input_channels_empty(chosen_LP->processing_thread)){
           exponential_moving_avg(1, EMPTY_PT_ID);
        }
        else
           exponential_moving_avg(0,EMPTY_PT_ID);

        chosen_EVT->unprocessed = true;
        pt_put_lo_prio_msg(chosen_LP->processing_thread, chosen_EVT);
        sent_events++;
        events_to_fill_PT_port[chosen_LP->processing_thread]--;
        unsigned int chosen_LP_id = chosen_LP->lid.to_int;

        if (rootsim_config.scheduler == SCHEDULER_STF) {

            Threads[tid]->curr_scheduled_events[chosen_LP_id] = Threads[tid]->curr_scheduled_events[chosen_LP_id] + 1;

            if (Threads[tid]->curr_scheduled_events[chosen_LP_id] >= MAX_LP_EVENTS_PER_BATCH) {
                //FIND THE LP IN THE MASK AND SET IT TO NULL
                for (i = 0; i < n_lp_per_thread; i++) {
                    if (asym_lps_mask[i] != NULL && lid_equals(asym_lps_mask[i]->lid, chosen_LP->lid)) {
                        asym_lps_mask[i] = NULL;
                        break;
                    }
                }
            }

            if (events_to_fill_PT_port[chosen_LP->processing_thread] == 0) {  //NO MORE EMPTY SLOTS FOR THAT PT
                //FIND THE LP IN THE MASK AND SET IT TO NULL
                for (i = 0; i < n_lp_per_thread; i++) {
                    if (asym_lps_mask[i] != NULL && asym_lps_mask[i]->processing_thread == chosen_LP->processing_thread)
                        asym_lps_mask[i] = NULL;
                }
            }
        }
    }

    if (sent_events == 0) {
            //  total_idle_microseconds[tid] += timer_value_micro(timer_local_thread);
        }
}


/**
* This function checks which LP must be activated (if any),
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

	if (unlikely(!to_be_sent_to_LP(event))) {
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
	if(rootsim_config.num_controllers>0)
	    next->last_processed = next->next_last_processed;

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
