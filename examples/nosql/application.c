#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ROOT-Sim.h>

#include "application.h"


#define LOOP 100000


void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *ptr) {
	
	int i;
	int j;

	event_content_type new_event_content;
	simtime_t timestamp;

	bzero(&new_event_content, sizeof(new_event_content));
	
	lp_state_type *state;
	state = (lp_state_type*)ptr;
	
	if(state != NULL) {
		state->lvt = now;
	}


	for(i = 0; i < LOOP; i++);
	
	switch(event_type) {
			
		case INIT:
		
			// Initialize the LP's state
			state = (lp_state_type *)malloc(sizeof(lp_state_type));
			if (state == NULL){
				printf("Out of memory!\n");
				exit(EXIT_FAILURE);
			}

			SetState(state);
			
			bzero(state, sizeof(lp_state_type));


			timestamp = (simtime_t) (20 * Random());
			ScheduleNewEvent(me, timestamp, START_TX, NULL, 0);

			break;

	
		case START_TX:

			state->residual_tx_ops = RandomRange(MIN_OP_COUNT, MAX_OP_COUNT);
			state->read_set_size = RandomRange(10, MAX_RS_SIZE);
//			state->read_set = malloc(sizeof(int) * state->read_set_size);
			
			timestamp = now + (simtime_t)Expent(TX_OP_ARRIVAL);

			ScheduleNewEvent(me, timestamp, TX_OP, NULL, 0);
	


			break;

			
		case TX_OP:


			state->residual_tx_ops--;
			timestamp= now + (simtime_t)Expent(TX_OP);

			if(state->residual_tx_ops > 0) {

				ScheduleNewEvent(me, timestamp, TX_OP, NULL, 0);

			} else {

				int recv = FindReceiver(TOPOLOGY_MESH);
				int recv2;
				do {
					recv2 = FindReceiver(TOPOLOGY_MESH);
				} while (recv == recv2);


				new_event_content.from = me;
				new_event_content.size = state->read_set_size;

				new_event_content.read_set_ptr = state->read_set;
				#ifndef ECS
				memcpy(new_event_content.read_set, state->read_set, sizeof(int) * state->read_set_size);
				#endif
				
				new_event_content.second = false;
//				ScheduleNewEvent(recv, timestamp, PREPARE, &new_event_content, sizeof(new_event_content));
				new_event_content.second = true;
				timestamp = now + (simtime_t)Expent(TX_OP);
				ScheduleNewEvent(recv2, timestamp, PREPARE, &new_event_content, sizeof(new_event_content));
				timestamp= now + (simtime_t)Expent(TX_OP);
				ScheduleNewEvent(me, timestamp, START_TX, NULL, 0);
			state->committed_tx++;
				
			}

			break;

        	case PREPARE:

			for(i = 0; i < event_content->size; i++) {
				#ifdef ECS
				event_content->read_set_ptr[i] = me;
				#else
				event_content->read_set[i] = me;
				#endif
			}

			timestamp= now + (simtime_t)Expent(50);

			if(event_content->second) {
				new_event_content.read_set_ptr = event_content->read_set_ptr;
				new_event_content.second = true;
			} else {
				new_event_content.second = false;
			}
//			ScheduleNewEvent(event_content->from, timestamp, COMMIT, &event_content, sizeof(new_event_content));
//
			break;



		case COMMIT:

			state->committed_tx++;

			if(event_content->second) {
				timestamp= now + (simtime_t)Expent(TX_OP);
				ScheduleNewEvent(me, timestamp, START_TX, NULL, 0);
//				printf("faccio free di %p, stato %p\n", event_content->read_set_ptr, state);
//				free(event_content->read_set_ptr);
			}

			break;

		default: 
			fprintf(stdout, "PCS: Unknown event type! (me = %d - event type = %d)\n", me, event_type);
			abort();
			
	}
}


bool OnGVT(unsigned int me, lp_state_type *state) {

	printf("%d: %f\% (%d/%d)\n", me, 100 * state->committed_tx / (double)TOTAL_COMMITTED_TX, state->committed_tx, TOTAL_COMMITTED_TX);

	if(state->committed_tx < TOTAL_COMMITTED_TX)
		return false;
	return true;
}

