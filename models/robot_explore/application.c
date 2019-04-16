#include "application.h"
#include "topology_utils.h"

#define OBSTACLE_PROB	0.01


struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .write_enabled = false, .default_geometry = TOPOLOGY_HEXAGON};
struct _abm_settings_t abm_settings = {.neighbour_data_size = sizeof(unsigned), .traverse_handler = _TRAVERSE, .keep_history = false};


#define KEEP_ALIVE_INTERVAL (now + (simtime_t)Expent(50))

void new_agent(unsigned me){
	agent_t agent = SpawnAgent(sizeof(agent_state_type) + n_prc_tot * sizeof(map_t));
	agent_state_type *agent_state = DataAgent(agent, NULL);
	memset(agent_state, 0, sizeof(agent_state_type) + n_prc_tot * sizeof(map_t));

	agent_state->current_cell = UINT_MAX;
	agent_state->target_cell = UINT_MAX;

	EnqueueVisit(agent, me, REGION_IN);
	ScheduleNewLeaveEvent(10 * Random() + 1, REGION_OUT, agent);
}


void ProcessEvent(int me, simtime_t now, int event_type, agent_t *agent_p, int event_size, cell_state_t *state) {
	(void)event_size;

	agent_t robot;
	agent_state_type *agent_state, *robot_state;
	unsigned int i;
	unsigned int j;
	simtime_t timestamp;

	if(event_type != INIT)
		state->started = true;

	switch(event_type) {

		case INIT:
			state = malloc(sizeof(cell_state_t));
			if(state == NULL) {
				fprintf(stderr, "Error allocating cell %d state!\n", me);
				abort();
			}
			SetState(state);

			memset(state, 0, sizeof(cell_state_t));


			state->has_obstacles = false;
			// Set the values for neighbours. If one is non valid, then set it to -1
			for(i = 0; i < 6; i++) {
				if(GetReceiver(me, i, false) != DIRECTION_INVALID) {
					// With a random probability, an obstacle
					// prevents any robot from getting there
					if(!state->has_obstacles && Random() < OBSTACLE_PROB) {
						state->has_obstacles = true;
						state->neighbours[i] = -1;
					} else {
						state->neighbours[i] = GetReceiver(me, i, false);
					}
				} else {
					state->neighbours[i] = -1;
				}
			}

			for(i = 0; i < ROBOTS; ++i)
				ScheduleNewEvent(n_prc_tot*Random(), now + 0.001, NEW_ROBOT, NULL, 0);


			ScheduleNewEvent(me, KEEP_ALIVE_INTERVAL, KEEP_ALIVE, NULL, 0);

			break;

		case NEW_ROBOT:
			new_agent(me);
			break;

		case KEEP_ALIVE:
			ScheduleNewEvent(me, KEEP_ALIVE_INTERVAL, KEEP_ALIVE, NULL, 0);
			break;

		case REGION_IN:
			agent_state = DataAgent(*agent_p, NULL);

			agent_state->current_cell = me;

			state->present_agents++;

			// Mark the cell as explored and "discover" the surroundings
			if(!agent_state->visit_map[agent_state->current_cell].visited) {
				agent_state->visit_map[agent_state->current_cell].visited = true;
				agent_state->visited_cells++;
				memcpy(&agent_state->visit_map[agent_state->current_cell].neighbours, state->neighbours, sizeof(unsigned int) * 6);
			}

			// Have I reached my target? In case, forget the target and go on exploring randomly
			if(agent_state->current_cell == agent_state->target_cell) {
				agent_state->target_cell = UINT_MAX;
			}

			// Is there any other robot in the cell? In case coordinate, otherwise explore alone
			if(CountAgents() > 1) {
				IterAgents(NULL);
				while(IterAgents(&robot)){
					robot_state = DataAgent(robot, NULL);

					for(j = 0; j < n_prc_tot; j++) {

						if(robot_state->visit_map[j].visited) {
							memcpy(&agent_state->visit_map[j], &robot_state->visit_map[j], sizeof(map_t));
							agent_state->visited_cells++;
						} else if (agent_state->visit_map[j].visited) {
							memcpy(&robot_state->visit_map[j], &agent_state->visit_map[j], sizeof(map_t));
							robot_state->visited_cells++;
						}
					}

					// We have met!
					agent_state->met_robots++;
					robot_state->met_robots++;

					// Give a frontier to this robot to reach
					robot_state->target_cell = closest_frontier(agent_state, agent_state->current_cell, -1);
					agent_state->target_cell = closest_frontier(agent_state, agent_state->current_cell, robot_state->current_cell);

					break; // we only want to exchange with one robot???
				}
			} else {
				agent_state->target_cell = closest_frontier(agent_state, agent_state->current_cell, -1);
			}

			// With some (tiny) randomness, forget where we are heading to!
			if(Random() < 0.01) {
				agent_state->target_cell = UINT_MAX;
			}


			// If we have a target, try to reach it, otherwise explore alone
			if(agent_state->target_cell == UINT_MAX) {
				agent_state->target_cell = closest_frontier(agent_state, agent_state->current_cell, -1);
				if(agent_state->target_cell == UINT_MAX) {
					agent_state->target_cell = RandomRange(0, n_prc_tot - 1);
				}
			}

			agent_state->direction = compute_my_direction(agent_state);

			// If computed direction is UINT_MAX, then there is no path to the target.
			// Just take a random direction
			if(GetReceiver(agent_state->current_cell, agent_state->direction, false) == DIRECTION_INVALID) {
				printf("%d at time %f is in %d reaching %d but no path found\n", me, now, agent_state->current_cell, agent_state->target_cell);
				do {
					agent_state->direction = RandomRange(0, 5);
				} while(GetReceiver(agent_state->current_cell, agent_state->direction, false) == DIRECTION_INVALID);
			}

			EnqueueVisit(*agent_p, GetReceiver(agent_state->current_cell, agent_state->direction, false), REGION_IN);
			timestamp = now + Expent(TIME_STEP) + 0.0000001;
			state->max_ratio = (double)agent_state->visited_cells/(double)n_prc_tot;
			ScheduleNewLeaveEvent(timestamp, REGION_OUT, *agent_p);
			break;

		case REGION_OUT:
			state->present_agents--;
			break;
		case _TRAVERSE:
		default:
			abort();
	}
}

int OnGVT(unsigned int me, cell_state_t *state) {
	(void)me;

	if(state->present_agents) {
		if(state->max_ratio < 1.0)
			printf("Last Robot : %lf percent\n", state->max_ratio*100);
		return state->max_ratio >= 1.0;

	} else {
		return state->started;
	}
}
