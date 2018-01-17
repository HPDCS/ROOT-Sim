#include <ROOT-Sim.h>

#include "region.h"
#include "application.h"

/*
unsigned int get_target_id(unsigned int region_id, unsigned int direction) {
	// TODO: stub to be implemented
	return 0;
}

bool is_reachable(cell_state_type *region, unsigned int direction) {
	return !region->obstacles[direction];
}*/

unsigned int map_cartesian_to_linear(unsigned int x, unsigned int y) {
	unsigned int edge;

	edge = sqrt(number_of_regions);

	// Sanity checks
	if (edge * edge != number_of_regions) {
		rootsim_error(true, "Topology map wrongly specified!\n");
	}

	if (x > edge || y > edge) {
		rootsim_error(true, "Coordinates (%u, %u) are higher than maximum (%u, %u)\n", x, y, edge, edge);
	}

	return y * edge + x;
}

void map_linear_to_cartesian(unsigned int linear, unsigned int *x, unsigned int *y) {
	unsigned int edge;

	edge = sqrt(number_of_regions);

	// Sanity checks
	if (edge * edge != number_of_regions) {
		rootsim_error(true, "Topology map wrongly specified!\n");
	}

	if (linear > number_of_regions) {
		rootsim_error(true, "Required cell %u is higher than the total number of cells %u\n", linear, number_of_regions);
	}

	*x = linear % edge;
	*y = linear / edge;
}

void print_topology_map() {
	unsigned int edge;
	unsigned int x, y, d;
	cell_state_type *region;

	edge = sqrt(number_of_regions);

	// Sanity checks
	if (edge * edge != number_of_regions) {
		fprintf(stderr, "Topology map wrongly specified!\n");
		exit(EXIT_FAILURE);
	}

	printf("\n=================================================\n");
	printf("Map has %u cells\n\n", number_of_regions);

	for (y = 0; y < edge; y++) {
		for (x = 0; x < edge; x++) {
			printf(" %#02u ", y * edge + x);
		}
		printf("\n");
	}

	printf("\n=================================================\n");
}

unsigned int opposite_direction_of(unsigned int direction) {
	unsigned int opposite;

	switch (direction) {
		case DIRECTION_N:
			opposite = DIRECTION_S;
			break;
		case DIRECTION_E:
			opposite = DIRECTION_W;
			break;
		case DIRECTION_S:
			opposite = DIRECTION_N;
			break;
		case DIRECTION_W:
			opposite = DIRECTION_E;
		default:
			opposite = -1;
			break;
	}

	return opposite;
}

/*static char *direction_name(unsigned int direction) {

	switch (direction) {
	case DIRECTION_N:
		return "N";
	case DIRECTION_E:
		return "E";
	case DIRECTION_S:
		return "S";
	case DIRECTION_W:
		return "W";
	}
	return "UNKNOWN";
}*/
