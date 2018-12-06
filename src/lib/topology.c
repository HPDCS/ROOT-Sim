/**
* @file lib/topology.c
*
* @brief Topology Library
*
* Topology Library
*
* @copyright
* Copyright (C) 2008-2018 HPDCS Group
* https://hpdcs.github.io
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @author Alessandro Pellegrini
*/

#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <scheduler/scheduler.h>
#include <mm/mm.h>

static bool first_call = true;
static unsigned int edge; // Don't recompute every time the square root


// TODO: we do not check here for a valid topology
unsigned int FindReceiver(int topology) {
 	double u;
 	// These must be unsigned. They are not checked for negative (wrong) values,
 	// but they would overflow, and are caught by a different check.
	unsigned int x, y, nx, ny;
	int direction;
	GID_t receiver;
	GID_t sender = current->gid;

	switch_to_platform_mode();


	if(unlikely(first_call)) {
		edge = sqrt(n_prc_tot);
		first_call = false;
	}


	// Very simple case!
	if(unlikely(n_prc_tot == 1)) {
		receiver = sender;
		goto out;
	}

	switch(topology) {

		case TOPOLOGY_HEXAGON:
			// Sanity check!
			if(unlikely(edge * edge != n_prc_tot)) {
				rootsim_error(true, "Hexagonal map wrongly specified!\n");
				return 0;
			}

			// Convert linear coords to hexagonal coords
			x = sender.to_int % edge;
			y = sender.to_int / edge;


			// Find a random neighbour
			do {
				// Select a random neighbour once, then move counter clockwise
				direction = 6 * Random() + 2;

				switch(direction) {
					case DIRECTION_NW:
						nx = (y % 2 == 0 ? x - 1 : x);
						ny = y - 1;
						break;
					case DIRECTION_NE:
						nx = (y % 2 == 0 ? x : x + 1);
						ny = y - 1;
						break;
					case DIRECTION_SW:
						nx = (y % 2 == 0 ? x - 1 : x);
						ny = y + 1;
						break;
					case DIRECTION_SE:
						nx = (y % 2 == 0 ? x : x + 1);
						ny = y + 1;
						break;
					case DIRECTION_E:
						nx = x + 1;
						ny = y;
						break;
					case DIRECTION_W:
						nx = x - 1;
						ny = y;
						break;
					default:
						rootsim_error(true, "Met an impossible condition. Aborting...\n");
				}

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates (this is a GID)
			set_gid(receiver, ny * edge + nx);

			break;


		case TOPOLOGY_SQUARE:
			// Sanity check!
			if(unlikely(edge * edge != n_prc_tot)) {
				rootsim_error(true, "Hexagonal map wrongly specified!\n");
				return 0;
			}

			// Convert linear coords to square coords
			x = sender.to_int % edge;
			y = sender.to_int / edge;

			// Find a random neighbour
			do {

				direction = 4 * Random();
				if(unlikely(direction == 4)) {
					direction = 3;
				}

				switch(direction) {
					case DIRECTION_N:
						nx = x;
						ny = y - 1;
						break;
					case DIRECTION_S:
						nx = x;
						ny = y + 1;
						break;
					case DIRECTION_E:
						nx = x + 1;
						ny = y;
						break;
					case DIRECTION_W:
						nx = x - 1;
						ny = y;
						break;
					default:
						rootsim_error(true, "Met an impossible condition. Aborting...\n");
				}

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:
			// Sanity check!
			if(unlikely(edge * edge != n_prc_tot)) {
				rootsim_error(true, "Hexagonal map wrongly specified!\n");
				return 0;
			}

			// Convert linear coords to square coords
			x = sender.to_int % edge;
			y = sender.to_int / edge;

			direction = 4 * Random();
			if(unlikely(direction == 4)) {
				direction = 3;
			}

			switch(direction) {
				case DIRECTION_N:
					nx = x;
					ny = y - 1;
					break;
				case DIRECTION_S:
					nx = x;
					ny = y + 1;
					break;
				case DIRECTION_E:
					nx = x + 1;
					ny = y;
					break;
				case DIRECTION_W:
					nx = x - 1;
					ny = y;
					break;
				default:
					rootsim_error(true, "Met an impossible condition. Aborting...\n");
			}

			// Check for wrapping around
			if(nx >= edge)
				nx = nx % edge;
			if(ny >= edge)
				ny = ny % edge;

			// Convert back to linear coordinates
			set_gid(receiver, ny * edge + nx);

			break;



		case TOPOLOGY_MESH:

			set_gid(receiver, (unsigned int)(n_prc_tot * Random()));
			break;



		case TOPOLOGY_BIDRING:

			u = Random();

			if (u < 0.5) {
				if(sender.to_int == 0) {
					set_gid(receiver, n_prc_tot - 1);
				} else {
					set_gid(receiver, sender.to_int - 1);
				}
			} else {
				set_gid(receiver, sender.to_int + 1);

				if (sender.to_int == n_prc_tot) {
					set_gid(receiver, 0);
				}
			}

			break;



		case TOPOLOGY_RING:

			set_gid(receiver, sender.to_int + 1);

			if (receiver.to_int == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;


		case TOPOLOGY_STAR:

			if (sender.to_int == 0) {
				do {
					set_gid(receiver, (unsigned int)(n_prc_tot * Random()));
				} while(receiver.to_int == n_prc_tot);
			} else {
				set_gid(receiver, 0);
			}

			break;

		default:
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

    out:
	switch_to_application_mode();
	return receiver.to_int;

}



// TODO: we do not check here for a valid topology nor direction
unsigned int GetReceiver(int topology, int direction) {
	GID_t receiver, sender;
	unsigned int x, y, nx, ny;

	switch_to_platform_mode();

	sender = current->gid;
	set_gid(receiver, INVALID_DIRECTION);

	if(unlikely(first_call)) {
		first_call = false;

		edge = sqrt(n_prc_tot);
		// Sanity check!
		if(unlikely(edge * edge != n_prc_tot)) {
			rootsim_error(true, "Hexagonal map wrongly specified!\n");
			return 0;
		}
	}

	// Convert linear coords to square coords
	x = sender.to_int % edge;
	y = sender.to_int / edge;

	switch(topology) {

		case TOPOLOGY_HEXAGON:

			switch(direction) {
				case DIRECTION_NW:
					nx = (y % 2 == 0 ? x - 1 : x);
					ny = y - 1;
					break;
				case DIRECTION_NE:
					nx = (y % 2 == 0 ? x : x + 1);
					ny = y - 1;
					break;
				case DIRECTION_SW:
					nx = (y % 2 == 0 ? x - 1 : x);
					ny = y + 1;
					break;
				case DIRECTION_SE:
					nx = (y % 2 == 0 ? x : x + 1);
					ny = y + 1;
					break;
				case DIRECTION_E:
					nx = x + 1;
					ny = y;
					break;
				case DIRECTION_W:
					nx = x - 1;
					ny = y;
					break;
				default:
					goto out;
			}

			if(unlikely(nx >= edge || ny >= edge))
				set_gid(receiver, INVALID_DIRECTION);
			else
				set_gid(receiver, ny * edge + nx);

			break;


		case TOPOLOGY_SQUARE:

			switch(direction) {
				case DIRECTION_N:
					nx = x;
					ny = y - 1;
					break;
				case DIRECTION_S:
					nx = x;
					ny = y + 1;
					break;
				case DIRECTION_E:
					nx = x + 1;
					ny = y;
					break;
				case DIRECTION_W:
					nx = x - 1;
					ny = y;
					break;
				default:
					goto out;
			}

			if(unlikely(x >= edge || ny >= edge))
				set_gid(receiver, INVALID_DIRECTION);
			else
				set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:

			switch(direction) {
				case DIRECTION_N:
					nx = x;
					ny = y - 1;
					break;
				case DIRECTION_S:
					nx = x;
					ny = y + 1;
					break;
				case DIRECTION_E:
					nx = x + 1;
					ny = y;
					break;
				case DIRECTION_W:
					nx = x - 1;
					ny = y;
					break;
				default:
					goto out;
			}

			// Check for wrapping around
			if(nx >= edge)
				nx = nx % edge;
			if(ny >= edge)
				ny = ny % edge;

			// Convert back to linear coordinates
			set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_BIDRING:

			if(direction == DIRECTION_W)
				set_gid(receiver, sender.to_int - 1);
			else if(direction == DIRECTION_E)
				set_gid(receiver, sender.to_int + 1);
			else
				goto out;

			if (receiver.to_int == UINT_MAX) {
				set_gid(receiver, n_prc_tot - 1);
			}
			if (receiver.to_int == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;


		case TOPOLOGY_RING:

			if(direction == DIRECTION_E)
				set_gid(receiver, sender.to_int + 1);
			else
				goto out;

			if (receiver.to_int == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;
	}

    out:
	switch_to_application_mode();
	return receiver.to_int;

}

// TODO: again, we should have a generic bitmap type in the simulator.
#define NUM_CHUNKS_PER_BLOCK 32

#define SET_BIT_AT(B, K) ( B |= (MASK << K) )
#define CHECK_BIT_AT(B, K) ( B & (MASK << K) )

#define CHECK_BIT(A, I) CHECK_BIT_AT((A)[(int)(I / NUM_CHUNKS_PER_BLOCK)], ((int)I % NUM_CHUNKS_PER_BLOCK))
#define SET_BIT(A, I) SET_BIT_AT((A)[(int)(I / NUM_CHUNKS_PER_BLOCK)], ((int)I % NUM_CHUNKS_PER_BLOCK))

void SetupObstacles(obstacles_t **obstacles)  {
	int bitmap_blocks;

	*obstacles = NULL;

	// Valid only for square regions
	if(unlikely(first_call)) {
		edge = sqrt(n_prc_tot);
		first_call = false;
	}
	if(unlikely(edge * edge != n_prc_tot)) {
		rootsim_error(true, "Hexagonal map wrongly specified!\n");
		return;
	}

	// Allocate a bitmap
	bitmap_blocks = n_prc_tot / NUM_CHUNKS_PER_BLOCK;
	if(bitmap_blocks < 1)
		bitmap_blocks = 1;
	*obstacles = __wrap_malloc(sizeof(obstacles_t) + sizeof(unsigned int) * bitmap_blocks);
	(*obstacles)->size = n_prc_tot;
}

void AddObstacles(obstacles_t *obstacles, int num, ...)  {
	va_list args;
	int i, cell;

	// Process variadic arguments
	va_start(args, num);

	for(i = 0; i < num; i++) {
		// Get the next cell to be set as an obstacle and set
		// it into the bitmap
		cell = va_arg(args, int);
		SET_BIT(obstacles->grid, cell);
	}

	va_end(args);
}

void AddObstacle(obstacles_t *obstacles, int cell)  {
	SET_BIT(obstacles->grid, cell);
}

void DiscardObstacles(obstacles_t *obstacles) {
	__wrap_free(obstacles);
}

typedef struct _astar_t {
	unsigned int num_steps;
	unsigned int *list;
} astar_t;


static int compare_astar(const void *a, const void *b) {
	return (int)(((astar_t *)a)->num_steps - ((astar_t *)b)->num_steps);
}

static astar_t a_star(unsigned int *a_star_bitmap, int topology, unsigned int current_cell, unsigned int dest, unsigned int step, obstacles_t *obstacles) {
	unsigned int i;
	unsigned int tentative_cell;

	astar_t states[8] = { [0 ... 7] = { UINT_MAX, NULL } };

	// TODO: again, we need a generic bitmap type in ROOT-Sim
	unsigned int tentative_a_star_bitmap[n_prc_tot / NUM_CHUNKS_PER_BLOCK + 1];

	// Is this the target?
	if (current_cell == dest) {
		states[0].list = rsalloc(sizeof(unsigned int) * step);
		states[0].list[step - 1] = current_cell;
		states[0].num_steps = step;
		return states[0];
	}

	// We have visited this cell
	SET_BIT(a_star_bitmap, current_cell);

	// Scan all possible neighbours depending on the actual topology
	for(i = 0; i < (topology == TOPOLOGY_SQUARE ? 4 : 8); i++) {
		tentative_cell = GetReceiver(TOPOLOGY_HEXAGON, i);

		// Try all reachable unvisited cells
		if(tentative_cell == INVALID_DIRECTION || CHECK_BIT(a_star_bitmap, tentative_cell) || CHECK_BIT(obstacles->grid, tentative_cell))
			continue;

		// Get closer to the destination
		memcpy(tentative_a_star_bitmap, a_star_bitmap, sizeof(unsigned int) * (n_prc_tot / NUM_CHUNKS_PER_BLOCK + 1));
		states[i] = a_star(tentative_a_star_bitmap, topology, tentative_cell, dest, step + 1, obstacles);
	}

	// Pick the state with the minimum distance
	qsort(states, (topology == TOPOLOGY_SQUARE ? 4 : 8), sizeof(astar_t), compare_astar);

	// Free unneeded states
	for(i = 1; i < (topology == TOPOLOGY_SQUARE ? 4 : 8); i++) {
		if(states[i].list != NULL) {
			rsfree(states[i].list);
		}
	}

	// Register me in the 0-th state's list
	states[0].list[step - 1] = current_cell;

	return states[0];
}


unsigned int ComputeMinTour(unsigned int **list, obstacles_t *obstacles, int topology, unsigned int source, unsigned int dest) {
	astar_t solution;

	// TODO: again, we need a generic bitmap type in ROOT-Sim
	unsigned int a_star_bitmap[n_prc_tot / NUM_CHUNKS_PER_BLOCK + 1];
	bzero(a_star_bitmap, sizeof(unsigned int) * (n_prc_tot / NUM_CHUNKS_PER_BLOCK + 1));

	// Sanity checks
	if(topology != TOPOLOGY_HEXAGON && topology != TOPOLOGY_SQUARE) {
		rootsim_error(true, "Invalid topology passed to ComputeMinTour().\n");
		return UINT_MAX;
	}
	if(source >= n_prc_tot) {
		rootsim_error(true, "Invalid source passed to ComputeMinTour(): %u.\n", source);
		return UINT_MAX;
	}
	if(dest >= n_prc_tot) {
		rootsim_error(true, "Invalid destination passed to ComputeMinTour(): %u.\n", dest);
		return UINT_MAX;
	}
	if(source == dest) {
		rootsim_error(true, "Asking ComputeMinTour() to find a path from a source equal to the destination\n");
		return UINT_MAX;
	}

	*list = NULL;
	solution = a_star(a_star_bitmap, topology, source, dest, 0, obstacles);

	if(solution.num_steps != UINT_MAX) {
		*list = __wrap_malloc(sizeof(unsigned int) * solution.num_steps);
		memcpy(*list, solution.list, sizeof(unsigned int) * solution.num_steps);
	}
	rsfree(solution.list);

	return solution.num_steps;
}

