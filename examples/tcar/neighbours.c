#include <ROOT-Sim.h>
#include <math.h>
#include <stdio.h>

#include "application.h"


#define NE	0
#define NW	1
#define W	2
#define SW	3
#define SE	4
#define E	5



bool isValidNeighbour(unsigned int sender, unsigned int neighbour) {

 	// For hexagon topology
 	unsigned int edge;
 	unsigned int x, y, nx, ny;
	

	// Convert linear coords to hexagonal coords
	edge = sqrt(n_prc_tot);
	x = sender % edge;
	y = sender / edge;
			
	// Sanity check!
	if(edge * edge != n_prc_tot) {
		rootsim_error(true, "Hexagonal map wrongly specified!\n");
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



int GetNeighbourId(unsigned int sender, unsigned int neighbour) {
	unsigned int receiver;

 	// For hexagon topology
 	unsigned int edge;
 	unsigned int x, y, nx, ny;
	
	// Convert linear coords to hexagonal coords
	edge = sqrt(n_prc_tot);
	x = sender % edge;
	y = sender / edge;
			
	// Sanity check!
	if(edge * edge != n_prc_tot) {
		rootsim_error(true, "Hexagonal map wrongly specified!\n");
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
	}
	
	// Convert back to linear coordinates
	receiver = (ny * edge + nx);
		
	return receiver;
}




#undef NE
#undef NW
#undef W
#undef SW
#undef SE
#undef E
