#include <ROOT-Sim.h>
#include <math.h>
#include <stdio.h>

#include "application.h"


extern unsigned int num_cells;


bool isValidNeighbour(unsigned int sender, unsigned int neighbour) {

 	// For hexagon topology
 	unsigned int edge;
 	unsigned int x, y, nx, ny;


	// Convert linear coords to hexagonal coords
	edge = sqrt(num_cells);
	x = sender % edge;
	y = sender / edge;

	// Sanity check!
	if(edge * edge != num_cells) {
		fprintf(stderr, "Hexagonal map wrongly specified!\n");
		abort();
	}

	// Get the neighbour value
	switch(neighbour) {
		case NW:
			nx = (y % 2 == 0 ? x - 1 : x);
			ny = y - 1;
			break;
		case NE:
			nx = (y % 2 == 0 ? x : x + 1);
			ny = y - 1;
			break;
		case SW:
			nx = (y % 2 == 0 ? x - 1 : x);
			ny = y + 1;
			break;
		case SE:
			nx = (y % 2 == 0 ? x : x + 1);
			ny = y + 1;
			break;
		case E:
			nx = x + 1;
			ny = y;
			break;
		case W:
			nx = x - 1;
			ny = y;
			break;
	}

	if(nx < 0 || ny < 0 || nx >= edge || ny >= edge) {
		return false;
	}

	return true;
}



unsigned int GetNeighbourId(unsigned int sender, unsigned int neighbour) {
	unsigned int receiver;

 	// For hexagon topology
 	unsigned int edge;
 	unsigned int x, y, nx, ny;

	// Convert linear coords to hexagonal coords
	edge = sqrt(num_cells);
	x = sender % edge;
	y = sender / edge;

	// Sanity check!
	if(edge * edge != num_cells) {
		fprintf(stderr, "Hexagonal map wrongly specified!\n");
		abort();
	}

	switch(neighbour) {
		case NW:
			nx = (y % 2 == 0 ? x - 1 : x);
			ny = y - 1;
			break;
		case NE:
			nx = (y % 2 == 0 ? x : x + 1);
			ny = y - 1;
			break;
		case SW:
			nx = (y % 2 == 0 ? x - 1 : x);
			ny = y + 1;
			break;
		case SE:
			nx = (y % 2 == 0 ? x : x + 1);
			ny = y + 1;
			break;
		case E:
			nx = x + 1;
			ny = y;
			break;
		case W:
			nx = x - 1;
			ny = y;
			break;
		default:
			printf("UNKNOWN DIRECTION\n");
			abort();
	}

	// Convert back to linear coordinates
	receiver = (ny * edge + nx);

	return receiver;
}
