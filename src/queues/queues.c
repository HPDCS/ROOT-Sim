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
* @file queues.c
* @brief This module implements the event/message queues subsystem
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <scheduler/process.h>
#include <core/core.h>
#include <arch/atomic.h>
#include <arch/thread.h>
#include <datatypes/list.h>
#include <datatypes/msgchannel.h>
#include <queues/queues.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <scheduler/scheduler.h>
#include <communication/communication.h>
#include <communication/gvt.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>


/**
* This function returns the timestamp of the last executed event
*
* @author Francesco Quaglia
*
* @param lid The Light Process id
* @return The timestamp of the last executed event
*/
simtime_t last_event_timestamp(LID_t lid) {
	return lvt(lid);
}



/**
* This function return the timestamp of the next-to-execute event
*
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @param lid The Logicall Process id
* @return The timestamp of the next-to-execute event
*/
simtime_t next_event_timestamp(LID_t lid) {
	msg_t *evt;

	// The bound can be NULL in the first execution or if it has gone back
	if (LPS(lid)->bound == NULL && !list_empty(LPS(lid)->queue_in)) {
		return list_head(LPS(lid)->queue_in)->timestamp;
	} else {
		evt = list_next(LPS(lid)->bound);
		if(evt != NULL) {
			return evt->timestamp;
		}
	}

	return INFTY;

}


/**
* This function advances the pointer to the last correctly executed event (bound).
* It is called right before the execution of it. This means that after this
* call, before the actual call to ProcessEvent(), bound is pointing to a
* not-yet-executed event. This is the only case where this can happen.
*
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @param lid The Light Process id
* @return The pointer to the event is going to be processed
*/
msg_t *advance_to_next_event(LID_t lid) {

	if (LPS(lid)->bound == NULL) {
		if (!list_empty(LPS(lid)->queue_in)) {
			LPS(lid)->bound = list_head(LPS(lid)->queue_in);
		} else {
			return NULL;
		}
	} else {
		if (list_next(LPS(lid)->bound) != NULL) {
			LPS(lid)->bound = list_next(LPS(lid)->bound);
		} else {
			return NULL;
		}
	}

	return LPS(lid)->bound;
}


/**
* Insert a message in the bottom half of a locally-hosted LP. Of course,
* the LP must be locally hosted.
*
* @author Alessandro Pellegrini
*
* @param msg The message to be added into some LP's bottom half.
*/
void insert_bottom_half(msg_t *msg) {
	LID_t lid = GidToLid(msg->receiver);

	validate_msg(msg);

	insert_msg(LPS(lid)->bottom_halves, msg);
	#ifdef HAVE_PREEMPTION
	update_min_in_transit(LPS(lid)->worker_thread, msg->timestamp);
	#endif
}


/**
* Process bottom halves received by all the LPs hosted by the current KLT
*
* @author Alessandro Pellegrini
*/
void process_bottom_halves(void) {
	unsigned int i;
	LID_t lid_receiver;
	LP_State *receiver;

	msg_t *msg_to_process;
	msg_t *matched_msg;

	for(i = 0; i < n_prc_per_thread; i++) {

		while((msg_to_process = get_msg(LPS_bound(i)->bottom_halves)) != NULL) {
			lid_receiver = GidToLid(msg_to_process->receiver);
			receiver = LPS(lid_receiver);
			
			if(msg_to_process->timestamp < get_last_gvt())
				printf("ERRORE\n");

			// Handle control messages
			if(!receive_control_msg(msg_to_process)) {
				msg_release(msg_to_process);
				continue;
			}

			switch (msg_to_process->message_kind) {

				// It's an antimessage
				case negative:

					statistics_post_lp_data(lid_receiver, STAT_ANTIMESSAGE, 1.0);

					// Find the message matching the antimessage
					matched_msg = list_tail(receiver->queue_in);
					while(matched_msg != NULL && matched_msg->mark != msg_to_process->mark) {
						matched_msg = list_prev(matched_msg);
					}

					// Sanity check
					if(matched_msg == NULL) {
						rootsim_error(false, "LP %d Received an antimessage, but no such mark has been found!\n", lid_to_int(lid_receiver));
						dump_msg_content(msg_to_process);
						abort();
					} 


					#ifdef HAVE_MPI
					register_incoming_msg(msg_to_process);
					#endif

					// If the matched message is in the past, we have to rollback
					if(matched_msg->timestamp <= lvt(lid_receiver)) {

						receiver->bound = list_prev(matched_msg);
						while((receiver->bound != NULL) && D_EQUAL(receiver->bound->timestamp, msg_to_process->timestamp)) {
							receiver->bound = list_prev(receiver->bound);
						}

						receiver->state = LP_STATE_ROLLBACK;

//						if(matched_msg->unprocessed == false)
	//						goto delete;

						// Unchain the event from the input queue
						list_delete_by_content(receiver->queue_in, matched_msg);
						list_insert_tail(LPS(lid_receiver)->retirement_queue, matched_msg);
					} else {
					    delete:
						// Unchain the event from the input queue
						list_delete_by_content(receiver->queue_in, matched_msg);
						// Delete the matched message
						//msg_release(matched_msg);
						list_insert_tail(LPS(lid_receiver)->retirement_queue, matched_msg);
					}

					break;

				// It's a positive message
				case positive:

					// A positive message is directly placed in the queue
	//				list_insert(receiver->queue_in, timestamp, msg_to_process);
	do {\
		__typeof__(msg_to_process) __n; /* in-block scope variable */\
		__typeof__(msg_to_process) __new_n = (msg_to_process);\
		size_t __key_position = my_offsetof((receiver->queue_in), timestamp);\
		double __key;\
		size_t __size_before;\
		rootsim_list *__l;\
		do {\
			__l = (rootsim_list *)(receiver->queue_in);\
			assert(__l);\
			__size_before = __l->size;\
			if(__l->size == 0) { /* Is the list empty? */\
				__new_n->prev = NULL;\
				__new_n->next = NULL;\
				__l->head = __new_n;\
				__l->tail = __new_n;\
				break;\
			}\
			__key = get_key(__new_n); /* Retrieve the new node's key */\
			/* Scan from the tail, as keys are ordered in an increasing order */\
			__n = __l->tail;\
			while(__n != NULL && __key < get_key(__n)) {\
				__n = __n->prev;\
			}\
			/* Insert depending on the position */\
		 	if(__n == __l->tail) { /* tail */\
				__new_n->next = NULL;\
				((__typeof(msg_to_process))__l->tail)->next = __new_n;\
				__new_n->prev = __l->tail;\
				__l->tail = __new_n;\
			} else if(__n == NULL) { /* head */\
				__new_n->prev = NULL;\
				__new_n->next = __l->head;\
				((__typeof(msg_to_process))__l->head)->prev = __new_n;\
				__l->head = __new_n;\
			} else { /* middle */\
				__new_n->prev = __n;\
				__new_n->next = __n->next;\
				__n->next->prev = __new_n;\
				__n->next = __new_n;\
			}\
		} while(0);\
		__l->size++;\
		assert(__l->size == (__size_before + 1));\
	} while(0);


					// Check if we've just inserted an out-of-order event.
					// Here we check for a strictly minor timestamp since
					// the queue is FIFO for same-timestamp events. Therefore,
					// A contemporaneous event does not cause a causal violation.
					if(msg_to_process->timestamp < lvt(lid_receiver)) {

						receiver->bound = list_prev(msg_to_process);
						while((receiver->bound != NULL) && D_EQUAL(receiver->bound->timestamp, msg_to_process->timestamp)) {
							receiver->bound = list_prev(receiver->bound);
						}

						receiver->state = LP_STATE_ROLLBACK;
					}

					#ifdef HAVE_MPI
					register_incoming_msg(msg_to_process);
					#endif
					break;

				// It's a control message
				case control:

					// Check if it is an anti control message
					if(!anti_control_message(msg_to_process)) {
						msg_release(msg_to_process);
						continue;
					}

					break;

				default:
					rootsim_error(true, "Received a message which is neither positive nor negative. Aborting...\n");
			}
		}
	}

	// We have processed all in transit messages.
	// Actually, during this operation, some new in transit messages could
	// be placed by other threads. In this case, we loose their presence.
	// This is not a correctness error. The only issue could be that the
	// preemptive scheme will not detect this, and some events could
	// be in fact executed out of order.
	#ifdef HAVE_PREEMPTION
	reset_min_in_transit(tid);
	#endif
}




/**
* This function generates a mark value that is unique w.r.t. the previous values for each Logical Process.
* It is based on the Cantor Pairing Function, which maps 2 naturals to a single natural.
* The two naturals are the LP gid (which is unique in the system) and a non decreasing number
* which gets incremented (on a per-LP basis) upon each function call.
* It's fast to calculate the mark, it's not fast to invert it. Therefore, inversion is not
* supported at all in the simulator code (but an external utility is provided for debugging purposes,
* which can be found in src/lp_mark_inverse.c)
*
* @author Alessandro Pellegrini
*
* @param lid The local Id of the Light Process
* @return A value to be used as a unique mark for the message within the LP
*/
unsigned long long generate_mark(LID_t lid) {
	unsigned long long k1 = (unsigned long long)gid_to_int(LidToGid(lid));
	unsigned long long k2 = LPS(lid)->mark++;

	return (unsigned long long)( ((k1 + k2) * (k1 + k2 + 1) / 2) + k2 );
}

