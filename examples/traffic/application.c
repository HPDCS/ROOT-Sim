/**
*
* TRAFFIC is a simulation model for the ROme OpTimistic Simulator (ROOT-Sim)
* which allows to simulate car traffic on generic routes, which can be
* specified from text file.
*
* The software is provided as-is, with no guarantees, and is released under
* the GNU GPL v3 (or higher).
*
* For any information, you can find contact information on my personal webpage:
* http://www.dis.uniroma1.it/~pellegrini
*  
* @file application.c
* @brief This module contains events' handler and check termination callbacks
* @author Alessandro Pellegrini
* @date January 12, 2012
*/

//#define DUMMY_LOAD
#ifdef DUMMY_LOAD
#define DUMMY_CYCLE	5000
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "application.h"
#include "init.h"



void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, size_t size, void * ptr) {

	int i;
	event_content_type new_event_content;
	simtime_t timestamp = 0;
	lp_state_type *state = (lp_state_type*)ptr;

	// Store the new event's simulation time
	if(event_type != INIT) {

/*		if(now < state->lvt) {
			printf("SEVERE ERROR: my LVT is %f, now is %f\n", state->lvt, now);
		}
*/
		state->lvt = now;
	}

#ifdef DUMMY_LOAD
	int dummy_x = 2;
	int dummy_y = 3;
	for (i = 0; i < DUMMY_CYCLE; i++) {
		dummy_x *= dummy_y;
		if (dummy_y > 0)
			dummy_y -= dummy_x;
		else
			dummy_y -= dummy_x;

		if (dummy_x > 1000)
			dummy_x /= 10;
		if (dummy_y > 1000)
			dummy_y /= 10;
	}
#endif

	switch(event_type) {

		// This event initializes the simulation state for each LP and inject first events		
		case INIT:	
			state = (lp_state_type *)malloc(sizeof(lp_state_type));
			if(state == NULL){
				fprintf(stderr, "ERROR: Unable to allocate simulation state!\n");
				fflush(stderr);
				exit(EXIT_FAILURE);
			}
			
			SetState(state);

			char **arguments = (char **)event_content;
			int w;
			for(w = 0; w < size; w += 2) {
			//parse commandline parameters
			}

			// Initialize state
			bzero(state, sizeof(lp_state_type));

			// Parse the topology file and set state accordingly
			init_my_state(me, state);

			// Set the number of queuable cars in this node
			if(state->lp_type == JUNCTION) {
				state->queue_slots = CARS_PER_JUNCTION;
			} else if(state->lp_type == SEGMENT) {
				state->queue_slots = (int)(state->segment_length * CARS_PER_UNIT_LENGTH);
			}
			state->total_queue_slots = state->queue_slots;


			// LP 0 will collect statistics, prepare its simulation state for this
/*			if(me == 0) {
				state->statistics = malloc(sizeof(stat) * n_prc_tot);
			}
*/
			// Set the number of cars in the current node at the beginning of the simulation
			if(state->lp_type == JUNCTION) {
				inject_new_cars(state, me);
			}

			// In order to allow simulation time to advance, we inject fake cars in every segment
			// FIXME: this is a major issue in the simulator, cannot be solved at app level
			if(state->lp_type == SEGMENT) {
				state->enter_prob = JUNCTION_TRAVERSE_TIME; // that's a bad hack...
				for(i = 0; i < 20; i++) {
					inject_new_cars(state, me);
				}
			}
/*			if(state->lp_type == SEGMENT) {
				for(i = 0; i < state->topology->num_neighbours; i++) {
					event_content_type new_evt;
					timestamp = (simtime_t)(Expent(20.0));
					if(timestamp < 0) {
						fprintf(stderr, "(%d) primo evento ARRIVAL a %f\n", me, timestamp);
						fflush(stderr);
					}
					new_evt.from = state->topology->neighbours[i];
					ScheduleNewEvent(me, timestamp, ARRIVAL, &new_evt, sizeof(event_content_type));
				}
			}
*/
			// Print simulation information (once)
			if(me == 0) {
				// Anche la simulation time
				// TODO
			}

			break;



		case ARRIVAL:

			// TODO check su queue_slots!
			if(state->queue_slots == 0) {
				break;
			}

//			state->cars_passed++;


			// Check whether there is still an accident
			check_accident_end(state, me);

			if(!state->accident) {
				cause_accident(state, me);
			}

			// See if the car is leaving the highway
			if(!check_car_leaving(state, event_content->from, me)) {

				state->queue_slots--;

				// Send the car to another LP
				forward_car(state, event_content->from, me);

			} else {

//				state->cars_left++;
			}

			// If the arrival is related to a new car entering the highway, schedule
			// the next car entering
			if(event_content->injection && state->lp_type == JUNCTION) {
				inject_new_cars(state, me);
			} 
			// HACK
/*			else if(state->lp_type == SEGMENT && event_content->injection) {
				timestamp = now + (simtime_t)(Expent(10000)); 
				new_event_content.from = me;
				new_event_content.injection = 1;
				ScheduleNewEvent(me, timestamp, ARRIVAL, &new_event_content, sizeof(event_content_type));
			}
*/
			break;


		case LEFT:

			state->queue_slots++;
			break;



/*		case INJECT:

			// Add the new car to the queue
			if(enqueue_car(state, now, event_content->from) == 1) {
				state->cars_joined++;
			} else {
				state->cars_rejected++;
			}

			// Schedule the new car arrival event
			if(state->lp_type == JUNCTION) {
				inject_new_cars(state, me);
			}

*/
/*		case SEGMENT_FULL:

			// Check if some cars leaved us
			check_cars_leaving(state, me);

			// When this event is received, the car must be reassigned in my queue
			enqueue_car(state, now, me);
			break;
*/


/*		case FREE_ROAD:
			
			// Restore normal traffic conditions
			state->accident = 0;

			check_cars_leaving(state, me);
			break;
*/

		case COLLECT_STAT:
			// il campo from nell'evento dice dove salvare le statistiche
			break;
	

      		default: 
			printf(" state simulation: error - inconsistent event (me = %d - event type = %d)\n",me,event_type); 
			break;
	}
}



int OnGVT(unsigned int me, lp_state_type *snapshot) {

/*	if(D_EQUAL(snapshot->lvt, 0)) {
		printf("Lp %d (%s) a LVT 0!\n", gid, snapshot->name);
		fflush(stdout);
	}
*/
	// TODO: Aggregare le statistiche!!!

	if (snapshot->lvt < EXECUTION_TIME) 
		return 0;
	return 1;	
}



