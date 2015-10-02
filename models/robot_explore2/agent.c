#include <math.h>
#include <limits.h>
#include <strings.h>

#include "application.h"
#include "agent.h"
#include "region.h"

double a_star(agent_state_type* astate, cell_state_type *rstate, unsigned int current_cell, unsigned int *good_direction) {
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
	BITMAP_SET_BIT(rstate->a_star_map, current_cell);
	//dump_a_star_map(state);

	// Translates the selected target cell into the cartesian coordinates
	// in order to perform path search
	map_linear_to_cartesian(astate->target_cell, &x1, &y1);

	for (i = 0; i < CELL_EDGES; i++) {

		// Query the simulation engine to retrieve the id of the region
		// heading in the selected direction.
		tentative_cell = GetReceiver(TOPOLOGY_SQUARE, i);

		if ((signed int)tentative_cell < 0) {
			// Can't go that way!
			continue;
		}
		// We don't simply visit a cell already visited!
		if (BITMAP_CHECK_BIT(rstate->a_star_map, tentative_cell)) {
			continue;
		}

		if (BITMAP_CHECK_BIT(astate->visit_map, tentative_cell)) {
			continue;
		}

		// TODO: reintroduce obstacles. With ECS this is easy,
		// without during initialization the full obstacles map
		// should be broadcast to all regions
		// Can I go in that direction?
		// TODO: signed comparison
//		cell_state_type *region;
//		region = GetRegionState(current_cell);
//		if (CHECK_BIT_AT(region->obstacles, i)) {
//			continue;
//		}

		// Is this the target?
		if (current_cell == astate->target_cell) {
			*good_direction = i;
			return 0.0;
		}
		// Compute the distance from the target if we make that move
		map_linear_to_cartesian(tentative_cell, &x2, &y2);
		dx = x2 - x1;
		dy = y2 - y1;

		distance_increment = a_star(astate, rstate, tentative_cell, good_direction);
		// TODO: optimize?
		current_distance = sqrt(dx*dx + dy*dy);

		// Is it a good choice?
		if (current_distance < INFTY && current_distance < min_distance) {
			min_distance = current_distance;
			*good_direction = i;
		}
	}

	return min_distance;
}

unsigned int compute_direction(agent_state_type *astate, cell_state_type *rstate) {
	unsigned int good_direction = UINT_MAX;

	BITMAP_BZERO(rstate->a_star_map, number_of_regions);

	a_star(astate, rstate, astate->current_cell, &good_direction);

	return good_direction;
}

unsigned int closest_frontier(agent_state_type *astate, cell_state_type *rstate, unsigned int exclude) {
	unsigned int i;
	unsigned int x, y, curr_x, curr_y;
	double distance;
	double min_distance = INFTY;
	unsigned int target = -1;
	unsigned int curr_cell = astate->current_cell;

	map_linear_to_cartesian(curr_cell, &curr_x, &curr_y);

	for (i = 0; i < number_of_regions; i++) {

		if (BITMAP_CHECK_BIT(astate->visit_map, i)) {

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

	return target;
}
