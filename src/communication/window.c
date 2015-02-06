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
* @file window.c
* @brief This module implements the acking windows used to notify other instances
*        of the simulator that a set of messages has been received and correctly
*        enqueued.
* @author Francesco Quaglia
*
*/


#include <stdlib.h>
#include <float.h>

#include <core/core.h>
#include <core/timer.h>
#include <gvt/gvt.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <scheduler/process.h>





// This was used by the old GVT. This is no longer needed on a multicore system.
// I'm leaving this here because we are currently rethinking the distributed GVT algorithm,
// so this might be useful as a reference.
#if 0



/// Circular buffer for acking messages
static wnd_buffer buffer[N_KER_MAX];


/// Total number of acked messages (per simulation kernel)
static int acked_messages[N_KER_MAX];


/// Timer for acking messages
timer ack_timer;



/**
* Initialization of the window subsystem
*
* @author Francesco Quaglia
*/
void windows_init(void) {
	register unsigned int i, k;

	// Buffer initialization for windowed acking subsystem
	for (k = 0; k < n_ker; k++) {
		buffer[k].first_wnd = buffer[k].next_wnd = 0;
		for (i = 0; i < WND_BUFFER_LENGTH; i++) {
			buffer[k].wnd_buffer[i].msg_number= 0;
			buffer[k].wnd_buffer[i].min_timestamp = -1.;
		}
	}
}






/**
* This function start the ack timer
*
* @author Roberto Vitali
*
*/
void start_ack_timer(void) {
	timer_start(ack_timer);
}




/**
* This function registers a message in the buffer, until the corresponding ack is received
*
* @author Francesco Quaglia
* @author Roberto Vitali
*
*/
void register_msg(msg_t *msg) {
	register unsigned int k;

	// Operate on the local buffer targeted at a particular destination kernel
	k = GidToKernel(msg->receiver);

	register unsigned int next = buffer[k].next_wnd;

	if (buffer[k].wnd_buffer[next].msg_number == 0) { // Message box is empty
		buffer[k].wnd_buffer[next].msg_number++;
		buffer[k].wnd_buffer[next].min_timestamp = msg->timestamp;
	} else if (buffer[k].wnd_buffer[next].msg_number < WINDOW_DIMENSION) { // // Message box is not empty
		buffer[k].wnd_buffer[next].msg_number++;
		// Does the new message have a timestamp less than the one registered in the box?
		if (((buffer[k].wnd_buffer[next].min_timestamp - 1) < DBL_EPSILON) || msg->timestamp < buffer[k].wnd_buffer[next].min_timestamp)
			buffer[k].wnd_buffer[next].min_timestamp = msg->timestamp;
	}

	// If the message box dimesion has been reached
	if (buffer[k].wnd_buffer[next].msg_number == WINDOW_DIMENSION) {

		buffer[k].next_wnd++;
		buffer[k].next_wnd %= WND_BUFFER_LENGTH;

		if (buffer[k].next_wnd == buffer[k].first_wnd) {
			rootsim_error(true, "Error, ack window overflow\n");
		}
	}

}




#if 0
/**
* This function update the ack window upon new acks reception
*
* @author Francesco Quaglia
* @author Roberto Vitali
*/
void receive_ack(void) {
	int i, flag = 0;
	int ack_number, from_kernel;
	MPI_Status status;

	comm_probe(MPI_ANY_SOURCE, MSG_GVT_ACK, MPI_COMM_WORLD, &flag, &status);

	// If we can receive an ack
	if (flag != 0) {
		from_kernel = status.MPI_SOURCE;
		// do receive it!
		comm_recv(&ack_number,sizeof(ack_number),MPI_CHAR,from_kernel,MSG_GVT_ACK,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

		gvt_messages_sent[from_kernel] -= ack_number;
		if(gvt_messages_sent[from_kernel] < 0) {

			comm_abort(MPI_COMM_WORLD, 0);
			rootsim_error(true, "Negative messages sent. gvt_messages_sent[%d] = %d\n\n", from_kernel, gvt_messages_sent[from_kernel]);

		}
		acked_messages[from_kernel] += ack_number;

		int next = buffer[from_kernel].next_wnd;
		int first = buffer[from_kernel].first_wnd;

		// This variable tells what's the last cell to be cancelled
		int to_delete = acked_messages[from_kernel] / WINDOW_DIMENSION;

		if ((first + to_delete) % WND_BUFFER_LENGTH == next &&
		    acked_messages[from_kernel] % WINDOW_DIMENSION > buffer[from_kernel].wnd_buffer[next].msg_number) {
			rootsim_error(true, "Received more ack than the number of sent messages\n");
		}

		for (i = first; i < first + to_delete; i++) {
			buffer[from_kernel].wnd_buffer[i % WND_BUFFER_LENGTH].msg_number = 0;
			buffer[from_kernel].wnd_buffer[i % WND_BUFFER_LENGTH].min_timestamp = -1;
		}

		buffer[from_kernel].first_wnd = i % WND_BUFFER_LENGTH;

		if(acked_messages[from_kernel] > 0)
			acked_messages[from_kernel] = acked_messages[from_kernel] % WINDOW_DIMENSION;
	}
}




/**
* This function sends ack to the kernels from which messages are received
*
* @author Francesco Quaglia
*/
void send_forced_ack(void) {
	register unsigned int i;

	int delta_ack_timer = timer_value(ack_timer);

	if (abs(delta_ack_timer) > (time_t) (GVT_ACK_TIME_PERIOD - 1)) {
		timer_restart(ack_timer);
		for (i = 0; i < n_ker; i++) {
			if (i != kid && gvt_messages_rcvd[i]!=0) {
				// Send the total number of messages which were received from kernel i.
				// The i-th kernel will receive this number in receive_ack(), storing it
				// into the ack_number variable
				comm_basic_send( (char*)&gvt_messages_rcvd[i] , sizeof(int), MPI_CHAR, i, MSG_GVT_ACK, MPI_COMM_WORLD);
				gvt_messages_rcvd[i] = 0;
			}
		}
	}
}
#endif





/**
* This function retrieve the min timestamp in the kernel considering both the event in the LP's queues and the messages sent by the kernel but not already acked
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/
simtime_t local_min_timestamp(void) {
	simtime_t min = INFTY, tmp;
	register unsigned int i, k;

	simtime_t min2;

	// Computation of the min timestamp in the bottom half queue for the locally hosted LPs
/*	for (i = 0; i < n_prc; i++) {
		spin_lock(&LPS[i]->lock);
		tmp = LPS[i]->min_bh_ts;
		spin_unlock(&LPS[i]->lock);
		if (tmp < min) {
			min = tmp;
		}
		//~ printf("--%d: bh = %f , min = %f\n", i, tmp, min);
    	}
*/
	// Computation of non-acked messages minimum timestamp
	for(k = 0; k < n_ker; k++) {
		for(i = buffer[k].first_wnd; i <= buffer[k].next_wnd; i++) {
			tmp = buffer[k].wnd_buffer[i].min_timestamp;
			if(D_DIFFER(tmp, -1.0) && tmp < min) {
				min = tmp;
			}
			//~ printf("%d: non-acked = %f, min = %f\n", i, tmp, min);
		}
	}

	min2 = min;

	// Comparison between the obtained minimum timestamp and the timestamp of the min bound of all the woker threads
/*	for (i = 0; i < n_cores; i++) {
		tmp = min_bound_per_thread[i];
		if (tmp < min) {
			min = tmp;
		}
	}
*/	for (i = 0; i < n_prc; i++) {
		tmp = last_event_timestamp(i);
		if (tmp < min2) {
			min2 = tmp;
		}
		//~ printf("%d: lvt = %f, min = %f\n", i, tmp, min);
	}

	return min2;
}

#endif /* if 0 */

