#include <math.h>
#include <limits.h>
#include <strings.h>

#include "application.h"
#include "agent.h"
#include "region.h"

double a_star(agent_state_type * state, unsigned int current_cell, unsigned int *good_direction) {
	unsigned int i;
	double min_distance = INFTY;
	double current_distance;
	double dx, dy;
	int x1, y1, x2, y2;
	unsigned int tentative_cell;
	double distance_increment;

//      printf("a_star invocation with current_cell %d; target_cell is %d\n", current_cell, state->target_cell);

	*good_direction = UINT_MAX;
	//SET_BIT(state->a_star_map, current_cell);
	BITMAP_SET_BIT(state->a_star_map, current_cell);
	//dump_a_star_map(state);

	// Translates the selected target cell into the cartesian coordinates
	// in order to perform path search
	map_linear_to_cartesian(state->target_cell, &x1, &y1);

	for (i = 0; i < CELL_EDGES; i++) {

		// Query the simulation engine to retrieve the id of the region
		// heading in the selected direction.
		// Note that GetTargetRegion could return -1 in case we tring
		// to move across a boundary.
		tentative_cell = GetTargetRegion(current_cell, i);

//              printf("Agent in cell %u is tring to move towards %s (cell %d)... ", current_cell, direction_name(i), tentative_cell);
		if ((signed int)tentative_cell < 0) {
			// In that direction no region is reachable
//                      printf("cannot move towards boundary\n");
			continue;
		}
		// We don't simply visit a cell already visited!
		if (BITMAP_CHECK_BIT(state->a_star_map, tentative_cell)) {
//                      printf("Cycle, skipping %d\n", tentative_cell);
			continue;
		}

		if (BITMAP_CHECK_BIT(state->visit_map, tentative_cell)) {
//                      printf("Region %d has already been visited\n", tentative_cell);
			continue;
		}
		// Can I go in that direction?
		// TODO: signed comparison
		//if(state->visit_map[current_cell].neighbours[i] != -1) {
		cell_state_type *region;
		region = GetRegionState(current_cell);
		if (CHECK_BIT_AT(region->obstacles, i)) {
//                      printf("Region %d in direction %d has an obstacle trying another choice\n", current_cell, i);
			continue;
		}
//              printf("direction chosen\n");

		// Is this the target?
		if (current_cell == state->target_cell) {
//                      printf("\033[1;32mTROVATO! current cell %d target %d\033[0m\n", current_cell, state->target_cell);
			*good_direction = i;
			return 0.0;
		}
		// Compute the distance from the target if we make that move
		map_linear_to_cartesian(tentative_cell, &x2, &y2);
		dx = x2 - x1;
		dy = y2 - y1;

		distance_increment = a_star(state, tentative_cell, good_direction);
		// TODO: optimize?
		current_distance = sqrt(dx * dx + dy * dy);

//              printf("target_cell is %d --> (%d, %d)\n", state->target_cell, x1, y1);
//              printf("tentative_cell is %d --> (%d, %d) :: distance= %.2f\n", tentative_cell, x2, y2, current_distance);

		// Is it a good choice?
		if (current_distance < INFTY && current_distance < min_distance) {
			min_distance = current_distance;
			*good_direction = i;
//                      printf("Trovata una nuova direzione migliore con distanza %.2f: %s\n", min_distance == INFTY ? -1 : min_distance, direction_name(i));
		}
	}

//      printf("A_START ends with distance %.2f\n", min_distance == INFTY ? -1 : min_distance, direction_name(i));

	return min_distance;
}

unsigned int compute_direction(agent_state_type * state) {
	unsigned int good_direction = UINT_MAX;

	BITMAP_BZERO(state->a_star_map, number_of_regions);

	a_star(state, state->current_cell, &good_direction);

//      printf("Direction computed: Agent in region %u moves towards %s [%u]\n", state->current_cell, direction_name(good_direction), good_direction);

	return good_direction;
}

unsigned int closest_frontier(agent_state_type * state, unsigned int exclude) {
	unsigned int i;
	unsigned int x, y, curr_x, curr_y;
	double distance;
	double min_distance = INFTY;
	unsigned int target = -1;
	unsigned int curr_cell = state->current_cell;

	map_linear_to_cartesian(curr_cell, &curr_x, &curr_y);

	for (i = 0; i < number_of_regions; i++) {

		if (BITMAP_CHECK_BIT(state->visit_map, i)) {

			if (i == exclude) {
				// I know this is a frontier, but I don't want to go there
				continue;
			}

			map_linear_to_cartesian(i, &x, &y);
			// TODO: optimize
			distance = sqrt((curr_x - x) * (curr_x - x) + (curr_y - y) * (curr_y - y));

			if (distance < min_distance) {
				min_distance = distance;
				target = i;
			}
		}
	}

//      printf("Closes frontier has returned a new min distance= %.2f towards region %d\n", min_distance, target);

	return target;
}
