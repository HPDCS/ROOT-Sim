#include <ROOT-Sim.h>
#include <stdio.h>
#include <limits.h>

#include "application.h"

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .write_enabled = false, .default_geometry = TOPOLOGY_SQUARE};
struct _abm_settings_t abm_settings = {sizeof(unsigned), _TRAVERSE, false};

void ProcessEvent(unsigned me, simtime_t now, int event_type, void *unused, unsigned event_size, lp_state_type *pointer) {

	(void) me;
	(void) unused;
	(void) event_size;

	unsigned i, j;
	unsigned receiver;
	unsigned *trails_p;
	unsigned min_trails;
	simtime_t timestamp = 0;

	switch(event_type) {

		case INIT:

			pointer = (lp_state_type *)malloc(sizeof(lp_state_type));
			if(pointer == NULL){
				printf("Out of memory!\n");
				exit(EXIT_FAILURE);
			}
			pointer->trails = 0;
			SetState(pointer);

			if(OCCUPIED_CELLS > n_prc_tot){
				printf("We require more cells to start the simulation!\n");
				exit(EXIT_FAILURE);
			}

			// Occupy the "first" and "last" cells
			if(me < ((OCCUPIED_CELLS + 1)/2) || me >= ((n_prc_tot)-(OCCUPIED_CELLS/2))) {
				for(i = 0; i < ROBOTS_PER_CELL; i++) {
					ScheduleNewEvent(me, now + (simtime_t)(20 * Random()), REGION_IN, NULL, 0);
				}
			}

			TrackNeighbourInfo(&pointer->trails);
			ScheduleNewEvent(me, now + 10, PING, NULL, 0);
			break;


		case REGION_IN:

			pointer->trails++;

			ScheduleNewEvent(me, now + TIME_STEP/100000, REGION_OUT, NULL, 0);

			break;

		case REGION_OUT:

			// Go to the neighbour who has the smallest trails count
			min_trails = UINT_MAX;
			i = DirectionsCount();
			while(i--) {
				if(GetNeighbourInfo(i, &j, (void **)&trails_p) != -1 && min_trails > *trails_p){
					min_trails = *trails_p;
					receiver = j;
				}
			}

			while(1){
				if(GetNeighbourInfo(Random()*DirectionsCount(), &receiver, (void **)&trails_p) != -1 && min_trails == *trails_p)
					break;
			}

			switch (DISTRIBUTION) {

				case UNIFORM:
					timestamp= now + (simtime_t) (TIME_STEP * Random());
					break;

				case EXPONENTIAL:
					timestamp= now + (simtime_t)(Expent(TIME_STEP));
					break;

				default:
					timestamp= now + (simtime_t)(TIME_STEP * Random());
		   			break;

			}

			ScheduleNewEvent(receiver, timestamp, REGION_IN, NULL, 0);
			break;

		case PING:
			ScheduleNewEvent(me, now + 10, PING, NULL, 0);
			break;
		case _TRAVERSE:
      		default:
      			printf("Unsupported event!\n");
      			exit(EXIT_FAILURE);
			break;
	}
}


int OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void) me;

 	if(snapshot->trails > MINIMUM_VISITS)
		return true;

	return false;
}
