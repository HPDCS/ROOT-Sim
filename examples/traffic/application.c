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

#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>


#include "application.h"
#include "init.h"



void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, size_t size, lp_state_type *state) {
	simtime_t timestamp = 0;
	(void)size;

	if(state != NULL) {
		state->lvt = now;
	}

	switch(event_type) {

		// This event initializes the simulation state for each LP and inject first events
		case INIT:
			state = malloc(sizeof(lp_state_type));
			if(state == NULL){
				fprintf(stderr, "ERROR: Unable to allocate simulation state!\n");
				fflush(stderr);
				exit(EXIT_FAILURE);
			}

			SetState(state);

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

			// Set the number of cars in the current node at the beginning of the simulation
			if(state->lp_type == JUNCTION) {
				inject_new_cars(state, me);
			}
			
			// Schedule a keep alive event
			timestamp = now + Expent(10);
			ScheduleNewEvent(me, timestamp, KEEP_ALIVE, NULL, 0);
			
			break;



		case ARRIVAL:

			// TODO check su queue_slots!
			if(state->queue_slots == 0) {
				break;
			}

			// Check whether there is still an accident
			check_accident_end(state);

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

		case KEEP_ALIVE:
			timestamp = now + Expent(10);
			ScheduleNewEvent(me, timestamp, KEEP_ALIVE, NULL, 0);
			break;

      		default:
			printf(" state simulation: error - inconsistent event (me = %d - event type = %d)\n",me,event_type);
			break;
	}
}



bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void)me;
	
	if (snapshot->lvt < EXECUTION_TIME)
		return false;
	return true;
}
