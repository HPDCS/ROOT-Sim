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
* @file control.c
* @brief
* @author
*/


#include <stdbool.h>

#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <core/core.h>
#include <communication/communication.h>
#include <mm/dymelor.h>
#include <datatypes/list.h>
#include <gvt/gvt.h>

#ifdef HAVE_CROSS_STATE
void unblock_synchronized_objects(unsigned int lid) {
	unsigned int i;
	msg_t control_msg;

	for(i = 1; i <= LPS[lid]->ECS_index; i++) {
		bzero(&control_msg, sizeof(msg_t));
		control_msg.sender = LidToGid(lid);
		control_msg.receiver = LidToGid(LPS[lid]->ECS_synch_table[i]);
		control_msg.type = RENDEZVOUS_UNBLOCK;
		control_msg.timestamp = lvt(lid);
		control_msg.send_time = lvt(lid);
		control_msg.message_kind = positive;
		control_msg.rendezvous_mark = LPS[lid]->wait_on_rendezvous;

		Send(&control_msg);
	}

	LPS[lid]->wait_on_rendezvous = 0;
	LPS[lid]->ECS_index = 0;
}
#endif

void rollback_control_message(unsigned int lid, simtime_t simtime) {
	msg_t control_antimessage;

	msg_t *msg, *msg_prev;

	if(list_empty(LPS[lid]->rendezvous_queue)) {
		return;
	}

	msg = list_tail(LPS[lid]->rendezvous_queue);
	while(msg != NULL && msg->timestamp > simtime) {

		// Control antimessage
		bzero(&control_antimessage, sizeof(msg_t));
		control_antimessage.type = RENDEZVOUS_ROLLBACK;
                control_antimessage.sender = msg->receiver;
                control_antimessage.receiver = msg->sender;
                control_antimessage.timestamp = msg->timestamp;
                control_antimessage.send_time = msg->send_time;
		control_antimessage.rendezvous_mark = msg->rendezvous_mark;
                control_antimessage.message_kind = other;

                Send(&control_antimessage);

		msg_prev = list_prev(msg);
		list_delete_by_content(lid, LPS[lid]->rendezvous_queue, msg);
		msg = msg_prev;
	}
}

// return false if the antimessage is recognized (and processed) as a control antimessage
bool anti_control_message(msg_t * msg) {

	#ifndef HAVE_CROSS_STATE
	(void)msg;
	#else
	msg_t *old_rendezvous ;

	if(msg->type == RENDEZVOUS_ROLLBACK) {

		unsigned int lid_receiver = msg->receiver;

		//Check if a relative message exists
		//TODO non serve andare indietro più del tempo di rendezvous_rollback
		old_rendezvous = list_tail(LPS[lid_receiver]->queue_in);
		while(old_rendezvous != NULL && old_rendezvous->rendezvous_mark != msg->rendezvous_mark) {
			old_rendezvous = list_prev(old_rendezvous);
		}

		if(old_rendezvous == NULL) {
			return false;
		}

		//If this event is in the past
		if(old_rendezvous->timestamp <= lvt(lid_receiver)) {

			//Set LP->bound to the message that caused ECS
			LPS[lid_receiver]->bound = old_rendezvous;
	                while (LPS[lid_receiver]->bound != NULL && LPS[lid_receiver]->bound->timestamp >= old_rendezvous->timestamp) {
	                	if(list_prev(LPS[lid_receiver]->bound) == NULL)
					break;
				LPS[lid_receiver]->bound = list_prev(LPS[lid_receiver]->bound);
	        	}

	                LPS[lid_receiver]->state = LP_STATE_ROLLBACK;
		}

		#ifdef HAVE_GLP_SCH_MODUL
                else{
			if(GLPS[LPS[lid_receiver]->current_group]->lvt!= NULL)
				printf("ERRORE LP:%d S:%d anti-control message T:%f after LP-lvt:%f GLP-lvt:%f \n",
			 	     lid_receiver,old_rendezvous->sender,old_rendezvous->timestamp, lvt(lid_receiver), GLPS[LPS[lid_receiver]->current_group]->lvt->timestamp
			     	 );
			else
				 printf("ERRORE LP:%d S:%d anti-control message T:%f after LP-lvt:%f \n",
                                     lid_receiver,old_rendezvous->sender,old_rendezvous->timestamp, lvt(lid_receiver));
		}

		#endif

		old_rendezvous->rendezvous_mark = 0;

		//Reset ECS information
		if(LPS[lid_receiver]->wait_on_rendezvous == msg->rendezvous_mark) {
			LPS[lid_receiver]->ECS_index = 0;
			LPS[lid_receiver]->wait_on_rendezvous = 0;
		}

		return false;
	}
	#endif

	return true;
}

// return true if the control message should be reprocessed during silent exceution
bool reprocess_control_msg(msg_t *msg) {


	if(msg->type < MIN_VALUE_CONTROL) {
		return true;
	}

	return false;
}


// return true if the event must not be filtered here
bool receive_control_msg(msg_t *msg) {

	if(msg->type < MIN_VALUE_CONTROL || msg->type > MAX_VALUE_CONTROL) {
		return true;
	}

	#ifdef  HAVE_CROSS_STATE
	switch(msg->type) {

		case RENDEZVOUS_START:
			return true;

		case RENDEZVOUS_ACK:

//			printf("(%d) matched mark:%llu WOR:%llu state:%d from %d\n",msg->receiver, msg->rendezvous_mark,LPS[msg->receiver]->wait_on_rendezvous, LPS[msg->receiver]->state, msg->sender);

			if(	LPS[msg->receiver]->state == LP_STATE_ROLLBACK ||
				LPS[msg->receiver]->state == LP_STATE_SILENT_EXEC
			) {
				break;
			}

			if(LPS[msg->receiver]->wait_on_rendezvous == msg->rendezvous_mark) {
				LPS[msg->receiver]->state = LP_STATE_READY_FOR_SYNCH;
			}

			break;

		case RENDEZVOUS_UNBLOCK:

			if(	LPS[msg->receiver]->state == LP_STATE_ROLLBACK ||
				LPS[msg->receiver]->state == LP_STATE_SILENT_EXEC
			)  {
//				printf("(%d) discarding mark %llu\n",msg->receiver, msg->rendezvous_mark);
				break;
			}

			if(LPS[msg->receiver]->wait_on_rendezvous == msg->rendezvous_mark) {

//				printf("(%d) matched mark %llu from %d\n",msg->receiver, msg->rendezvous_mark, msg->sender);

				LPS[msg->receiver]->wait_on_rendezvous = 0;
			
			current_lp = GidToLid(msg->receiver);
			current_lvt = msg->timestamp;
			force_LP_checkpoint(current_lp);
			LogState(current_lp);
			current_lvt = INFTY;
			current_lp = IDLE_PROCESS;

				current_lp = msg->receiver;
				current_lvt = msg->timestamp;
				force_LP_checkpoint(current_lp);

				LogState(current_lp);

				current_lvt = INFTY;
				current_lp = IDLE_PROCESS;
			} else {
//				printf("(%d) wait on rendezvous: %llu - mark: %llu - unblock from %d\n",msg->receiver, LPS[msg->receiver]->wait_on_rendezvous, msg->rendezvous_mark, msg->sender);
			}
			break;

		case RENDEZVOUS_ROLLBACK:
			return true;

		default:
			rootsim_error(true, "Trying to handle a control message which is meaningless at receive time: %d\n", msg->type);

	}
	#endif

	return false;
}


// return true if must be passed to the LP
bool process_control_msg(msg_t *msg) {

	#ifdef HAVE_CROSS_STATE
	msg_t control_msg;
	#endif

	if(msg->type < MIN_VALUE_CONTROL || msg->type > MAX_VALUE_CONTROL) {
		return true;
	}

	#ifdef HAVE_CROSS_STATE
	switch(msg->type) {

		case RENDEZVOUS_START:

			list_insert(msg->receiver, LPS[msg->receiver]->rendezvous_queue, timestamp, msg);
			// Place this into input queue
			LPS[msg->receiver]->wait_on_rendezvous = msg->rendezvous_mark;

			LPS[msg->receiver]->state = LP_STATE_WAIT_FOR_UNBLOCK;
			bzero(&control_msg, sizeof(msg_t));
			control_msg.sender = msg->receiver;
			control_msg.receiver = msg->sender;
			control_msg.type = RENDEZVOUS_ACK;
			control_msg.timestamp = msg->timestamp;
			control_msg.send_time = msg->timestamp;
			control_msg.message_kind = positive;
			control_msg.rendezvous_mark = msg->rendezvous_mark;
			Send(&control_msg);
			break;
//TODO MN DEBUG case RENDEZVOUS_ACK and UNBLOCK  was noted
		/*case RENDEZVOUS_ACK:
			LPS[msg->receiver]->state = LP_STATE_READY_FOR_SYNCH;
			return true;

		case RENDEZVOUS_UNBLOCK:
			LPS[msg->receiver]->state = LP_STATE_READY;
			break;*/

		/* case NULL_LOG_MESSAGE: */
/* //			printf("[%d] process NULL_LOG_MESSAGE log-counter:%d \n",msg->receiver,GLPS[LPS[msg->receiver]->current_group]->counter_log); */
		/* 	break; */

		default:
			rootsim_error(true, "Trying to handle a control message which is meaningless at schedule time: %d\n", msg->type);

	}
	#endif

	return false;
}
