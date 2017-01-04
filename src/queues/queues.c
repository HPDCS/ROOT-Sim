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
#include <queues/queues.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <mm/allocator.h>
#include <scheduler/scheduler.h>
#include <communication/communication.h>
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
simtime_t last_event_timestamp(unsigned int lid) {
	simtime_t ret = 0.0;

	if (LPS[lid]->bound != NULL) {
		ret = LPS[lid]->bound->timestamp;
	}

	return ret;
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
simtime_t next_event_timestamp(unsigned int id) {

	simtime_t ret = -1.0;
	msg_t *evt;

	// The bound can be NULL in the first execution or if it has gone back
	if (LPS[id]->bound == NULL && !list_empty(LPS[id]->queue_in)) {
		ret = list_head(LPS[id]->queue_in)->timestamp;
	} else {
		evt = list_next(LPS[id]->bound);
		if(evt != NULL) {
			ret = evt->timestamp;
		} else {
			ret = -1;
		}
	}

	return ret;

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
msg_t *advance_to_next_event(unsigned int lid) {

	if (LPS[lid]->bound == NULL) {
		if (!list_empty(LPS[lid]->queue_in)) {
			LPS[lid]->bound = list_head(LPS[lid]->queue_in);
		} else {
			return NULL;
		}
	} else {
		if (list_next(LPS[lid]->bound) != NULL) {
			LPS[lid]->bound = list_next(LPS[lid]->bound);
		} else {
			return NULL;
		}
	}

	return LPS[lid]->bound;
}






/**
* Insert a message in the bottom halft of a locally-hosted LP. Of course,
* the LP must be locally hosted. This is guaranteed by the fact
* that the only point where this function is called is from Send(),
* which checks whether the LP is hosted locally from this kernel
* instance or not.
*
* @author Alessandro Pellegrini
*
* @param msg The message to be added into some LP's bottom half.
*/
void insert_bottom_half(msg_t *msg) {

	unsigned int lid = GidToLid(msg->receiver);

	insert_BH(lid, msg, sizeof(msg_t));
	#ifdef HAVE_PREEMPTION
	update_min_in_transit(LPS[lid]->worker_thread, msg->timestamp);
	#endif

	//~ spin_lock(&LPS[lid]->lock);
	//~ (void)list_insert_tail(msg->sender, LPS[lid]->bottom_halves, msg);
	//~ spin_unlock(&LPS[lid]->lock);
}


/**
* Process bottom halves received by all the LPs hosted by the current KLT
*
* @author Alessandro Pellegrini
*/
void process_bottom_halves(void) {
	unsigned int i;
	unsigned int lid_receiver;
	msg_t *msg_to_process;
	msg_t *matched_msg;

	//~ list(msg_t) processing;

	for(i = 0; i < n_prc_per_thread; i++) {

		//~ spin_lock(&LPS_bound[i]->lock);
		//~ processing = LPS_bound[i]->bottom_halves;
		//~ LPS_bound[i]->bottom_halves = new_list(msg_t);
		//~ spin_unlock(&LPS_bound[i]->lock);

		//~ while(!list_empty(processing)) {
			//~ msg_to_process = list_head(processing);

		while((msg_to_process = (msg_t *)get_BH(LPS_bound[i]->lid)) != NULL) {

			lid_receiver = GidToLid(msg_to_process->receiver);

			// TODO: reintegrare per ECS
			//~ if(!receive_control_msg(msg_to_process)) {
				//~ goto expunge_msg;
			//~ }

			switch (msg_to_process->message_kind) {

				// It's an antimessage
				case negative:

					statistics_post_lp_data(msg_to_process->receiver, STAT_ANTIMESSAGE, 1.0);

					// Find the message matching the antimessage
					matched_msg = list_tail(LPS[lid_receiver]->queue_in);
					while(matched_msg != NULL && matched_msg->mark != msg_to_process->mark) {
						matched_msg = list_prev(matched_msg);
					}

					if(matched_msg == NULL) {
						rootsim_error(false, "LP %d Received an antimessage with mark %llu at LP %u from LP %u, but no such mark found in the input queue!\n", LPS_bound[i]->lid, msg_to_process->mark, msg_to_process->receiver, msg_to_process->sender);
						printf("Message Content:"
							"sender: %d\n"
							"receiver: %d\n"
							"type: %d\n"
							"timestamp: %f\n"
							"send time: %f\n"
							"is antimessage %d\n"
							"mark: %llu\n"
							"rendezvous mark %llu\n",
							msg_to_process->sender,
							msg_to_process->receiver,
							msg_to_process->type,
							msg_to_process->timestamp,
							msg_to_process->send_time,
							msg_to_process->message_kind,
							msg_to_process->mark,
							msg_to_process->rendezvous_mark);
						fflush(stdout);
						abort();
					} else {

						// If the matched message is in the past, we have to rollback
						if(matched_msg->timestamp <= lvt(lid_receiver)) {
							LPS[lid_receiver]->bound = list_prev(matched_msg);
							while ((LPS[lid_receiver]->bound != NULL) && LPS[lid_receiver]->bound->timestamp == msg_to_process->timestamp) {
								LPS[lid_receiver]->bound = list_prev(LPS[lid_receiver]->bound);
							}
							LPS[lid_receiver]->state = LP_STATE_ROLLBACK;
						}

						// Delete the matched message
						list_delete_by_content(matched_msg->sender, LPS[lid_receiver]->queue_in, matched_msg);

						list_deallocate_node_buffer(LPS_bound[i]->lid, msg_to_process);
					}

					break;

				// It's a positive message
				case positive:

					list_place_by_content(lid_receiver, LPS[lid_receiver]->queue_in, timestamp, msg_to_process);

					// Check if we've just inserted an out-of-order event
					if(msg_to_process->timestamp < lvt(lid_receiver)) {
						LPS[lid_receiver]->bound = list_prev(msg_to_process);
						while ((LPS[lid_receiver]->bound != NULL) && LPS[lid_receiver]->bound->timestamp == msg_to_process->timestamp) {
							LPS[lid_receiver]->bound = list_prev(LPS[lid_receiver]->bound);
						}
						LPS[lid_receiver]->state = LP_STATE_ROLLBACK;
					}
					break;

				// TODO: reintegrare per ECS
				// It's a control message
				//~ case other:
					// Check if it is an anti control message
					//~ if(!anti_control_message(msg_to_process)) {
						//~ goto expunge_msg;
					//~ }
					//~ break;

				default:
					rootsim_error(true, "Received a message which is neither positive nor negative. Aborting...\n");
			}

		    //~ expunge_msg:
			//~ list_pop(msg_to_process->sender, processing);
		}
		//~ rsfree(processing);
	}

	// We have processed all in transit messages.
	// Actually, during this operation, some new in transit messages could
	// be placed by other threads. In this case, we loose their presence.
	// This is not a correctness error. The only issue could be that the
	// preemptive scheme will not detect this, and some events could
	// be in fact executed out of error.
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
unsigned long long generate_mark(unsigned int lid) {
	unsigned long long k1 = (unsigned long long)LidToGid(lid);
	unsigned long long k2 = LPS[lid]->mark++;

	return (unsigned long long)( ((k1 + k2) * (k1 + k2 + 1) / 2) + k2 );
}
