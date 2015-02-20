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
#include <scheduler/process.h>
#include <datatypes/list.h>
#include <mm/dymelor.h>

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
//	windows_init();
}


/**
* This function finalizes the communication subsystem
*
* @author Roberto Vitali
*
*/
void communication_fini(void) {
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
	msg_t event;

	// In Silent execution, we do not send again already sent messages
	if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC) {
		return;
	}

	// Check whether the destination LP is out of range
	if(gid_receiver > n_prc_tot - 1) {	// It's unsigned, so no need to check whether it's < 0
		rootsim_error(false, "Warning: the destination LP %d is out of range. The event has been ignored\n", gid_receiver);
		return;
	}

	// Check if the associated timestamp is negative
	if(timestamp < lvt(current_lp)) {
		rootsim_error(true, "LP %d is trying to generate an event (type %d) to %d in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n", current_lp, event_type, gid_receiver, lvt(current_lp), timestamp);
	}

	// Copy all the information into the event structure
	bzero(&event, sizeof(msg_t));
	event.sender = LidToGid(current_lp);
	event.receiver = gid_receiver;
	event.type = event_type;
	event.timestamp = timestamp;
	event.send_time = lvt(current_lp);
	event.message_kind = positive;
	event.mark = generate_mark(current_lp);
	event.size = event_size;

	if(event.type == RENDEZVOUS_START) {
		event.rendezvous_mark = current_evt->rendezvous_mark;
	}

	if(event_size > MAX_EVENT_SIZE) {
		rootsim_error(true, "Event size exceeds MAX_EVENT_SIZE: %d\n", event_size);
	}

	if (event_content != NULL) {
		memcpy(&event.event_content, event_content, event_size);
	}

	insert_outgoing_msg(&event);
}



/**
* This function send all the antimessages for a certain LP.
* After the antimessage is sent, the header is removed from the output queue!
*
* @author Francesco Quaglia
*
* @param lid The Logical Process Id
*/
void send_antimessages(unsigned int lid, simtime_t after_simtime) {
	msg_hdr_t *anti_msg,
		  *anti_msg_next;

	msg_t msg;

	if (list_empty(LPS[lid]->queue_out))
		return;

	// Get the first message header with a timestamp <= after_simtime
	anti_msg = list_tail(LPS[lid]->queue_out);
	while(anti_msg != NULL && anti_msg->send_time > after_simtime)
		anti_msg = list_prev(anti_msg);

	// The next event is the first event with a sendtime > after_simtime, if any
	if(anti_msg == NULL)
		return;

	anti_msg = list_next(anti_msg);

	// Now send all antimessages
	while(anti_msg != NULL) {
		bzero(&msg, sizeof(msg_t));
		msg.sender = anti_msg->sender;
		msg.receiver = anti_msg->receiver;
		msg.timestamp = anti_msg->timestamp;
		msg.send_time = anti_msg->send_time;
		msg.mark = anti_msg->mark;
		msg.message_kind = negative;

		Send(&msg);

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

//	return MPI_Finalize();
	return 0;

}



/**
* Send a message. If it's scheduled to a local LP, update its queue, otherwise
* ask MPI to deliver it to the hosting kernel instance.
*
* @author Francesco Quaglia
*/
void Send(msg_t *msg) {
	// Check whether the message recepient is local or remote
	if(GidToKernel(msg->receiver) == kid) { // is local
		insert_bottom_half(msg);
	} else { // is remote
		rootsim_error(true, "Calling an operation not yet reimplemented, this should never happen!\n", __FILE__, __LINE__);
	}
}



/**
* This function allows kernels to receive time barrier from other instances and calculate
* the maximum value.
*
* @author Francesco Quaglia
*/
/*simtime_t receive_time_barrier(simtime_t max) {
	register unsigned int i;
	simtime_t tmp;

	// Compute the maximum time barrier
	for (i = 0; i < n_ker; i++) {
		if  (i != kid) {
			(void)comm_recv((char*)&tmp, sizeof(simtime_t), MPI_CHAR, (int)i, MSG_TIME_BARRIER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			if (D_DIFFER(max, -1) || ((tmp - 1) > DBL_EPSILON && tmp > max))
				max = tmp;
		}
	}

	return max;
}*/




#if 0
/**
*
*
* @author Francesco Quaglia
*/
static bool pending_msg(void) {

    int flag = 0;

    (void)comm_probe(MPI_ANY_SOURCE, MSG_EVENT, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
    return (bool)flag;

}


/**
*
*
* @author Francesco Quaglia
*/
int messages_checking(void) {
	msg_t msg;
	int at_least_one_message = 0;


	// Are we in GVT computation phase?
	// TODO a cosa serve questa parte qui sotto?!
//	if (ComputingGvt() == false)
//		send_forced_ack();


	// Check whether we have received any ack message
	receive_ack();

	// If we have pending messages, we process all of them in order to update the queues
 	while(pending_msg()){

		at_least_one_message = 1;

		// Receive the message
	    	(void)comm_recv((char*)&msg, sizeof(msg_t), MPI_CHAR, MPI_ANY_SOURCE, MSG_EVENT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		// For GVT Operations
		gvt_messages_rcvd[GidToKernel(msg.sender)] += 1;


		// Update the queues
//		DataUpdating(&msg);

	}

	// Did we receive at least one message?
	return at_least_one_message;

}
#endif





/**
*
*
* @author Francesco Quaglia
*/
void insert_outgoing_msg(msg_t *msg) {

	// Sanity check
	if(LPS[current_lp]->outgoing_buffer.size == MAX_OUTGOING_MSG){
		rootsim_error(true, "Outgoing message queue full! Too many events scheduled by one single event. Aborting...");
	}

	// Message structure was declared on stack in ScheduleNewEvent: make a copy!
	LPS[current_lp]->outgoing_buffer.outgoing_msgs[LPS[current_lp]->outgoing_buffer.size++] = *msg;

	// Store the minimum timestamp of outgoing messages
	if(msg->timestamp < LPS[current_lp]->outgoing_buffer.min_in_transit[LPS[current_lp]->worker_thread]) {
		LPS[current_lp]->outgoing_buffer.min_in_transit[LPS[current_lp]->worker_thread] = msg->timestamp;
	}
}



void send_outgoing_msgs(unsigned int lid) {

	register unsigned int i = 0;
	msg_t *msg;
	msg_hdr_t msg_hdr;

	for(i = 0; i < LPS[lid]->outgoing_buffer.size; i++) {
		msg = &LPS[lid]->outgoing_buffer.outgoing_msgs[i];
		Send(msg);

		// Register the message in the sender's output queue, for antimessage management
		msg_hdr.sender = msg->sender;
		msg_hdr.receiver = msg->receiver;
		msg_hdr.timestamp = msg->timestamp;
		msg_hdr.send_time = msg->send_time;
		msg_hdr.mark = msg->mark;
		(void)list_insert(msg->sender, LPS[msg->sender]->queue_out, send_time, &msg_hdr);
	}

	LPS[lid]->outgoing_buffer.size = 0;
}
