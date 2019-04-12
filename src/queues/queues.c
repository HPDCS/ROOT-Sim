/**
* @file queues/queues.c
*
* @brief Message queueing subsystem
*
* This module implements the event/message queues subsystem.
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
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
* @date March 16, 2011
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
#include <mm/mm.h>
#include <scheduler/scheduler.h>
#include <communication/communication.h>
#include <communication/gvt.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>

/**
* This function return the timestamp of the next-to-execute event
*
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @param lp A pointer to the LP's lp_struct for which we want to discover
*           the timestamp of the next event
* @return The timestamp of the next-to-execute event
*/
simtime_t next_event_timestamp(struct lp_struct *lp)
{
	msg_t *evt;

	// The bound can be NULL in the first execution or if it has gone back
	if (unlikely(lp->bound == NULL && !list_empty(lp->queue_in))) {
		return list_head(lp->queue_in)->timestamp;
	} else {
		evt = list_next(lp->bound);
		if (likely(evt != NULL)) {
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
* @param lp A pointer to the LP's lp_struct which should have its bound
*           updated in order to point to the next event to be processed
* @return The pointer to the event is going to be processed
*/
msg_t *advance_to_next_event(struct lp_struct *lp)
{
	if (likely(list_next(lp->bound) != NULL)) {
		lp->bound = list_next(lp->bound);
	} else {
		return NULL;
	}

	return lp->bound;
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
void insert_bottom_half(msg_t * msg)
{
	struct lp_struct *lp = find_lp_by_gid(msg->receiver);

	validate_msg(msg);

	insert_msg(lp->bottom_halves, msg);
#ifdef HAVE_PREEMPTION
	update_min_in_transit(lp->worker_thread, msg->timestamp);
#endif
}

/**
* Process bottom halves received by all the LPs hosted by the current KLT
*
* @author Alessandro Pellegrini
*/
void process_bottom_halves(void)
{
	struct lp_struct *receiver;

	msg_t *msg_to_process;
	msg_t *matched_msg;

	foreach_bound_lp(lp) {

		while ((msg_to_process = get_msg(lp->bottom_halves)) != NULL) {
			receiver = find_lp_by_gid(msg_to_process->receiver);

			// Sanity check
			if (unlikely
			    (msg_to_process->timestamp < get_last_gvt()))
				rootsim_error(true,
					      "The impossible happened: I'm receiving a message before the GVT\n");

			// Handle control messages
			if (unlikely(!receive_control_msg(msg_to_process))) {
				msg_release(msg_to_process);
				continue;
			}

			switch (msg_to_process->message_kind) {

				// It's an antimessage
			case negative:

				statistics_post_data(receiver, STAT_ANTIMESSAGE, 1.0);

				// Find the message matching the antimessage
				matched_msg = list_tail(receiver->queue_in);
				while (matched_msg != NULL
				       && matched_msg->mark !=
				       msg_to_process->mark) {
					matched_msg = list_prev(matched_msg);
				}

				// Sanity check
				if (unlikely(matched_msg == NULL)) {
					rootsim_error(false,
						      "LP %d Received an antimessage, but no such mark has been found!\n",
						      receiver->gid.to_int);
					dump_msg_content(msg_to_process);
					rootsim_error(true, "Aborting...\n");
				}
				// If the matched message is in the past, we have to rollback
				if (matched_msg->timestamp <= lvt(receiver)) {

					receiver->bound = list_prev(matched_msg);
					while ((receiver->bound != NULL)
						&& D_EQUAL(receiver->bound->timestamp, msg_to_process->timestamp)) {
						receiver->bound = list_prev(receiver->bound);
					}
					
					receiver->state = LP_STATE_ROLLBACK;
				}
#ifdef HAVE_MPI
				register_incoming_msg(msg_to_process);
#endif

				// Delete the matched message
				list_delete_by_content(receiver->queue_in,
						       matched_msg);
				msg_release(matched_msg);

				break;

				// It's a positive message
			case positive:

				// A positive message is directly placed in the queue
				list_insert(receiver->queue_in, timestamp,
					    msg_to_process);

				// Check if we've just inserted an out-of-order event.
				// Here we check for a strictly minor timestamp since
				// the queue is FIFO for same-timestamp events. Therefore,
				// A contemporaneous event does not cause a causal violation.
				if (msg_to_process->timestamp < lvt(receiver)) {

					receiver->bound = list_prev(msg_to_process);
					while ((receiver->bound != NULL)
					       && D_EQUAL(receiver->bound->timestamp, msg_to_process->timestamp)) {
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
				if (!anti_control_message(msg_to_process)) {
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
	reset_min_in_transit(local_tid);
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
* @param lp A pointer to the LP's lp_struct for which we want to generate
*           a system-wide unique mark
* @return A value to be used as a unique mark for the message within the LP
*/
unsigned long long generate_mark(struct lp_struct *lp)
{
	unsigned long long k1 = (unsigned long long)lp->gid.to_int;
	unsigned long long k2 = lp->mark++;

	return (unsigned long long)(((k1 + k2) * (k1 + k2 + 1) / 2) + k2);
}
