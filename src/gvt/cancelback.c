/**
*			Copyright (C) 2008-2016 HPDCS Group
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
* @file cancelback.c
* @brief This module encapsulates all the logic behind the Cancelback protocol
* @author Simone Riccardelli
*/


#include <arch/atomic.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <mm/dymelor.h>

#define MSG_TO_SELF(a) ((a)->sender == (a)->receiver)

bool is_memory_limit_exceeded() {

    long rss = 0L;
	FILE* fp = NULL;
	
	if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
		return false;
	
	if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
	{
		fclose( fp );
		return false;
	}
	
	fclose( fp );

	printf("Number of pages used by simulation: %zu\n", rss);

    return rss > get_cancelback_threshold();
}

inline void synch_for_cancelback() {
	
	int i = 0;
	for (; i < n_prc_per_thread; i++) {
       
	   	LPS_bound[i]->state_to_resume = LPS_bound[i]->state; 
		LPS_bound[i]->state = LP_STATE_SYNCH_FOR_CANCELBACK;
	}
}

inline bool is_cancelback_in_past(unsigned int lid) {

    return LPS[lid]->state == LP_STATE_CANCELBACK && LPS[lid]->bound->timestamp <= lvt(lid);
}

void send_backward_messages(unsigned int lid) {
	msg_t *cb_msg,
		  *cb_msg_next;
	
	msg_t msg;

	printf("\033[1;32mSending backward Cancelback messages!\033[0m\n");

	// cb_msg = list_head(LPS[lid]->queue_in);
	// while(cb_msg != NULL && cb_msg->timestamp < LPS[lid]->bound->timestamp)
    //    cb_msg = list_next(cb_msg);

	if (LPS[lid]->bound == NULL) {
		stylized_printf("[NULL BOUND] Could not free memory through Cancelback protocol!\n", RED, true);
		return;
	}

	cb_msg = list_next(LPS[lid]->bound);

	int del_count = 0;
	while (cb_msg != NULL) {
		bzero(&msg, sizeof(msg_t));
		msg.sender = cb_msg->receiver;
		msg.receiver = cb_msg->sender;
		msg.timestamp = cb_msg->timestamp;
		msg.send_time = cb_msg->send_time;
		msg.mark = cb_msg->mark;
		msg.message_kind = positive_cb;
		
		if (msg.sender != lid) {
			rootsim_error(true, "LP %u sending a message for which it is not the sender!\n", lid);
		}

		Send(&msg);

		cb_msg_next = list_next(cb_msg);
		list_delete_by_content(LPS[lid]->queue_in, cb_msg);
		cb_msg = cb_msg_next;

		del_count++;
	}

	printf("\033[1;33mInbound\033[0m messages deleted: %d\n", del_count);
}

void send_forward_messages(unsigned int lid) {
	msg_hdr_t *cb_msg,
              *cb_msg_next;

	msg_t msg;	

	printf("\033[1;32mSending forward Cancelback messages!\033[0m\n");

	cb_msg = list_head(LPS[lid]->queue_out);
	while(cb_msg != NULL && cb_msg->send_time < LPS[lid]->bound->timestamp)
        cb_msg = list_next(cb_msg);

    int del_count = 0;
	while(cb_msg != NULL) {
		bzero(&msg, sizeof(msg_t));
		msg.sender = cb_msg->sender;
		msg.receiver = cb_msg->receiver;
		msg.timestamp = cb_msg->timestamp;
		msg.send_time = cb_msg->send_time;
		msg.mark = cb_msg->mark;
		msg.message_kind = negative_cb;

		if (msg.sender != lid) {
			rootsim_error(true, "LP %u sending a message for which it is not the sender!\n", lid);
		}

		Send(&msg);

		cb_msg_next = list_next(cb_msg);
		list_delete_by_content(LPS[lid]->queue_out, cb_msg);
		cb_msg = cb_msg_next;

		del_count++;
	}

	printf("\033[1;33mOutbound\033[0m messages deleted: %d\n", del_count);
}

void send_cancelback_messages(unsigned int lid) {

	if(LPS[lid]->state != LP_STATE_CANCELBACK) {
		rootsim_error(false, "I'm asked to execute Cancelback on LP %d, but its state has not been set properly. Ignoring...\n", LidToGid(lid));
	} else if (LPS[lid]->bound == NULL) {
		stylized_printf("Cancelback bound has been deleted. Halting Cancelback protocol...\n", RED, true);
	}

	LPS[lid]->state = LP_STATE_READY;

	// According to the original publication, the item(s) to "cancelback" are chosen non-deterministically
	// Hence, here we simply choose the longest queue as the one from which we select the items to purge
	if (list_sizeof(LPS[lid]->queue_in) > list_sizeof(LPS[lid]->queue_out)) {
		send_backward_messages(lid);
	} else {
		send_forward_messages(lid);
	}
}

/** Execute Cancelback protocol */
bool attempt_cancelback(simtime_t new_gvt) {

    printf("Starting Cancelback protocol...\n");

    simtime_t max_lvt = DBL_MIN;
    unsigned int cb_lid;

    register unsigned int i;
    for(i = 0; i < n_prc_per_thread; i++) {

        unsigned int lid = LPS_bound[i]->lid;
        simtime_t current_lvt = lvt(lid);

        if (current_lvt > max_lvt) {
            max_lvt = current_lvt;
            cb_lid = lid;
        }
    }

	// Just a sanity check
    if (!list_empty(LPS[cb_lid]->queue_in)){
        msg_t* bound = list_head(LPS[cb_lid]->queue_in);
        while (bound != NULL && bound->timestamp < new_gvt) {
            bound = list_next(bound);
		}

		if (bound == NULL) {
			stylized_printf("[NULL BOUND] Could not free memory through Cancelback protocol!\n", RED, true);
		} else {
			LPS[cb_lid]->bound = bound;
        	LPS[cb_lid]->state = LP_STATE_CANCELBACK;
		}

    } else {
		stylized_printf("[EMPTY QUEUE] Could not free memory through Cancelback protocol!\n", RED, true);
	}

	return LPS[cb_lid]->state == LP_STATE_CANCELBACK;
}

int num_unprocessed_msgs(unsigned int lid) {
	
	int count = 0;
	msg_t* lp_bound = LPS[lid]->bound;
	
	while(lp_bound != NULL) {
		lp_bound = list_next(lp_bound);
		count++;
	}

	return (count - 1) < 0 ? count : (count - 1);
}
