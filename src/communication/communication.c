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

#include <arch/thread.h>
#include <core/core.h>
#include <core/init.h>
#include <gvt/gvt.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <statistics/statistics.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <datatypes/list.h>
#include <datatypes/slab.h>
#include <datatypes/treiber.h>
#include <mm/dymelor.h>
#include <arch/atomic.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

static treiber **msg_treiber;
static struct slab_chain *msg_slab;


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
	unsigned int i;

	#ifdef HAVE_MPI
	inter_kernel_comm_init();
	#endif

	msg_treiber = rsalloc(n_cores * sizeof(msg_treiber));
	msg_slab = rsalloc(n_cores * sizeof(*msg_slab));

	for(i = 0; i < n_cores; i++) {
		msg_treiber[i] = treiber_init();
	}
}


void communication_init_thread(void) {
	slab_init(&msg_slab[tid], SLAB_MSG_SIZE); 
}

void communication_fini_thread(void) {
	slab_destroy(&msg_slab[tid]);
}


/**
* Finalizes the communication subsystem
*/
void communication_fini(void) {
	#ifdef HAVE_MPI
	inter_kernel_comm_finalize();
	mpi_finalize();
	#endif

	rsfree(msg_slab);
}

msg_hdr_t *get_msg_hdr_from_slab(void) {
	// TODO: The magnitude of this hack compares to that of the national debt.
	// We must have a single allocation point where we just get a buffer, and then
	// we map that to the various data structures.
	msg_hdr_t *msg = (msg_hdr_t *)get_msg_from_slab();
	bzero(msg, SLAB_MSG_SIZE);
	msg->alloc_tid = tid;
	return msg;
}

void msg_hdr_release(msg_hdr_t *msg) {
	int thr = msg->alloc_tid;
	treiber_push(msg_treiber[thr], msg);
}

msg_t *get_msg_from_slab(void) {
	msg_t *msg = NULL;
	treiber *to_release;
	treiber *to_release_nxt;

	// Unlink the whole Treiber stack and release all nodes.
	// If at least one node is available, reuse it for the
	// current allocation.
	to_release = treiber_detach(msg_treiber[tid]);
	while(to_release != NULL) {
		if(msg == NULL) {
			msg = to_release->data;
		} else {
			slab_free(&msg_slab[tid], to_release->data);
		}
		to_release_nxt = to_release->next;
		rsfree(to_release);
		to_release = to_release_nxt;
	}

	if(msg != NULL) {
		goto out;
	}

	msg = (msg_t *)slab_alloc(&msg_slab[tid]);

    out:
	bzero(msg, SLAB_MSG_SIZE);
	msg->alloc_tid = tid;
	return msg;
}

void msg_release(msg_t *msg) {
	unsigned int thr;

	if(sizeof(msg_t) + msg->size <= SLAB_MSG_SIZE) {
		thr = msg->alloc_tid;
		if(tid == thr) {
			slab_free(&msg_slab[thr], msg);
		} else {
			treiber_push(msg_treiber[thr], msg);
		}
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
void ParallelScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	msg_t *event;
	GID_t receiver;

	switch_to_platform_mode();

	// Internally to the platform, the receiver is a GID, while models
	// have no difference across GIDs and LIDs. We convert here the passed
	// id to a GID.
	set_gid(receiver, gid_receiver);

	// In Silent execution, we do not send again already sent messages
	if(LPS(current_lp)->state == LP_STATE_SILENT_EXEC) {
		return;
	}

	/* Sanity checks */
#ifndef NDEBUG
	// Check whether the destination LP is out of range
	if(receiver.id > n_prc_tot - 1) { // It's unsigned, so no need to check whether it's < 0
		rootsim_error(false, "Warning: the destination LP %u is out of range. The event has been ignored\n", receiver.id);
		goto out;
	}

	// Check if the associated timestamp is negative. In asymmetric computation, anyhow, this sanity check doesn't hold.
	if(rootsim_config.num_controllers == 0 && timestamp < lvt(current_lp)) {
		rootsim_error(true, "LP %u is trying to generate an event (type %d) to %u in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n", current_lp, event_type, receiver.id, lvt(current_lp), timestamp);
	}

	// Check if the event type is mapped to an internal control message
	if(event_type >= MIN_VALUE_CONTROL) {
		rootsim_error(true, "LP %u is generating an event with type %d which is a reserved type. Switch event type to a value less than %d. Aborting...\n", current_lp, event_type, MIN_VALUE_CONTROL);
	}
#endif


	// Copy all the information into the event structure
	pack_msg(&event, LidToGid(current_lp), receiver, event_type, timestamp, current_evt->timestamp, event_size, event_content);
	event->mark = generate_mark(current_lp);

	if(event->type == RENDEZVOUS_START) {
		event->rendezvous_mark = current_evt->rendezvous_mark;
		printf("rendezvous_start mark=%llu\n",event->rendezvous_mark);
		fflush(stdout);
	}

	insert_outgoing_msg(event);

#ifndef NDEBUG
    out:
#endif
	switch_to_application_mode();
}



/**
* This function send all the antimessages for a certain lp.
* After the antimessage is sent, the header is removed from the output queue!
*
* @author Francesco Quaglia
*
* @param lid The Logical Process Id
*
* @todo One shot scan of the list
*/
void send_antimessages(LID_t lid, simtime_t after_simtime) {
	msg_hdr_t *anti_msg, *anti_msg_prev;
	msg_t *msg;

//	printf("send_antimessages lid: %d after: %f\n", lid, after_simtime);

	if (list_empty(LPS(lid)->queue_out))
		return;

	// Scan the output queue backwards, sending all required antimessages
	anti_msg = list_tail(LPS(lid)->queue_out);
	while(anti_msg != NULL && anti_msg->send_time > after_simtime) {
		msg = get_msg_from_slab();
		hdr_to_msg(anti_msg, msg);
		msg->message_kind = negative;

		Send(msg);

		// Remove the already-sent antimessage from the output queue
		anti_msg_prev = list_prev(anti_msg);
                list_delete_by_content(LPS(lid)->queue_out, anti_msg);
		msg_hdr_release(anti_msg);
                anti_msg = anti_msg_prev;
	}
}





/**
*
*
* @author Roberto Vitali
*/
int comm_finalize(void) {

//	register unsigned int i;

	// TODO: reimplement with foreach

	// Release as well memory used for remaining input/output queues
/*	for(i = 0; i < n_prc; i++) {
		while(!list_empty(LPS[i]->queue_in)) {
			list_pop(i, LPS[i]->queue_in);
		}
		while(!list_empty(LPS[i]->queue_out)) {
			list_pop(i, LPS[i]->queue_out);
		}
	}
*/
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

	#ifdef HAVE_MPI
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
	if(LPS(current_lp)->outgoing_buffer.size == LPS(current_lp)->outgoing_buffer.max_size){
		LPS(current_lp)->outgoing_buffer.max_size *= 2;
		LPS(current_lp)->outgoing_buffer.outgoing_msgs = rsrealloc(LPS(current_lp)->outgoing_buffer.outgoing_msgs, sizeof(msg_t *) * LPS(current_lp)->outgoing_buffer.max_size);
	}

	LPS(current_lp)->outgoing_buffer.outgoing_msgs[LPS(current_lp)->outgoing_buffer.size++] = msg;

	// store the minimum timestamp of outgoing messages
	if(msg->timestamp < LPS(current_lp)->outgoing_buffer.min_in_transit[LPS(current_lp)->worker_thread]) {
		LPS(current_lp)->outgoing_buffer.min_in_transit[LPS(current_lp)->worker_thread] = msg->timestamp;
	}
}



void send_outgoing_msgs(LID_t lid) {

	register unsigned int i = 0;
	msg_t *msg;
	msg_hdr_t *msg_hdr;

	for(i = 0; i < LPS(lid)->outgoing_buffer.size; i++) {
		msg_hdr = get_msg_hdr_from_slab();
		msg = LPS(lid)->outgoing_buffer.outgoing_msgs[i];
		msg_to_hdr(msg_hdr, msg);

		Send(msg);

		// register the message in the sender's output queue, for antimessage management
		list_insert(LPS(lid)->queue_out, send_time, msg_hdr);

		if(msg->send_time > LPS(lid)->last_sent_time)
			LPS(lid)->last_sent_time = msg->send_time;
	}

	LPS(lid)->outgoing_buffer.size = 0;
}


void asym_send_outgoing_msgs(LID_t lid) {
	register unsigned int i = 0;
	msg_t *msg;

	for(i = 0; i < LPS(lid)->outgoing_buffer.size; i++) {
		msg = LPS(lid)->outgoing_buffer.outgoing_msgs[i];

		pt_put_out_msg(msg);
//		printf("Putting in the output port the following message\n");
//		dump_msg_content(msg);
	}

	LPS(lid)->outgoing_buffer.size = 0;
}

void asym_extract_generated_msgs(void) {
	unsigned int i;
	msg_t *msg;
	msg_hdr_t *msg_hdr;
	for(i = 0; i < Threads[tid]->num_PTs; i++) {
//		printf("Output port size for PT %u: %d\n", Threads[tid]->PTs[i]->tid), atomic_read(&Threads[tid]->PTs[i]->output_port->size);
		while((msg = pt_get_out_msg(Threads[tid]->PTs[i]->tid)) != NULL) {
			if(is_control_msg(msg->type) && msg->type == ASYM_ROLLBACK_ACK) {
				LPS(GidToLid(msg->receiver))->state = LP_STATE_ROLLBACK_ALLOWED;
				printf("Received ROLLBACK ACK for LP %d with timestamp %lf\n", gid_to_int(msg->receiver), msg->timestamp);
				msg_release(msg);
				continue;
			}
			Send(msg);

			if(msg->send_time > LPS(GidToLid(msg->sender))->last_sent_time &&
					LPS(GidToLid(msg->sender))->state == LP_STATE_READY)
				LPS(GidToLid(msg->sender))->last_sent_time = msg->send_time;

			msg_hdr = get_msg_hdr_from_slab();
			msg_to_hdr(msg_hdr, msg);
			// register the message in the sender's output queue, for antimessage management
			// We can use msg->sender here (ensuring data separation) because we're extracting
			// messages from an output port coming from a PT managed by the current CT, therefore
			// involving only local LPs.
			list_insert(LPS(GidToLid(msg->sender))->queue_out, send_time, msg_hdr);
		}
	}
}


// TODO: si può generare qua dentro la marca, perché si usa sempre il sender. Occhio al gid/lid!!!!
void pack_msg(msg_t **msg, GID_t sender, GID_t receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload) {

	// Check if we can rely on a slab to initialize the message
	if(sizeof(msg_t) + size <= SLAB_MSG_SIZE) {
		*msg = get_msg_from_slab();
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

void dump_msg_content(msg_t *msg) {
	printf("\tsender: %u\n", gid_to_int(msg->sender));
	printf("\treceiver: %u\n", gid_to_int(msg->sender));
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
unsigned int mark_to_gid(unsigned long long mark) {
	double z = (double)mark;
        double w = floor( (sqrt( 8 * z + 1) - 1) / 2.0);
        double t = ( w*w + w ) / 2.0;
        double y = z - t;
        double x = w - y;

	return (int)x;
}

void validate_msg(msg_t *msg) {
	assert(gid_to_int(msg->sender) <= n_prc_tot);
	assert(gid_to_int(msg->receiver) <= n_prc_tot);
	assert(msg->message_kind == positive || msg->message_kind == negative || msg->message_kind == control);
	assert(mark_to_gid(msg->mark) <= n_prc_tot);
	assert(mark_to_gid(msg->rendezvous_mark) <= n_prc_tot);
	assert(msg->type < MAX_VALUE_CONTROL);
	assert(msg->alloc_tid < n_cores);
}
#endif

