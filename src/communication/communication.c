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
* @file communication.c
* @brief This module implements all the communication routines, for exchanging
*        messages among different logical processes and simulator instances.
* @author Francesco Quaglia
* @author Roberto Vitali
*
*/

#include <stdlib.h>
#include <float.h>

#include <core/core.h>
#include <gvt/gvt.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <statistics/statistics.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <datatypes/list.h>
#include <mm/mm.h>
#include <arch/atomic.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

/// This is the function pointer to correctly set ScheduleNewEvent API version, depending if we're running serially or parallelly
void (*ScheduleNewEvent)(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

/**
* This function initializes the communication subsystem
*
* @author Roberto Vitali
*/
void communication_init(void)
{
#ifdef HAVE_MPI
	inter_kernel_comm_init();
#endif
}

void communication_init_thread(void)
{
}

void communication_fini_thread(void)
{
}

/**
* Finalizes the communication subsystem
*/
void communication_fini(void)
{
#ifdef HAVE_MPI
	inter_kernel_comm_finalize();
	mpi_finalize();
#endif
}

static inline struct lp_struct *which_slab_to_use(GID_t sender, GID_t receiver)
{
	// Local messages are taken in the destination slab.
	// Remote buffers are taken from the sender slab (this will be
	// freed shortly, once we hand that to MPI)
	if (find_kernel_by_gid(receiver) == kid)
		return find_lp_by_gid(receiver);
	return find_lp_by_gid(sender);
}

// Headers are always taken from the sender slab
void msg_hdr_release(msg_hdr_t * msg)
{
	struct lp_struct *lp;

	lp = find_lp_by_gid(msg->sender);
	slab_free(lp->mm->slab, msg);
}

msg_hdr_t *get_msg_hdr_from_slab(struct lp_struct *lp)
{
	// TODO: The magnitude of this hack compares to that of the national debt.
	// We are wasting a lot of memory from the LP buddy just to keep antimessages!
	msg_hdr_t *msg = (msg_hdr_t *) get_msg_from_slab(lp);
	bzero(msg, SLAB_MSG_SIZE);
	return msg;
}

msg_t *get_msg_from_slab(struct lp_struct *lp)
{
	msg_t *msg = (msg_t *) slab_alloc(lp->mm->slab);
	bzero(msg, SLAB_MSG_SIZE);
	return msg;
}

void msg_release(msg_t * msg)
{
	struct lp_struct *lp;

	if (likely(sizeof(msg_t) + msg->size <= SLAB_MSG_SIZE)) {
		lp = which_slab_to_use(msg->sender, msg->receiver);
		slab_free(lp->mm->slab, msg);
	} else {
		rsfree(msg);
	}
}

/**
* This function is invoked by the application level software to inject new events into the simulation
*
* @author Francesco Quaglia
*
* @param gid_receiver Global id of logical process at which the message must be delivered
* @param timestamp Logical Virtual Time associated with the event enveloped into the message
* @param event_type Type of the event
* @param event_content Payload of the event
* @param event_size Size of event's payload
*/
void ParallelScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size)
{
	msg_t *event;
	GID_t receiver;

	switch_to_platform_mode();

	// Internally to the platform, the receiver is a GID, while models
	// have no difference across GIDs and LIDs. We convert here the passed
	// id to a GID.
	set_gid(receiver, gid_receiver);

	// In Silent execution, we do not send again already sent messages
	if (unlikely(current->state == LP_STATE_SILENT_EXEC)) {
		return;
	}
	// Check whether the destination LP is out of range
	if (unlikely(gid_receiver > n_prc_tot - 1)) {	// It's unsigned, so no need to check whether it's < 0
		rootsim_error(false, "Warning: the destination LP %u is out of range. The event has been ignored\n", gid_receiver);
		goto out;
	}
	// Check if the associated timestamp is negative
	if (unlikely(timestamp < lvt(current))) {
		rootsim_error(true, "LP %u is trying to generate an event (type %d) to %u in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n",
			      current->gid, event_type, gid_receiver,
			      lvt(current), timestamp);
	}
	// Check if the event type is mapped to an internal control message
	if (unlikely(event_type >= RESERVED_MSG_CODE)) {
		rootsim_error(true, "LP %u is generating an event with type %d which is a reserved type. Switch event type to a value less than %d. Aborting...\n",
			      current->gid, event_type, MIN_VALUE_CONTROL);
	}

	// Copy all the information into the event structure
	pack_msg(&event, current->gid, receiver, event_type, timestamp,
		 lvt(current), event_size, event_content);
	event->mark = generate_mark(current);

	if (unlikely(event->type == RENDEZVOUS_START)) {
		event->rendezvous_mark = current_evt->rendezvous_mark;
		printf("rendezvous_start mark=%llu\n", event->rendezvous_mark);
		fflush(stdout);
	}

	insert_outgoing_msg(event);

 out:
	switch_to_application_mode();
}

/**
* This function send all the antimessages for a certain lp.
* After the antimessage is sent, the header is removed from the output queue!
*
* @author Francesco Quaglia
*
* @param lp A pointer to the LP lp_struct for which antimessages should be sent
* @param after_simtime The simulation time instant after which to send antimessages
*/
void send_antimessages(struct lp_struct *lp, simtime_t after_simtime)
{
	msg_hdr_t *anti_msg, *anti_msg_prev;
	msg_t *msg;

	if (unlikely(list_empty(lp->queue_out)))
		return;

	// Scan the output queue backwards, sending all required antimessages
	anti_msg = list_tail(lp->queue_out);
	while (anti_msg != NULL && anti_msg->send_time > after_simtime) {
		msg = get_msg_from_slab(which_slab_to_use(anti_msg->sender, anti_msg->receiver));
		hdr_to_msg(anti_msg, msg);
		msg->message_kind = negative;

		Send(msg);

		// Remove the already-sent antimessage from the output queue
		anti_msg_prev = list_prev(anti_msg);
		list_delete_by_content(lp->queue_out, anti_msg);
		msg_hdr_release(anti_msg);
		anti_msg = anti_msg_prev;
	}
}

/**
*
*
* @author Roberto Vitali
*/
void comm_finalize(void)
{

	// Release as well memory used for remaining input/output queues
	foreach_lp(lp) {
		while (!list_empty(lp->queue_in)) {
			list_pop(lp->queue_in);
		}
		while (!list_empty(lp->queue_out)) {
			list_pop(lp->queue_out);
		}
	}
}

/**
* Send a message. if it's scheduled to a local LP, update its queue, otherwise
* ask MPI to deliver it to the hosting kernel instance.
*
* @author francesco quaglia
*/
void Send(msg_t * msg)
{

	validate_msg(msg);

#ifdef HAVE_MPI
	// Check whether the message recepient kernel is remote
	if (find_kernel_by_gid(msg->receiver) != kid) {
		send_remote_msg(msg);
		return;
	}
#endif
	insert_bottom_half(msg);
}

/**
*
*
* @author Francesco Quaglia
*/
void insert_outgoing_msg(msg_t * msg)
{

	// if the model is generating many events at the same time, reallocate the outgoing buffer
	if (unlikely(current->outgoing_buffer.size == current->outgoing_buffer.max_size)) {
		current->outgoing_buffer.max_size *= 2;
		current->outgoing_buffer.outgoing_msgs = rsrealloc(current->outgoing_buffer.outgoing_msgs, sizeof(msg_t *) * current->outgoing_buffer.max_size);
	}

	current->outgoing_buffer.outgoing_msgs[current->outgoing_buffer.size++] = msg;

	// store the minimum timestamp of outgoing messages
	if (msg->timestamp <
	    current->outgoing_buffer.min_in_transit[current->worker_thread]) {
		current->outgoing_buffer.min_in_transit[current->worker_thread] = msg->timestamp;
	}
}

void send_outgoing_msgs(struct lp_struct *lp)
{
	register unsigned int i = 0;
	msg_t *msg;
	msg_hdr_t *msg_hdr;

	for (i = 0; i < lp->outgoing_buffer.size; i++) {
		msg_hdr = get_msg_hdr_from_slab(lp);
		msg = lp->outgoing_buffer.outgoing_msgs[i];
		msg_to_hdr(msg_hdr, msg);

		Send(msg);

		// register the message in the sender's output queue, for antimessage management
		list_insert(lp->queue_out, send_time, msg_hdr);
	}

	lp->outgoing_buffer.size = 0;
}

// TODO: si può generare qua dentro la marca, perché si usa sempre il sender. Occhio al gid/lid!!!!
void pack_msg(msg_t ** msg, GID_t sender, GID_t receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload)
{

	// Check if we can rely on a slab to initialize the message
	if (likely(sizeof(msg_t) + size <= SLAB_MSG_SIZE)) {
		*msg = get_msg_from_slab(which_slab_to_use(sender, receiver));
	} else {
		*msg = rsalloc(sizeof(msg_t) + size);
		bzero(*msg, sizeof(msg_t) + size);
	}

	(*msg)->sender = sender;
	(*msg)->receiver = receiver;
	(*msg)->type = type;
	(*msg)->message_kind = positive;
	(*msg)->timestamp = timestamp;
	(*msg)->send_time = send_time;
	(*msg)->size = size;

	if (payload != NULL && size > 0)
		memcpy((*msg)->event_content, payload, size);
}

void msg_to_hdr(msg_hdr_t * hdr, msg_t * msg)
{
	validate_msg(msg);

	hdr->sender = msg->sender;
	hdr->receiver = msg->receiver;
	hdr->type = msg->type;
	hdr->rendezvous_mark = msg->rendezvous_mark;
	hdr->timestamp = msg->timestamp;
	hdr->send_time = msg->send_time;
	hdr->mark = msg->mark;
}

void hdr_to_msg(msg_hdr_t * hdr, msg_t * msg)
{
	msg->sender = hdr->sender;
	msg->receiver = hdr->receiver;
	msg->type = hdr->type;
	msg->rendezvous_mark = hdr->rendezvous_mark;
	msg->timestamp = hdr->timestamp;
	msg->send_time = hdr->send_time;
	msg->mark = hdr->mark;
}

void dump_msg_content(msg_t * msg)
{
	printf("\tsender: %u\n", msg->sender.to_int);
	printf("\treceiver: %u\n", msg->sender.to_int);
#ifdef HAVE_MPI
	printf("\tcolour: %d\n", msg->colour);
#endif
	printf("\ttype: %d\n", msg->type);
	printf("\tmessage_kind: %d\n", msg->message_kind);
	printf("\ttimestamp: %f\n", msg->timestamp);
	printf("\tsend_time: %f\n", msg->send_time);
	printf("\tmark: %llu\n", msg->mark);
	printf("\trendezvous_mark: %llu\n", msg->rendezvous_mark);
	printf("\tsize: %d\n", msg->size);
}

#ifndef NDEBUG
unsigned int mark_to_gid(unsigned long long mark)
{
	double z = (double)mark;
	double w = floor((sqrt(8 * z + 1) - 1) / 2.0);
	double t = (w * w + w) / 2.0;
	double y = z - t;
	double x = w - y;

	return (int)x;
}

void validate_msg(msg_t * msg)
{
	assert(msg->sender.to_int <= n_prc_tot);
	assert(msg->receiver.to_int <= n_prc_tot);
	assert(msg->message_kind == positive || msg->message_kind == negative
	       || msg->message_kind == control);
	assert(mark_to_gid(msg->mark) <= n_prc_tot);
	assert(mark_to_gid(msg->rendezvous_mark) <= n_prc_tot);
	assert(msg->type < MAX_VALUE_CONTROL);
}
#endif
