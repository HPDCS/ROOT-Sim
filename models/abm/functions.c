#include <ROOT-Sim.h>
#include "application.h"


#define print_array(a) printf("Path found: [");\
	for (unsigned int __n = 0; __n < steps; __n++) {\
		printf("%d,", (a)[__n]);\
	}\
	printf("\033[1D]\n")



agent_t *create_agent(void) {
	agent_t *agent = malloc(sizeof(agent_t *));
	bzero(agent, sizeof(agent_t));
	return agent;
}


agent_t *find_agent(lp_state_t *state, unsigned long long uuid) {
	agent_node_t *node;

	// Look for the agent identified by the provided uuid
	node = state->agents;
	while(node != NULL) {
		if(node->agent->uuid == uuid) {
			return node->agent;
		}
		node = node->next;
	}
}


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
	agent_node_t *node, *prev;
	agent_t *agent;

	printf("Remove agent %llu\n", uuid);

	// Look for the agent identified by the provided uuid
	// and free that space in the simulation state
	prev = node = state->agents;
	while(node != NULL) {
		agent = node->agent;

		if(node->agent->uuid == uuid) {
			// Unlink and free the identified node agent
			prev->next = node->next;
			free(node);
			state->num_agents--;
			break;
		}
		prev = node;
		node = node->next;
	}

	return agent;
}


unsigned int get_agent_current_destination(agent_t *agent) {
	for (int i = agent->visited; i < agent->visit_list_size; i++) {
		if (agent->visit_list[i].action != TRAVERSE) {
			return agent->visit_list[i].region;
		}
	}
}


unsigned int compute_agent_path(agent_t **agent, obstacles_t *obstacles) {
	unsigned int steps;
	unsigned int *list;
	agent_t *new_agent;
	visit_t v;

	if (*agent == NULL)
		return UINT_MAX;

	steps = ComputeMinTour(&list, obstacles, TOPOLOGY, get_agent_current_cell(*agent), get_agent_current_destination(*agent));
	if(steps == UINT_MAX) {
		error(true, "Picked an unreachable cell\n");
	}

	// **** DEBUG ****
	print_array(list);

	// Allocate a new agent
	new_agent = malloc(sizeof(agent_t) + sizeof(visit_t) * ((*agent)->visit_list_size + steps));

	// Copy the first visited part of the path
	memcpy(new_agent, *agent, sizeof(agent_t) + sizeof(visit_t) * ((*agent)->visited));

	// Build the path of visit_t list
	for (unsigned int i = (*agent)->visited; i < steps; i++) {
		v = (visit_t)new_agent->visit_list[i];
		v.time = INFTY;
		v.region = list[i];
		v.action = TRAVERSE;
	}

	// Copy the remainining part of the visit list
	memcpy(new_agent->visit_list + (*agent)->visited + steps, (*agent)->visit_list + (*agent)->visited, sizeof(visit_t) * steps);

	// Free the temporary list
	free(list);

	// Free the old agent's descriptor
	free(*agent);

	// Return the new agent's descriptor
	*agent = new_agent;
	
	return steps;
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
