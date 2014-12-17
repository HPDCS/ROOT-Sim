/**
*			Copyright (C) 2008-2014 HPDCS Group
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
#include <scheduler/scheduler.h>
#include <communication/communication.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>




#ifdef TRACE_INPUT_QUEUE
void __trace_input_queue(char * file, int line, unsigned int lid) {
	
	msg_t *m = list_head(LPS[lid]->queue_in);
	bool passed_bound = false;
	
	printf("\n[QUEUE IN TRACE] (LP %d from %s:%d): ", lid, file, line);
	while(m != NULL) {
		printf("%f (%d, %p), type: %d, channel: %d\n", m->timestamp, m->sender, m, m->type, ((event_content_type *)(m->event_content))->channel);
		if(LPS[lid]->bound != NULL) {
			if(LPS[lid]->bound == m) {
				passed_bound = true;
			}
			if(m->timestamp > LPS[lid]->bound->timestamp && !passed_bound) {
				printf("[ERROR IN BOUND], ");
			}
		}
		m = list_next(m);
	}
	printf("bound: %p (%f)\n", LPS[lid]->bound, (LPS[lid]->bound != NULL ? LPS[lid]->bound->timestamp : -1.0));
}
#endif




/**
* This function returns the timestamp of the last executed event
*
* @author Francesco Quaglia
*
* @param lid The Light Process id
* @return The timestamp of the last executed event
*/
simtime_t last_event_timestamp(unsigned int lid) {
	simtime_t ret;

	if (LPS[lid]->bound != NULL) {
		ret = LPS[lid]->bound->timestamp;
	} else {
		// Ok, no bound: this means that the LP has not even executed INIT. We force the timestamp to 0!
		ret = 0.0;
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

//#ifdef FINE_GRAIN_DEBUG
//	printf("[NEXT EVENT] Searching for an event for LP %u, its bound is %p (timestamp %f)\n", id, LPS[id]->bound, (LPS[id]->bound != NULL ? LPS[id]->bound->timestamp : 0.0));
//#endif

	// The bound can be NULL in the first execution or if it has gone back	
	if (LPS[id]->bound == NULL && !list_empty(LPS[id]->queue_in)) {
		ret = list_head(LPS[id]->queue_in)->timestamp;
	} else {
		evt = list_next(LPS[id]->bound);
		if(evt != NULL) {
			ret = evt->timestamp;
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

	#ifdef FINE_GRAIN_DEBUG
	printf("[ADVANCE TO NEXT EVENT] LP %d updates is bound\n", lid);
	#endif

	if (LPS[lid]->bound == NULL) {
		if (!list_empty(LPS[lid]->queue_in)) {
			LPS[lid]->bound = list_head(LPS[lid]->queue_in);
			#ifdef TRACE_INPUT_QUEUE
			trace_input_queue(lid);
			#endif
		} else {
			return NULL;
		}
	} else {
		if (list_next(LPS[lid]->bound) != NULL) {
			LPS[lid]->bound = list_next(LPS[lid]->bound);
			#ifdef TRACE_INPUT_QUEUE
			trace_input_queue(lid);
			#endif
		} else {
			return NULL;
		}
	}

	return LPS[lid]->bound;
}






/**
* This function advance the pointer to the last correctly executed event,
*  it is called right before the execution of it
*
* @author Francesco Quaglia
*
* @param lid The Light Process id
* @return The pointer to the last correctly executed event
*/
msg_t *get_last_event(unsigned int lid) {
	return LPS[lid]->bound;
}










/**
* This function removes one element from the outgoing queue
*
* @author Francesco Quaglia
*
* @param p The element to be removed
* @return The event right after the one removed if it exhist, NULL otherwise
*/
msg_t *free_queue_out_elem(unsigned int lid, msg_t *msg) {

	list_delete_by_content(LPS[lid]->queue_out, msg);

	// TODO: a che serve quello che viene restituito qui?
	return NULL;
}




void insert_bottom_half(msg_t *msg) {
	
	unsigned int lid = GidToLid(msg->receiver);
	
	spin_lock(&LPS[lid]->lock);
	(void)list_insert_tail(LPS[lid]->bottom_halves, msg);
	spin_unlock(&LPS[lid]->lock);
}


/**
* Process bottom halves received by all the LPs hosted by the current KLT
*
* @author Alessandro Pellegrini
*/
void process_bottom_halves(void) {
	register unsigned int i;
	unsigned int lid_receiver;
	msg_t *msg;
	msg_t *msg_ptr;
	list(msg_t) processing;
	
	for(i = 0; i < n_prc_per_thread; i++) {
		
		spin_lock(&LPS_bound[i]->lock);
		processing = LPS_bound[i]->bottom_halves;
		LPS_bound[i]->bottom_halves = new_list(msg_t);
		spin_unlock(&LPS_bound[i]->lock);

		while(!list_empty(processing)) {
			msg = list_head(processing);
			
			lid_receiver = msg->receiver;

			if(!receive_control_msg(msg)) {
				goto expunge_msg;
			}

			// Check if it is an anti control message
			if(msg->is_antimessage == other) {
				if(!anti_control_message(msg)) {
					goto expunge_msg;
				}
			}

			switch (msg->is_antimessage) {

				case true:

					// Check if it is an anti control message
//					if(anti_control_message(msg)) {
//						goto expunge_msg;
//					}
					
					
					// TODO: cambiare la macro list_delete per gestire qualsiasi
					// tipo di chiave ed utilizzarla qui invece della ricerca manuale
					//list_delete(LPS[lid_receiver]->queue_in, mark, msg->mark);
					msg_ptr = list_head(LPS[lid_receiver]->queue_in);
					while(msg_ptr != NULL && msg_ptr->mark != msg->mark) {
						msg_ptr = list_next(msg_ptr);
					}
					
					if(msg_ptr == NULL) {
						rootsim_error(false, "LP %d Received an antimessage with mark %llu at LP %u from LP %u, but no such mark found in the input queue!\n", LPS_bound[i]->lid, msg->mark, msg->receiver, msg->sender);
						printf("Message Content:"
							"sender: %d\n"
							"receiver: %d\n"
							"type: %d\n"
							"timestamp: %f\n"
							"send time: %f\n"
							"is antimessage %d\n"
							"mark: %llu\n"
							"rendezvous mark %llu\n",
							msg->sender,
							msg->receiver,
							msg->type,
							msg->timestamp,
							msg->send_time,
							msg->is_antimessage,
							msg->mark,
							msg->rendezvous_mark);
						abort();
					} else {
						
						#ifdef FINE_GRAIN_DEBUG
						printf("[ANTIMESSAGE] Deleting event for LP %d at time %f ptr %p due to antimessage sent by %d at time %f", msg_ptr->receiver, msg_ptr->timestamp, list_container_of(msg_ptr), msg->sender, msg->timestamp);
						#endif

						// If the antimessage is in the past, we have to rollback
						if(msg->timestamp <= lvt(lid_receiver)) {

							LPS[lid_receiver]->bound = msg_ptr;
							while (LPS[lid_receiver]->bound != NULL && LPS[lid_receiver]->bound->timestamp >= msg_ptr->timestamp) {
								LPS[lid_receiver]->bound = list_prev(LPS[lid_receiver]->bound);
							}
							LPS[lid_receiver]->state = LP_STATE_ROLLBACK;
							#ifdef FINE_GRAIN_DEBUG
							printf(", this rollbacks to time %f", LPS[lid_receiver]->bound->timestamp);
							#endif
							#ifdef TRACE_INPUT_QUEUE
							trace_input_queue(lid_receiver);
							#endif

						}
						#ifdef FINE_GRAIN_DEBUG
						printf("\n");
						#endif

						list_delete_by_content(LPS[lid_receiver]->queue_in, msg_ptr);

						#ifdef TRACE_INPUT_QUEUE
						trace_input_queue(lid_receiver);
						#endif
					}
					
					break;

				case false:
				
					msg = list_insert(LPS[lid_receiver]->queue_in, timestamp, msg);
									
					#ifdef FINE_GRAIN_DEBUG
					printf("[BOTTOM HALVES] LP %d receives from %d at time %f a positive message placed in %p", msg->receiver, msg->sender, msg->timestamp, msg);
					#endif
					#ifdef TRACE_INPUT_QUEUE
					trace_input_queue(lid_receiver);
					#endif
					
					// Check if we've just inserted an out-of-order event
					if(LPS[lid_receiver]->bound != NULL) {
						if(msg->timestamp < lvt(lid_receiver)) {
							LPS[lid_receiver]->bound = msg;
							while ((LPS[lid_receiver]->bound != NULL) && LPS[lid_receiver]->bound->timestamp >= msg->timestamp) {
								LPS[lid_receiver]->bound = list_prev(LPS[lid_receiver]->bound);
							}
							LPS[lid_receiver]->state = LP_STATE_ROLLBACK;
							LPS[lid_receiver]->state = LP_STATE_ROLLBACK;

							#ifdef TRACE_INPUT_QUEUE
							trace_input_queue(lid_receiver);
							#endif

							#ifdef FINE_GRAIN_DEBUG
							printf(", this rollbacks to time %f", LPS[lid_receiver]->bound->timestamp);
							#endif

						}
					}
					#ifdef FINE_GRAIN_DEBUG	
					printf("\n");
					#endif
					break;

				default:
					rootsim_error(true, "Received a message which is neither positive nor negative. Aborting...\n");
			}
		
		    expunge_msg:
			list_pop(processing);
		}

		rsfree(processing);
	}
//	flush_in_transit_bound();
}


/**
* This function implements the operations needed to let a process send a message locally.
* It is called by the function Send: this is the reason why it has been decoupled from
* Message Checking.
*
* bound points to the last messagge correctly computed. Upon receiving a straggler or an
* annihilator, bound will point to the immediately preceding message (with respect to the
* timestamps).
* In this case, this means we have to rollback the state, and will point us to the
* event from which to restart.
* That pointer is rolled back in this function, while is updated in NextEvent.
*
* Depending on the message received, recipient process' in queues are updated:
*   - if it is a positive message, it is enqueued, ordered with respect to ascending
*     timestamps; if the timestamp is lower than the receiver's lvt (i.e., it is a
*     straggler), bound's value is updated;
*   - if it is a negative message, the corresponding positive message is extracted from
*     the queue. If it is a straggler as well, or if it's timestamp is the same as the
*     one of the last correctly executed event (bound), the bound is updated.
*
* CAVEAT: When entering MessageChecking, when DataUpdating is called for the first time,
*         we set bound->timestamp = lvt.
*         If more than one message for the same process is read in sequence, it may happen
*         that bound gets updated so that is points to the last CORRECTLY processed event.
*         This is the main reason why message's timestamp is checked against current bound->timestamp,
*	  rather than lvt.
*
*	  Not doing this, the following (uncorrect) pattern might be encountered:
*	  ...|3|5|20|30|40
*         lvt = 30, bound->tms = 30;
*
*         msg.rms = 15 is received, so: ...|3|5|15|20|30|40
*         since msg.tms < lvt we have bount->tms = prec(15) = 5
*
*	  msg.tms = 25 is received, so: ...|3|5|15|20|25|30|40
*	  since msg.tms < lvt we have bound-> tms = prec(25) = 20
*
*	  Upon rollback execution, lvt will be (erroneously) restored to 20 instead
*         of the correct value lvt = 5;
*
* @author Francesco Quaglia
*
* @param msg The incoming event to handle by the current kernel
*/
// TODO: this is no longer needed!
/*void DataUpdating(msg_t *msg) {

	unsigned int lid_receiver;

	lid_receiver = GidToLid(msg->receiver);

	switch (msg->is_antimessage) {

		case true:

			// Remove the msg with the same exact mark
			list_delete(LPS[lid_receiver]->queue_in, mark, msg->mark);
			break;

		case false:
//			list_insert(LPS[lid_receiver]->queue_in, timestamp, msg);
			insert_bottom_half(lid_receiver, msg);
			break;

		default:
			rootsim_error(true, "Received a message which is neither positive nor negative. Aborting...\n");
	}



	// TODO: this logic must be rewritten!!!

	/// If it has been arrived a straggler (either positive or negative)
	/// The bound is updated, it now points to the event with timestamp immediately smaller

	if ((LPS[lid_receiver]->bound) && (msg->timestamp < LPS[lid_receiver]->bound->timestamp)) {
	// The message is a straggler message

		p = LPS[lid_receiver]->bound;	

		// TODO: rewrite all this!
		while ( (p) && (p->timestamp >= msg->timestamp) ) {
			p->min_timestamp = -1;
			p = p->prec;			
		}
		LPS[lid_receiver]->bound = p;		

		LPS[lid_receiver]->count_stragglers++;

		send_antimessages( lid_receiver );
	}

}*/




/**
* This function generates a mark value that is unique w.r.t. the previous values for each Logical Process.
* It is based on the Cantor Pairing Function, which maps 2 naturals to a single natural.
* The two naturals are the LP gid (which is unique in the system) and a non decreasing number
* which gets incremented (on a per-LP basis) upon each function call.
* It's fast to calculate the mark, it's not fast to invert it. Therefore, inversion is not
* supported at all in the code.
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
