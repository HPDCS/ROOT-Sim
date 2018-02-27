#include <ROOT-Sim.h>
#include <limits.h>
#include "application.h"


// This is a global variable that is initialized only once during the
// execution of INIT by LP 0. It does not change over time, and it is
// only read by other LPs.
obstacles_t *obstacles;


void ProcessEvent(unsigned int me, simtime_t now, int event_type, void *event_content, int event_size, lp_state_t *state) {
	unsigned int steps;
	agent_t *agent;
	agent_node_t *agent_node;

	printf("[LP%d] :: Event %d at time %.7f\n", me, event_type, now);

	switch(event_type) {

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
				//printf("Agent %llu leaving from a cell that has no agent", *(unsigned long long *)event_content);
				//exit(EXIT_FAILURE);
				error(true, "Agent %d is leaving region %d, though it has no agents\n", *(unsigned long long *)event_content, me);
			}

			// This agent is leaving: remove from the list
			agent = remove_agent(state, *(unsigned long long *)event_content);

			// Once we have a cpoy of the agent to work with, we need
			// to decide whether the agent has a path to reach its destination;
			// To do that we invoke the 'copmute_agent_path()' function
			// if the action contained in the next task region is different
			// from TRAVERSE and the time is not defined yet.
			visit_t next = agent->visit_list[agent->visited];
			if(next.action == TRAVERSE && next.time == INFTY) {
				printf("Agent '%s' computes a new route to %d\n", agent->name, get_agent_current_destination(agent));
				compute_agent_path(&agent, obstacles);
			}

			// Send the agent to the destination cell
			// NOTE: since the array of visit_list is zero-based and the visited cell c
			// NOTE2: the list of cell to be visited does not include the current cell
			ScheduleNewEvent(next.region, now, AGENT_IN, agent, sizeof(agent_t) + sizeof(unsigned int) * agent->visit_list_size);

			printf("Agent '%s' (uuid:%d) wants to move in region '%d' towards destination '%d'\n", agent->name, agent->uuid, get_agent_current_region(agent), get_agent_current_destination(agent));
			// The agent has been copined by the platform into the event's content, now it possible to free that buffer
			free(agent);
			break;

		case AGENT_IN:
			// Sanity check: an obstacle could not host an agent
			if (IsObstacle(obstacles, me)) {
				printf("Obstacle %u is requested to host agent%llu\n", me, ((agent_t *)event_content)->uuid);
				exit(EXIT_FAILURE);
			}

			// Get the agent by copying the 'one' provoded into the event's payload
			// so that we still work in data separation
			//agent = ((agent_t *)event_content);
			agent = malloc(event_size);
			memcpy(agent, event_content, event_size);

			// Update the state of the current (new) copy of the agent
			agent->visited++;

			// Add the agent to the current list
			add_agent(state, agent);

			// NOTE: we do not rely on the proper event to change the destination
			// since it is intended to change the destination before to have reached
			// the current destionation
			if (get_agent_current_region(agent) == get_agent_current_destination(agent)) {
				steps = compute_agent_path(&agent, obstacles);
				if (steps == UINT_MAX) {
					error(false, "Impossible to determine a new destination for the agent %llu\n", agent->uuid);
				}
			}

			// Schedule a leave event
			ScheduleNewEvent(me, now + Expent(RESIDENCE_TIME), AGENT_OUT, &agent->uuid, sizeof(agent->uuid));

			break;

		case AGENT_CHANGE_DEST:
			// Find the agent indentified by the UUId provided in the event's content
			agent = find_agent(state, *(unsigned long long *)event_content);

			// Compute a new destinaion cell for that agent
			steps = compute_agent_path(&agent, obstacles);
			if (steps == UINT_MAX) {
				error(false, "Impossible to determine a new destination for the agent %llu\n", agent->uuid);
			}

			// Schedule the event to leave the current cell towards the destination one
			ScheduleNewEvent(me, now + Expent(RESIDENCE_TIME), AGENT_OUT, &agent->uuid, sizeof(agent->uuid));

			break;

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
