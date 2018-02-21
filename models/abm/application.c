#include <ROOT-Sim.h>
#include <limits.h>
#include "application.h"



// This is a global variable that is initialized only once during the
// execution of INIT by LP 0. It does not change over time, and it is
// only read by other LPs.
obstacles_t *obstacles;


void ProcessEvent(unsigned int me, simtime_t now, int event_type, void *event_content, int event_size, lp_state_t *state) {
	unsigned int i, j;
	unsigned int destination;
	agent_t *agent, *new_agent;
	agent_node_t *agent_node;
	unsigned int steps;
	unsigned int *list;

	switch(event_type) {

		printf("=> Event %d <=\n", event_type);

		case INIT:

			// Load the configuration file
			load_config();

			// Initialize simulation state
			state = malloc(sizeof(lp_state_t));
			if(state == NULL){
				printf("%s:%d: Unable to allocate memory for the simulation state\n", __FILE__, __LINE__);
				exit(EXIT_FAILURE);
			}
			SetState(state);

			state->num_agents = 0;
			state->agents = NULL;

			// Get the configuration parameters from the config file
			region_config(state, me);

			if(TOPOLOGY == TOPOLOGY_HEXAGON) {
				state->neighbours = malloc(sizeof(neighbour_state_t) * 8);
			} else if(TOPOLOGY == TOPOLOGY_SQUARE) {
				state->neighbours = malloc(sizeof(neighbour_state_t) * 4);
			} else {
				printf("%s:%d: A wrong topology has been specified\n", __FILE__, __LINE__);
				exit(EXIT_FAILURE);
			}

			// Initialize the obstacles for the current topology
			// TODO: this doesn't work in distributed!
			if(me == 0) {
				initialize_obstacles(&obstacles);
			}

			// Initialize local agents, if any
			while((agent = get_next_agent(me)) != NULL) {
				add_agent(state, agent);
			}

			// DEBUG
			//print_agent_list(state);

			// Schedule a leave event for all generated agents
			agent_node = state->agents;
			while(agent_node != NULL) {
				ScheduleNewEvent(me, now + Expent(RESIDENCE_TIME), AGENT_OUT, &agent_node->agent->uuid, sizeof(agent_node->agent->uuid));
				agent_node = agent_node->next;
			}
			
			break;

		case AGENT_OUT:
			// Sanity check: does the cell host any agent?
			if (state->num_agents == 0) {
				printf("Agent %llu leaving from a cell that has no agent", *(unsigned long long *)event_content);
				exit(EXIT_FAILURE);
			}
			state->num_agents--;
			
			// This agent is leaving: remove from the list
			agent = remove_agent(state, *(unsigned long long *)event_content);

			// Send the agent to the destination cell
			ScheduleNewEvent(agent->visit_list[agent->visited + 1].region, now, AGENT_IN, agent, sizeof(agent_t) + sizeof(unsigned int) * agent->visit_list_size);

			free(agent);
			break;

		case AGENT_IN:
			// Sanity check: an obstacle could not host an agent
			if (IsObstacle(obstacles, me)) {
				printf("Obstacle %lu is requested to host agent%llu\n", me, (agent_t *)event_content);
				exit(EXIT_FAILURE);
			}
			
			state->num_agents++;

			// Get the agent
			agent = (agent_t *)event_content;
			agent->visited++;

			// If this is my last cell to visit, get a new destination
			if(agent->visited == agent->visit_list_size) {
				do {
					destination = FindReceiver(TOPOLOGY_MESH);
				} while(destination == me || IsObstacle(obstacles, destination));
				steps = ComputeMinTour(&list, obstacles, TOPOLOGY, me, destination);

				if(steps == UINT_MAX) {
					printf("%s:%d: Picked an unreachable cell\n", __FILE__, __LINE__);
					exit(EXIT_FAILURE);
				}

				new_agent = malloc(sizeof(agent_t) + sizeof(unsigned int) * (agent->visit_list_size + steps) );
				memcpy(&new_agent->visit_list[agent->visit_list_size], list, sizeof(unsigned int) * steps);
			}

			// Finish to copy in my state the copy of the agent
			memcpy(new_agent, agent, sizeof(agent_t) + sizeof(unsigned int) * agent->visit_list_size); 
			
			// Add the agent to the current list
			add_agent(state, agent);

			// Schedule a leave event
			ScheduleNewEvent(me, now + Expent(RESIDENCE_TIME), AGENT_OUT, &agent->uuid, sizeof(agent->uuid));
		
		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
			exit(EXIT_FAILURE);
	}


	// Independently of the event, we update the neighbours
	//send_update_neighbours();

}

// In this simple model, we terminate only after a certain time
bool OnGVT(unsigned int me, lp_state_t *snapshot) {
	return false;
}
