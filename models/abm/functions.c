#include <ROOT-Sim.h>
#include "application.h"


void initialize_obstacles(obstacles_t **obstacles) {
	unsigned int i;
	
	SetupObstacles(obstacles);
	for(i = 0; i < OBSTACLES_PERCENT * n_prc_tot; i++) {
		AddObstacle(*obstacles, FindReceiver(TOPOLOGY_MESH));
	}
}

int add_agent(lp_state_type *state, agent_t *agent) {
	printf("Add agent: %llu\n", agent->uuid);
	return 0;
}



agent_t *remove_agent(lp_state_type *state, unsigned long long uuid) {
	printf("Remove agent: %llu\n", uuid);
	return NULL;
}

/**
 * This (helper) function schedules a new UPDATE_NEIGHBOURS
 * event towards the cells that represent my neighbours
 *
 * @author Matteo Principe
 * @date 8 feb 2018
 *
 * @param me The id of the cell who is calling the function
 * @param now The current time of the cell
 * @param present The number of bugs that are inside the cell
*/

void send_update_neighbours(simtime_t now, neighbour_state_t *new_event_content) {
	//int receiver;
	//int i;

	//// TODO: send state->num_agents

	//for(i = 0; i < 4; i++){
	//	receiver = GetReceiver(TOPOLOGY_TORUS,i);
	//	if(receiver >= (int) n_prc_tot || receiver < 0){
	//		printf("%s:%d Got receiver out of bounds!\n",__FILE__, __LINE__);
	//		exit(EXIT_FAILURE);
	//	}
	//	ScheduleNewEvent(receiver, now + TIME_STEP/100000, UPDATE_NEIGHBOURS, new_event_content, sizeof(new_event_content));
	//}

}
