
#include <math.h>
#include <limits.h>

#define REGION_IN	201

#define TIME_STEP	5.0



static unsigned int map_hexagon_to_linear(unsigned int x, unsigned int y) {
	unsigned int edge;

	edge = sqrt(num_cells);

	// Sanity checks
	if(edge * edge != num_cells) {
		rootsim_error(true, "Hexagonal map wrongly specified!\n");
	}
	if(x > edge || y > edge) {
		rootsim_error(true, "Coordinates (%u, %u) are higher than maximum (%u, %u)\n", x, y, edge, edge);
	}

	return y * edge + x;
}


static void map_linear_to_hexagon(unsigned int linear, unsigned int *x, unsigned int *y) {
	unsigned int edge;

	edge = sqrt(num_cells);

	// Sanity checks
	if(edge * edge != num_cells) {
		printf("Hexagonal map wrongly specified!\n");
		abort();
	}
	if(linear > num_cells) {
		printf("Required cell %u is higher than the total number of cells %u\n", linear, num_cells);
		abort();
	}

	*x = linear % edge;
	*y = linear / edge;
}


static unsigned int opposite_direction_of(unsigned int direction) {
	unsigned int opposite;

	switch(direction) {
		case NE:
			opposite = SW;
			break;
		case E:
			opposite = W;
			break;
		case SE:
			opposite = NW;
			break;
		case SW:
			opposite = NE;
			break;
		case W:
			opposite = E;
			break;
		case NW:
			opposite = SE;
			break;
		default:
			opposite = UINT_MAX;
			break;
	}

	return opposite;
}

static char *direction_name(unsigned int direction) {

	switch(direction) {
		case NE:
			return "NE";
		case E:
			return "E";
		case SE:
			return "SE";
		case SW:
			return "SW";
		case W:
			return "W";
		case NW:
			return "NW";
	}
	return "UNKNOWN";
}

static double a_star(agent_state_type *state, unsigned int current_cell, unsigned int *good_direction) {
	unsigned int i;
	double min_distance = INFTY;
	double current_distance;
	double dx, dy;
	unsigned int x1, y1, x2, y2;
	unsigned int tentative_cell;
	double distance_increment;


	*good_direction = UINT_MAX;

	SET_BIT(state->a_star_map, current_cell);

	map_linear_to_hexagon(state->target_cell, &x1, &y1);

	for(i = 0; i < 6; i ++) {

		// Is there a cell to reach in this direction?
		if(!isValidNeighbour(current_cell, i)) {
			continue;
		}

		tentative_cell = GetNeighbourId(current_cell, i);

		// We don't simply visit a cell already visited!
		if(CHECK_BIT(state->a_star_map, tentative_cell)) {
			continue;
		}

		// Can I go in that direction?
		if(state->visit_map[current_cell].neighbours[i] != -1) {

			// Is this the target?
			if(current_cell == state->target_cell) {
//				printf("TROVATO! current cell %d target %d\n", current_cell, state->target_cell);
				*good_direction = i;
				return 0.0;
			}

			// Compute the distance from the target if we make that move
			map_linear_to_hexagon(tentative_cell, &x2, &y2);
			dx = x1-x2;
			dy = y2-y1;
			distance_increment = a_star(state, tentative_cell, good_direction);
			current_distance = sqrt( dx*dx + dy*dy );

			// Is it a good choice?
			if(current_distance < INFTY && current_distance < min_distance) {
				min_distance = current_distance;
				*good_direction = i;
			}
		}
	}

//	if(min_distance < INFTY) {
//		printf("direction: %d\n", *good_direction); fflush(stdout);
//		printf("%d, ", GetNeighbourId(current_cell, *good_direction));
//	}

	// We're getting far!
	//~ if(min_distance > distance) {
		//~ return INFTY;
	//~ }

	return min_distance;
}


static unsigned int compute_my_direction(agent_state_type *state) {
	unsigned int good_direction = UINT_MAX;

	bzero(state->a_star_map, BITMAP_SIZE(num_cells));

//	printf("A* from %d to %d: ", state->current_cell, state->target_cell);

	a_star(state, state->current_cell, &good_direction);

//	printf("\n");
/*	unsigned int x1, y1;
	unsigned int x2, y2;
	double min_distance = INFTY;
	double distance;
	double dx, dy;

	map_linear_to_hexagon(state->target_cell, &x1, &y1);

	int i;
	for( i = 0; i < 6; i++) {
		if(isValidNeighbour(state->current_cell, i)) {
			map_linear_to_hexagon(GetNeighbourId(state->current_cell, i), &x2, &y2);

			dx = x1-x2;
                        dy = y2-y1;
			distance = sqrt( dx*dx + dy*dy );

			if(distance < min_distance) {
				min_distance = distance;
				good_direction = i;
			}
		}
	}
*/
	return good_direction;
}


static unsigned int closest_frontier(agent_state_type *state, unsigned int curr_cell, unsigned int exclude) {
	unsigned int i, j;
	unsigned int x, y, curr_x, curr_y;
	bool is_reachable;
	double distance;
	double min_distance = INFTY;
	unsigned int target = -1;

	map_linear_to_hexagon(curr_cell, &curr_x, &curr_y);

	for(i = 0; i < num_cells; i++) {

		if(!state->visit_map[i].visited) {

			if(i == exclude) {
				// I know this is a frontier, but I don't want to go there
				continue;
			}

			// A frontier is marked as non visited, but has at least
			// one visited cell around it, with no obstacle in between
/*			is_reachable = false;
			for(j = 0; j < 6; j++) {
				if(isValidNeighbour(i, j) &&
				   state->visit_map[GetNeighbourId(i, j)].visited &&
				   state->visit_map[GetNeighbourId(i, j)].neighbours[opposite_direction_of(j)] != 1) {
					is_reachable = true;
				}
			}

			if(is_reachable) {
*/				map_linear_to_hexagon(i, &x, &y);
				distance = sqrt((curr_x - x)*(curr_x - x) + (curr_y - y)*(curr_y - y));

				if(distance < min_distance) {
					min_distance = distance;
					target = i;
				}
//			}
		}
	}

	return target;
}

void AgentProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, int event_size, void *pointer) {

	agent_state_type *state = (agent_state_type *)pointer;
	agent_state_type *robot;
	cell_state_type *cell;
	event_content_type new_event;
	unsigned int i;
	unsigned int j;
	simtime_t timestamp;

	switch(event_type) {

		case INIT:
			pointer = malloc(sizeof(agent_state_type));
			if(pointer == NULL) {
				rootsim_error(true, "Error allocating agent %d state!\n", me - num_cells);
			}
			SetState(pointer);

			bzero(pointer, sizeof(agent_state_type));
			state = (agent_state_type *)pointer;

			// Allocate the map bitmap
			state->visit_map = malloc(num_cells * sizeof(map_t));
			bzero(state->visit_map, num_cells * sizeof(map_t));

			state->current_cell = UINT_MAX;
			state->target_cell = UINT_MAX;

			// Initialize the a_star visit bitmap
			state->a_star_map = ALLOCATE_BITMAP(num_cells);

			states[me] = pointer;

			new_event.cell = RandomRange(0, num_cells - 1);
			new_event.coming_from = -1;
			ScheduleNewEvent(me, 10 * Random() + 1, REGION_IN, &new_event, sizeof(event_content_type));

			break;


		case REGION_IN:
			state->current_cell = event_content->cell;

			if(event_content->coming_from != -1) {
				cell = (cell_state_type *)states[event_content->coming_from];
	                        cell->present_agents--;
	                        RESET_BIT(cell->agents, me - num_cells);
			}

//			printf("Robot %d e' in cella %d, ", me - num_cells, state->current_cell);
			fflush(stdout);

			// Register the position of the robot in the cell
			cell = (cell_state_type *)states[state->current_cell];
			cell->present_agents++;
			SET_BIT(cell->agents, me - num_cells);

			// Mark the cell as explored and "discover" the surroundings
			if(!state->visit_map[state->current_cell].visited) {
				state->visit_map[state->current_cell].visited = true;
				state->visited_cells++;
				memcpy(&state->visit_map[state->current_cell].neighbours, cell->neighbours, sizeof(unsigned int) * 6);
			}

			// Have I reached my target? In case, forget the target and go on exploring randomly
			if(state->current_cell == state->target_cell) {
				state->target_cell = UINT_MAX;
			}

			// Is there any other robot in the cell? In case coordinate, otherwise explore alone
			if(cell->present_agents > 1) {

				for(i = 0; i < (n_prc_tot - num_cells); i++) {
					if(CHECK_BIT(cell->agents, i)) {

						// Exchange information about the map
						robot = (agent_state_type *)states[num_cells + i];
						for(j = 0; j < num_cells; j++) {

							if(robot->visit_map[j].visited) {
								memcpy(&state->visit_map[j], &robot->visit_map[j], sizeof(map_t));
								state->visited_cells++;
							} else if (state->visit_map[j].visited) {
								memcpy(&robot->visit_map[j], &state->visit_map[j], sizeof(map_t));
								robot->visited_cells++;
							}
						}

						// We have met!
						state->met_robots++;
						robot->met_robots++;

						// Give a frontier to this robot to reach
						robot->target_cell = closest_frontier(state, state->current_cell, -1);
						state->target_cell = closest_frontier(state, state->current_cell, robot->current_cell);

						break;
					}
				}
			}// else {
			//	 state->target_cell = closest_frontier(state, state->current_cell, -1);
			//}

			// With some (tiny) randomness, forget where we are heading to!
			if(Random() < 0.01) {
				state->target_cell = UINT_MAX;
			}


			// If we have a target, try to reach it, otherwise explore alone
			if(state->target_cell == UINT_MAX) {
				state->target_cell = closest_frontier(state, state->current_cell, -1);
				if(state->target_cell == UINT_MAX) {
					state->target_cell = RandomRange(0, num_cells - 1);
				}
			}

			state->direction = compute_my_direction(state);

			// If computed direction is UINT_MAX, then there is no path to the target.
			// Just take a random direction
			if(!isValidNeighbour(state->current_cell, state->direction)) {
//				printf("%d at time %f is in %d reaching %d but no path found\n", me, now, state->current_cell, state->target_cell);
//				printf("Neighbours of %d: ", state->target_cell);
//				for(i = 0; i < 6; i ++) {
//					printf("%d, ", ((cell_state_type *)states[state->target_cell])->neighbours[i]);
//				}
//				printf("\n");
				do {
					state->direction = RandomRange(0, 5);
				} while(!isValidNeighbour(state->current_cell, state->direction));
			}

//			printf("ha direzione %s, ", direction_name(state->direction));
			fflush(stdout);


			// We can now mooooove!
			new_event.cell = GetNeighbourId(state->current_cell, state->direction);
			new_event.coming_from = state->current_cell;
//			printf("va in cella %d\n", new_event.cell);
			fflush(stdout);
			if(new_event.cell > num_cells) {
				printf("ERRORE\n");
				abort();
			}
			timestamp = now + Expent(TIME_STEP) + 0.0000001;

			ScheduleNewEvent(me, timestamp, REGION_IN, &new_event, sizeof(event_content_type));
			break;

		default:
			abort();
	}
}



int AgentOnGVT(unsigned int me, agent_state_type *state) {

	if((double)state->visited_cells / num_cells < 1.0) {
		printf("Robot %d: %.02f percent --- %d meetings so far --- currently in cell %d\n", me - num_cells, (double)state->visited_cells / num_cells * 100, state->met_robots, state->current_cell);

	} else {
		return true;
	}

	return false;
}


