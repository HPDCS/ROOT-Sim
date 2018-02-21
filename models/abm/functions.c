#include <ROOT-Sim.h>
#include "application.h"

int add_agent(lp_state_t *state, agent_t *agent) {
	agent_node_t *node;

	printf("Add agent %llu to state %p\n", agent->uuid, state);

	// Fill the agent structure
	node = malloc(sizeof(agent_node_t));
	node->next = state->agents;
	node->agent = agent;

	// Link the new agent node to the linked list of the
	// simulation state.
	state->agents = node;
	state->num_agents++;

	return 0;
}


agent_t *remove_agent(lp_state_t *state, unsigned long long uuid) {
	agent_node_t *node;

	printf("Remove agent %llu\n", uuid);

	// Look for the agent identified by the provided uuid
	// and free that space in the simulation state
	node = state->agents;
	while(node != NULL) {
		if(node->agent->uuid == uuid) {
			free(node->agent);
			free(node);
			state->num_agents--;
			break;
		}
		node = node->next;
	}

	return NULL;
}


void print_agent_list(lp_state_t *state) {
	agent_node_t *node = state->agents;

	printf("Agent list of state %p: [", state);
	while(node != NULL) {
		printf("%llu,", node->agent->uuid);
		node = node->next;
	}
	printf("\033[1D]\n");
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
