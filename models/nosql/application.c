#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ROOT-Sim.h>

#include "application.h"


//#define LOOP 100000
#define LOOP 1000

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .default_geometry = TOPOLOGY_GRAPH, .write_enabled = false};

void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *ptr) {
	(void)size;
	int i;
	int target;

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

			state->residual_tx_ops = RandomRange(MAX_OP_COUNT-10, MAX_OP_COUNT);
			state->read_set_size = RandomRange(MAX_RS_SIZE - 10, MAX_RS_SIZE);
//			state->write_set_size = RandomRange(10, MAX_RS_SIZE);
			state->tx_ops_displacement = 0;
//			state->read_set = malloc(sizeof(int) * state->read_set_size);

			timestamp = now + (simtime_t)Expent(TX_OP_ARRIVAL);

			ScheduleNewEvent(me, timestamp, TX_OP, NULL, 0);



			break;


		case TX_OP:


			state->residual_tx_ops--;
			state->tx_ops_displacement += 1;
			timestamp= now + (simtime_t)Expent(TX_OP);

			if(state->residual_tx_ops > 0) {

				ScheduleNewEvent(me, timestamp, TX_OP, NULL, 0);
				state->read_set[state->tx_ops_displacement] = RandomRange(10,10000);
				for(i=0;i<state->tx_ops_displacement;i++){
					if (state->read_set[i] != state->read_set[state->tx_ops_displacement]) {
						//found conflicting item
					}
				}
				state->tx_ops_displacement += 1;

			} else {

				int recv = FindReceiver();
				int recv2;
				do {
					recv2 = FindReceiver();
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

			if ( memcmp((void*)state->read_set,(void*)state->write_set,sizeof(int)*MAX_RS_SIZE)){
					state->conflicted_tx++;
					for(i=0;i<MAX_RS_SIZE;i++){
						if (state->read_set[i] != state->write_set[i]) {
							//found conflicting item
						}
					}
				}

			#ifndef ECS
			for(i = 0; i < event_content->size; i++) {
				state->read_set[i] = 0;
			}
			#endif

			}

			break;

        	case PREPARE:

			for(i = 0; i < event_content->size; i++) {
				#ifdef ECS
				//event_content->read_set_ptr[i] = me;
				target = event_content->read_set_ptr[i] ;
				#else
				//event_content->read_set[i] = me;
				target = event_content->read_set[i] ;
				#endif
				state->write_set[i] = target;
			}
			#ifdef ECS
		 	memset((void*)event_content->read_set_ptr,0,sizeof(int)*MAX_RS_SIZE);
			#endif

			timestamp= now + (simtime_t)Expent(50);

			if(event_content->second) {
				new_event_content.read_set_ptr = event_content->read_set_ptr;
				new_event_content.second = true;
			} else {
				new_event_content.second = false;
			}
//			ScheduleNewEvent(event_content->from, timestamp, COMMIT, &event_content, sizeof(new_event_content));
			ScheduleNewEvent(me, timestamp, COMMIT, &event_content, sizeof(new_event_content));
//
			break;



		case COMMIT:

			if ( memcmp((void*)state->read_set,(void*)state->write_set,sizeof(int)*MAX_RS_SIZE)){
					state->conflicted_tx++;
					for(i=0;i<MAX_RS_SIZE;i++){

						if (state->read_set[i] != 0){
							if (state->read_set[i] != state->write_set[i]) {
								//found conflicting item
							}
						}
					}
				}

//			printf("aborting transaction\n");

//			break;
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
	(void)me;
	//printf("%d: %f\% (%d/%d)\n", me, 100 * state->committed_tx / (double)TOTAL_COMMITTED_TX, state->committed_tx, TOTAL_COMMITTED_TX);

	if(state->committed_tx < TOTAL_COMMITTED_TX)
		return false;
	return true;
}

