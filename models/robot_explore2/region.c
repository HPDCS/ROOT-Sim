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
		rootsim_error(true, "Topology map wrongly specified!\n");
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
	case N:
		opposite = S;
		break;
	case E:
		opposite = W;
		break;
	case S:
		opposite = N;
		break;
	case W:
		opposite = E;
	default:
		opposite = -1;
		break;
	}

	return opposite;
}

char *direction_name(unsigned int direction) {

	switch (direction) {
	case N:
		return "N";
	case E:
		return "E";
	case S:
		return "S";
	case W:
		return "W";
	}
	return "UNKNOWN";
}
