#include <strings.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include "topology_utils.h"

static void map_linear_to_hexagon(unsigned int linear, unsigned int *x, unsigned int *y) {
	unsigned int edge;

	edge = sqrt(n_prc_tot);

	// Sanity checks
	if(edge * edge != n_prc_tot) {
		printf("Hexagonal map wrongly specified!\n");
		abort();
	}
	if(linear > n_prc_tot) {
		printf("Required cell %u is higher than the total number of cells %u\n", linear, n_prc_tot);
		abort();
	}

	*x = linear % edge;
	*y = linear / edge;
}


static unsigned int opposite_direction_of(unsigned int direction) {
	unsigned int opposite;

	switch(direction) {
		case DIRECTION_NE:
			opposite = DIRECTION_SW;
			break;
		case DIRECTION_E:
			opposite = DIRECTION_W;
			break;
		case DIRECTION_SE:
			opposite = DIRECTION_NW;
			break;
		case DIRECTION_SW:
			opposite = DIRECTION_NE;
			break;
		case DIRECTION_W:
			opposite = DIRECTION_E;
			break;
		case DIRECTION_NW:
			opposite = DIRECTION_SE;
			break;
		default:
			opposite = UINT_MAX;
			break;
	}

	return opposite;
}

static double a_star(agent_state_type *state, unsigned int current_cell, unsigned int *good_direction) {
	unsigned int i;
	double min_distance = DBL_MAX;
	double current_distance;
	double dx, dy;
	unsigned int x1, y1, x2, y2;
	unsigned int tentative_cell;

	*good_direction = UINT_MAX;

	state->visit_map[current_cell].a_star_f = true;

	map_linear_to_hexagon(state->target_cell, &x1, &y1);

	for(i = 0; i < 6; i ++) {

		// Is there a cell to reach in this direction?
		if((tentative_cell = GetReceiver(current_cell, i, false)) == DIRECTION_INVALID)
			continue;

		// We don't simply visit a cell already visited!
		if(state->visit_map[tentative_cell].a_star_f) {
			continue;
		}

		// Can I go in that direction?
		if(state->visit_map[current_cell].neighbours[i] != UINT_MAX) {

			// Is this the target?
			if(current_cell == state->target_cell) {
				*good_direction = i;
				return 0.0;
			}

			// Compute the distance from the target if we make that move
			map_linear_to_hexagon(tentative_cell, &x2, &y2);
			dx = x1-x2;
			dy = y2-y1;
			current_distance = sqrt( dx*dx + dy*dy );

			// Is it a good choice?
			if(current_distance < DBL_MAX && current_distance < min_distance) {
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


unsigned int compute_my_direction(agent_state_type *state) {
	unsigned int good_direction = UINT_MAX;

	unsigned i = n_prc_tot;
	while(i--){
		state->visit_map[i].a_star_f = false;
	}

	a_star(state, state->current_cell, &good_direction);

	//printf("\n");
	unsigned int x1, y1;
	unsigned int x2, y2;
	double min_distance = DBL_MAX;
	double distance;
	double dx, dy;
	unsigned receiver;

	map_linear_to_hexagon(state->target_cell, &x1, &y1);

	for( i = 0; i < 6; i++) {
		if((receiver = GetReceiver(state->current_cell, i, false)) != DIRECTION_INVALID) {
			map_linear_to_hexagon(receiver, &x2, &y2);

			dx = x1-x2;
                        dy = y2-y1;
			distance = sqrt( dx*dx + dy*dy );

			if(distance < min_distance) {
				min_distance = distance;
				good_direction = i;
			}
		}
	}

	return good_direction;
}


unsigned int closest_frontier(agent_state_type *state, unsigned int curr_cell, unsigned int exclude) {
	unsigned int i, j;
	unsigned int x, y, curr_x, curr_y;
	bool is_reachable;
	double distance;
	double min_distance = DBL_MAX;
	unsigned int target = -1;
	unsigned receiver;

	map_linear_to_hexagon(curr_cell, &curr_x, &curr_y);

	for(i = 0; i < n_prc_tot; i++) {

		if(!state->visit_map[i].visited) {

			if(i == exclude) {
				// I know this is a frontier, but I don't want to go there
				continue;
			}

			// A frontier is marked as non visited, but has at least
			// one visited cell around it, with no obstacle in between
			is_reachable = false;
			for(j = 0; j < 6; j++) {
				if((receiver = GetReceiver(i, j, false)) != DIRECTION_INVALID &&
				   state->visit_map[receiver].visited &&
				   state->visit_map[receiver].neighbours[opposite_direction_of(j)] != 1) {
					is_reachable = true;
				}
			}

			if(is_reachable) {
				map_linear_to_hexagon(i, &x, &y);
				distance = sqrt((curr_x - x)*(curr_x - x) + (curr_y - y)*(curr_y - y));

				if(distance < min_distance) {
					min_distance = distance;
					target = i;
				}
			}
		}
	}

	return target;
}



