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
#include <datatypes/slab.h>
#include <mm/dymelor.h>
#include <arch/atomic.h>
#ifdef HAS_MPI
#include <communication/mpi.h>
#endif

static struct slab_chain *msg_slab;
static spinlock_t *slab_lock;


/// This is the function pointer to correctly set ScheduleNewEvent API version, depending if we're running serially or parallelly
void (* ScheduleNewEvent)(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

/// Buffer used by MPI for outgoing messages
//static char buff[SLOTS * sizeof(msg_t)];


/**
* This function initializes the communication subsystem
*
* @author Roberto Vitali
*/
void communication_init(void) {
	int i;

	#ifdef HAS_MPI
	inter_kernel_comm_init();
	#endif

	msg_slab = rsalloc(n_cores * sizeof(*msg_slab));
	slab_lock = rsalloc(n_cores * sizeof(*slab_lock));

	for(i = 0; i < n_cores; i++) {
		spinlock_init(&slab_lock[i]);
	}
}


void communication_init_thread(void) {
	slab_init(&msg_slab[local_tid], SLAB_MSG_SIZE); 
}

void communication_fini_thread(void) {
	slab_destroy(&msg_slab[local_tid]);
}


/**
* Finalizes the communication subsystem
*/
void communication_fini(void) {
	#ifdef HAS_MPI
	inter_kernel_comm_finalize();
	mpi_finalize();
	#endif

	rsfree(msg_slab);
	rsfree(slab_lock);
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
void ParallelScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	msg_t *event;
	switch_to_platform_mode();

	// In Silent execution, we do not send again already sent messages
	if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC) {
		return;
	}

	// Check whether the destination LP is out of range
	if(gid_receiver > n_prc_tot - 1) {	// It's unsigned, so no need to check whether it's < 0
		rootsim_error(false, "Warning: the destination LP %u is out of range. The event has been ignored\n", gid_receiver);
		goto out;
	}

	// Check if the associated timestamp is negative
	if(timestamp < lvt(current_lp)) {
		rootsim_error(true, "LP %u is trying to generate an event (type %d) to %u in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n", current_lp, event_type, gid_receiver, lvt(current_lp), timestamp);
	}

	// Check if the event type is mapped to an internal control message
	if(event_type >= MIN_VALUE_CONTROL) {
		rootsim_error(true, "LP %u is generating an event with type %d which is a reserved type. Switch event type to a value less than %d. Aborting...\n", current_lp, event_type, MIN_VALUE_CONTROL);
	}


	// Copy all the information into the event structure
	pack_msg(&event, LidToGid(current_lp), gid_receiver, event_type, timestamp, lvt(current_lp), event_size, event_content);
	event->mark = generate_mark(current_lp);

	if(event->type == RENDEZVOUS_START) {
		event->rendezvous_mark = current_evt->rendezvous_mark;
		printf("rendezvous_start mark=%llu\n",event->rendezvous_mark);
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
* @param lid The logical process id
*
* @todo One shot scan of the list
*/
void send_antimessages(unsigned int lid, simtime_t after_simtime) {
	msg_hdr_t *anti_msg, *anti_msg_next;
	msg_t *msg;

//	printf("send_antimessages lid: %d after: %f\n", lid, after_simtime);

	if (list_empty(LPS[lid]->queue_out))
		return;

	// get the first message header with a timestamp <= after_simtime
	anti_msg = list_tail(LPS[lid]->queue_out);
	while(anti_msg != NULL && anti_msg->send_time > after_simtime) {
		anti_msg = list_prev(anti_msg);
	}
	// the next event is the first event with a sendtime > after_simtime, if any.
	// explicitly consider the case in which all anti messages should be sent.
	if(anti_msg == NULL && list_head(LPS[lid]->queue_out)->send_time <= after_simtime) {
		return;
	} else if (anti_msg == NULL && list_head(LPS[lid]->queue_out)->send_time > after_simtime) {
		anti_msg = list_head(LPS[lid]->queue_out);
	} else {
		anti_msg = list_next(anti_msg);
	}

	// now send all antimessages
	while(anti_msg != NULL) {
		msg = get_msg_from_slab();
		hdr_to_msg(anti_msg, msg);
		msg->message_kind = negative;

//		dump_msg_content(msg);

		Send(msg);

		// Remove the already sent antimessage from output queue
		anti_msg_next = list_next(anti_msg);
		list_delete_by_content(lid, LPS[lid]->queue_out, anti_msg);
		anti_msg = anti_msg_next;
	}
}





/**
*
*
* @author Roberto Vitali
*/
int comm_finalize(void) {

	register unsigned int i;

	// Release as well memory used for remaining input/output queues
	for(i = 0; i < n_prc; i++) {
		while(!list_empty(LPS[i]->queue_in)) {
			list_pop(i, LPS[i]->queue_in);
		}
		while(!list_empty(LPS[i]->queue_out)) {
			list_pop(i, LPS[i]->queue_out);
		}
	}

	return 0; // TODO: What's the point of this return?
}



/**
* Send a message. if it's scheduled to a local LP, update its queue, otherwise
* ask MPI to deliver it to the hosting kernel instance.
*
* @author francesco quaglia
*/
void Send(msg_t *msg) {

	validate_msg(msg);

	#ifdef HAS_MPI
	// Check whether the message recepient kernel is remote
	if(GidToKernel(msg->receiver) != kid){
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
void insert_outgoing_msg(msg_t *msg) {

	// if the model is generating many events at the same time, reallocate the outgoing buffer
	if(LPS[current_lp]->outgoing_buffer.size == LPS[current_lp]->outgoing_buffer.max_size){
		LPS[current_lp]->outgoing_buffer.max_size *= 2;
		LPS[current_lp]->outgoing_buffer.outgoing_msgs = rsrealloc(LPS[current_lp]->outgoing_buffer.outgoing_msgs, sizeof(msg_t *) * LPS[current_lp]->outgoing_buffer.max_size);
	}

	LPS[current_lp]->outgoing_buffer.outgoing_msgs[LPS[current_lp]->outgoing_buffer.size++] = msg;

	// store the minimum timestamp of outgoing messages
	if(msg->timestamp < LPS[current_lp]->outgoing_buffer.min_in_transit[LPS[current_lp]->worker_thread]) {
		LPS[current_lp]->outgoing_buffer.min_in_transit[LPS[current_lp]->worker_thread] = msg->timestamp;
	}
}



void send_outgoing_msgs(unsigned int lid) {

	register unsigned int i = 0;
	msg_t *msg;
	msg_hdr_t msg_hdr;

	for(i = 0; i < LPS[lid]->outgoing_buffer.size; i++) {
		msg = LPS[lid]->outgoing_buffer.outgoing_msgs[i];
		msg_to_hdr(&msg_hdr, msg);

//		dump_msg_content(msg);

//		printf("send %p\n", msg);

		Send(msg);

		// register the message in the sender's output queue, for antimessage management
		(void)list_insert(lid, LPS[lid]->queue_out, send_time, &msg_hdr);
	}

	LPS[lid]->outgoing_buffer.size = 0;
}


msg_t *get_msg_from_slab(void) {
	spin_lock(&slab_lock[local_tid]);
	msg_t *msg = (msg_t *)slab_alloc(&msg_slab[local_tid]);
	spin_unlock(&slab_lock[local_tid]);
	#ifndef NDEBUG
	bzero(msg, SLAB_MSG_SIZE-8);
	#endif
	msg->alloc_tid = local_tid;
	return msg;
}


// TODO: si può generare qua dentro la marca, perché si usa sempre il sender. Occhio al gid/lid!!!!
void pack_msg(msg_t **msg, unsigned int sender, unsigned int receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload) {

	// Check if we can rely on a slab to initialize the message
	if(sizeof(msg_t) + size <= SLAB_MSG_SIZE) {
		*msg = get_msg_from_slab();
	} else {
		*msg = rsalloc(sizeof(msg_t) + size);
	}

	(*msg)->sender = sender;
	(*msg)->receiver = receiver;
	(*msg)->type = type;
	(*msg)->message_kind = positive;
	(*msg)->timestamp = timestamp;
	(*msg)->send_time = send_time;
	(*msg)->size = size;

	if(payload != NULL && size > 0)
		memcpy((*msg)->event_content, payload, size);
}

void msg_to_hdr(msg_hdr_t *hdr, msg_t *msg) {
	validate_msg(msg);

	hdr->sender = msg->sender;
	hdr->receiver = msg->receiver;
	hdr->type = msg->type;
	hdr->rendezvous_mark = msg->rendezvous_mark;
	hdr->timestamp = msg->timestamp;
	hdr->send_time = msg->send_time;
	hdr->mark = msg->mark;
}

void hdr_to_msg(msg_hdr_t *hdr, msg_t *msg) {
	msg->sender = hdr->sender;
	msg->receiver = hdr->receiver;
	msg->type = hdr->type;
	msg->rendezvous_mark = hdr->rendezvous_mark;
	msg->timestamp = hdr->timestamp;
	msg->send_time = hdr->send_time;
	msg->mark = hdr->mark;
}

void msg_release(msg_t *msg) {
	if(sizeof(msg_t) + msg->size <= SLAB_MSG_SIZE) {
		int thr = msg->alloc_tid;

		spin_lock(&slab_lock[thr]);
		#ifndef NDEBUG
		bzero(msg, sizeof(msg_t) + msg->size);
		#endif
		slab_free(&msg_slab[thr], msg);
		spin_unlock(&slab_lock[thr]);
	} else {
		rsfree(msg);
	}
}

void dump_msg_content(msg_t *msg) {
	printf("\tsender: %lu\n", msg->sender);
	printf("\treceiver: %lu\n", msg->sender);
	#ifdef HAS_MPI
	printf("\tcolour: %d\n", msg->colour);
	#endif
	printf("\ttype: %d\n", msg->type);
	printf("\tmessage_kind: %d\n", msg->message_kind);
	printf("\ttimestamp: %f\n", msg->timestamp);
	printf("\tsend_time: %f\n", msg->send_time);
	printf("\tmark: %llu\n", msg->mark);
	printf("\trendezvous_mark: %llu\n", msg->rendezvous_mark);
	printf("\tsize: %llu\n", msg->size);
}

#ifndef NDEBUG
unsigned int mark_to_gid(unsigned long long mark) {
	double z = (double)mark;
        double w = floor( (sqrt( 8 * z + 1) - 1) / 2.0);
        double t = ( w*w + w ) / 2.0;
        double y = z - t;
        double x = w - y;

	return (int)x;
}

void validate_msg(msg_t *msg) {
	assert(msg->sender <= n_prc_tot);
	assert(msg->receiver <= n_prc_tot);
	assert(msg->message_kind == positive || msg->message_kind == negative || msg->message_kind == control);
	assert(mark_to_gid(msg->mark) <= n_prc_tot);
	assert(mark_to_gid(msg->rendezvous_mark) <= n_prc_tot);
	assert(msg->type < MAX_VALUE_CONTROL);
}
#endif

