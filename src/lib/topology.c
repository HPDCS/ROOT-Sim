#include <ROOT-Sim.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <scheduler/scheduler.h>
#include <datatypes/bitmap.h>

static unsigned int edge; // Don't recompute every time the square root

static void compute_edge(void) {
	static bool first_call = true;
	if(first_call) {
		first_call = false;

		edge = sqrt(n_prc_tot);

		// Sanity check!
		if(edge * edge != n_prc_tot) {
			rootsim_error(true, "Map wrongly specified!\n");
		}
	}
}

// TODO: we do not check here for a valid topology
unsigned int FindReceiver(topology_t topology) {
	// These must be unsigned. They are not checked for negative (wrong) values,
	// but they would overflow, and are caught by a different check.
	unsigned int x, y, nx, ny;
	direction_t direction;
	GID_t receiver;
	GID_t sender = LidToGid(current_lp);

	switch_to_platform_mode();

	// Very simple case!
	if(n_prc_tot == 1) {
		receiver = sender;
		goto out;
	}

	switch (topology) {

		case TOPOLOGY_HEXAGON:

			compute_edge();
			// Convert linear coords to hexagonal coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			// Find a random neighbour
			do {
				// Select a random neighbour once, then move counter clockwise
				direction = 6 * Random() + 2;

				switch (direction) {
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
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

				// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while (nx >= edge || ny >= edge);

			// Convert back to linear coordinates (this is a GID)
			set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_SQUARE:

			compute_edge();

			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			// Find a random neighbour
			do {

				direction = 4 * Random();

				switch (direction) {
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
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

				// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while (nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:

			compute_edge();

			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			direction = 4 * Random();

			switch (direction) {
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
					rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
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

			set_gid(receiver, (unsigned int )(n_prc_tot * Random()));

			break;

		case TOPOLOGY_BIDRING:

			if(Random() < 0.5) {
				if(gid_to_int(sender) == 0) {
					set_gid(receiver, n_prc_tot - 1);
				} else {
					set_gid(receiver, gid_to_int(sender) - 1);
				}
			} else {
				set_gid(receiver, gid_to_int(sender) + 1);

				if(gid_to_int(sender) == n_prc_tot) {
					set_gid(receiver, 0);
				}
			}

			break;

		case TOPOLOGY_RING:

			set_gid(receiver, gid_to_int(sender) + 1);

			if(gid_to_int(receiver) == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;

		case TOPOLOGY_STAR:

			if(gid_to_int(sender) == 0) {
				set_gid(receiver, (unsigned int )((n_prc_tot - 1) * Random()) + 1);
			} else {
				set_gid(receiver, 0);
			}

			break;

		default:
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	out:
	switch_to_application_mode();
	return gid_to_int(receiver);

}

// TODO: we do not check here for a valid topology nor direction
unsigned int GetReceiver(topology_t topology, unsigned int from, direction_t direction) {
	GID_t receiver, sender;
	unsigned int x, y, nx, ny;

	switch_to_platform_mode();

	set_gid(sender, from);
	set_gid(receiver, DIRECTION_INVALID);

	switch (topology) {

		case TOPOLOGY_HEXAGON:

			compute_edge();
			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			switch (direction) {
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

			if(nx >= edge || ny >= edge)
				set_gid(receiver, DIRECTION_INVALID);
			else
				set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_SQUARE:

			compute_edge();
			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			switch (direction) {
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

			if(nx >= edge || ny >= edge)
				set_gid(receiver, DIRECTION_INVALID);
			else
				set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:

			compute_edge();
			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			switch (direction) {
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
				set_gid(receiver, gid_to_int(sender) - 1);
			else if(direction == DIRECTION_E)
				set_gid(receiver, gid_to_int(sender) + 1);
			else
				goto out;

			if(gid_to_int(receiver) == UINT_MAX) {
				set_gid(receiver, n_prc_tot - 1);
			}
			if(gid_to_int(receiver) == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;

		case TOPOLOGY_RING:

			if(direction == DIRECTION_E)
				set_gid(receiver, gid_to_int(sender) + 1);
			else
				goto out;

			if(gid_to_int(receiver) == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;

		default:
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	out:
	switch_to_application_mode();
	return gid_to_int(receiver);

}

struct obstacles_t {
	void* fake_member;
};

obstacles_t* NewObstacles(void) {

	obstacles_t *ret = __wrap_malloc(bitmap_required_size(n_prc_tot));

	if(ret == NULL)
		rootsim_error(true, "Unable to allocate the obstacle grid\n");

	bitmap_initialize(ret, n_prc_tot);

	return ret;

}

int AddObstacles(obstacles_t *obstacles, unsigned int num, ...) {
	va_list args;
	unsigned int cell;

	// Process variadic arguments
	va_start(args, num);

	while (num--) {
		// Get the next cell to be set as an obstacle and set
		// it into the bitmap
		cell = va_arg(args, unsigned int);
		if(cell >= n_prc_tot)
			return -1;
		bitmap_set(obstacles, cell);
	}

	va_end(args);
	return 0;
}

int AddObstacle(obstacles_t *obstacles, unsigned int cell) {
	if(cell >= n_prc_tot)
		return -1;
	bitmap_set(obstacles, cell);
	return 0;
}

void FreeObstacles(obstacles_t *obstacles) {
	__wrap_free(obstacles);
}

bool IsObstacle(obstacles_t *obstacles, unsigned int cell) {
	if(cell >= n_prc_tot)
		return false;
	return bitmap_check(obstacles, cell);
}

#define ABS_VAL(x) ((x) > 0 ? (x) : -(x))

// this computes the vector connecting cell "from" to cell "to", it is needed for later computation
static void compute_vector(topology_t topology, unsigned int from, unsigned int to, int *vx, int *vy) {
	if(topology == TOPOLOGY_SQUARE) {

		// directed vector on plane
		*vx = (int) (to % edge) - (int) (from % edge);
		*vy = (int) (to / edge) - (int) (from / edge);

	} else if(topology == TOPOLOGY_HEXAGON) {

		// directed vector in axial coordinates (my reference: https://www.redblobgames.com/grids/hexagons/)
		*vx = (to % edge) - (to / edge) / 2 - (from % edge) + (from / edge) / 2;
		*vy = (to / edge) - (from / edge);

	} else {
		rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
	}
}

// this computes the minimum possible tour length between two cells
static unsigned int min_distance(topology_t topology, int vx, int vy) {
	if(topology == TOPOLOGY_SQUARE) {

		return (unsigned int) ABS_VAL(vx) + (unsigned int) ABS_VAL(vy);

	} else if(topology == TOPOLOGY_HEXAGON) {

		return ((unsigned int) ABS_VAL(vx) + (unsigned int) ABS_VAL(vy) + (unsigned int) ABS_VAL(vx + vy)) / 2;
	}

	rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
	return UINT_MAX; // impossible
}

/* a simple heuristics which assumes it is more likely to find an optimal path
 *  if we go in the destination's direction. This returns the directions ordered by "likelihood".
 */
static void likely_directions(topology_t topology, int vx, int vy, direction_t directions[6]) {
	int i, dir_ne, dir_e, dir_nw;
	if(topology == TOPOLOGY_SQUARE) {

		// check whether x component is more relevant than y component
		if(ABS_VAL(vx) > ABS_VAL(vy)) {
			// simple cartesian considerations
			directions[0] = vx > 0 ? DIRECTION_E : DIRECTION_W;
			directions[1] = vy > 0 ? DIRECTION_S : DIRECTION_N;
			directions[2] = vy > 0 ? DIRECTION_N : DIRECTION_S;
			directions[3] = vx > 0 ? DIRECTION_W : DIRECTION_E;

		} else {
			directions[0] = vy > 0 ? DIRECTION_S : DIRECTION_N;
			directions[1] = vx > 0 ? DIRECTION_E : DIRECTION_W;
			directions[2] = vx > 0 ? DIRECTION_W : DIRECTION_E;
			directions[3] = vy > 0 ? DIRECTION_N : DIRECTION_S;
		}

	} else if(topology == TOPOLOGY_HEXAGON) {
		// these are the projections on the directions EAST, NORTHEAST, NORTHWEST
		dir_e = vx - vy;
		dir_ne = 2 * vx + vy;
		dir_nw = 2 * vy + vx;

		// this is a little generalization of the square case written in a more concise pattern
		i = (ABS_VAL(dir_e) < ABS_VAL(dir_ne)) + (ABS_VAL(dir_e) < ABS_VAL(dir_nw));
		directions[i] = (dir_e) > 0 ? DIRECTION_E : DIRECTION_W;
		directions[5 - i] = (dir_e) > 0 ? DIRECTION_W : DIRECTION_E;

		i = (ABS_VAL(dir_ne) <= ABS_VAL(dir_e)) + (ABS_VAL(dir_ne) < ABS_VAL(dir_nw));
		directions[i] = (dir_ne) > 0 ? DIRECTION_NE : DIRECTION_SW;
		directions[5 - i] = (dir_ne) > 0 ? DIRECTION_SW : DIRECTION_NE;

		i = (ABS_VAL(dir_nw) <= ABS_VAL(dir_e)) + (ABS_VAL(dir_nw) <= ABS_VAL(dir_ne));
		directions[i] = (dir_nw) > 0 ? DIRECTION_NW : DIRECTION_SE;
		directions[5 - i] = (dir_nw) > 0 ? DIRECTION_SE : DIRECTION_NW;

	} else {
		rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
	}

}

#undef ABS_VAL

// The a_star method.
/* Non obvious arguments:
 * @param min_steps: a reference to an array with values initialized to UINT_MAX which is needed to keep track of best steps to
 * 					visited cells, min_steps[dest] will hold the number of steps of the best tour or UINT_MAX if that tour doesn't exist
 * @param solution: a reference to an array which will hold the solution.
 */
static void a_star(rootsim_bitmap *visited_bitmap, topology_t topology, unsigned int current_cell, unsigned int dest, unsigned int step,
		unsigned int min_steps[n_prc_tot], unsigned int solution[n_prc_tot]) {

	direction_t ordered_directions[6];
	unsigned int i;
	unsigned int tentative_cell;
	unsigned int tentative_steps;
	int vx, vy;
	bool found_solution = false;

	// printf("a_star(current_cell: %u, destination: %u, step: %u)\n", current_cell, dest, step);

	// this vector is useful for later computation
	compute_vector(topology, current_cell, dest, &vx, &vy);

	// we are processing a useless path since we already found a necessarily shorter one
	if(step >= min_steps[current_cell] || step + min_distance(topology, vx, vy) >= min_steps[dest]) {
		return;
	}
	// this is the best route found so far from the initial source to this cell
	min_steps[current_cell] = step;

	// Is this the target?
	if(current_cell == dest) {
		//printf("Destination reached! %u\n", step);
		solution[step - 1] = current_cell;
		return;
	}

	// Mark this cell as visited
	bitmap_set(visited_bitmap, current_cell);

	// Save current minimum steps count
	tentative_steps = min_steps[dest];

	// A very primitive heuristic optimization
	likely_directions(topology, vx, vy, ordered_directions);

	// Scan all possible neighbours depending on the actual topology
	for (i = 0; i < (topology == TOPOLOGY_SQUARE ? 4 : 6); i++) {

		tentative_cell = GetReceiver(topology, current_cell, ordered_directions[i]);

		// Try all reachable unvisited cells
		if(tentative_cell == DIRECTION_INVALID || bitmap_check(visited_bitmap, tentative_cell))
			continue;

		// Get closer to the destination
		a_star(visited_bitmap, topology, tentative_cell, dest, step + 1, min_steps, solution);

		// if min_steps decreased than it means we found a candidate solution
		if(min_steps[dest] < tentative_steps) {
			found_solution = true;
		}

	}

	// Mark this cell as not visited
	bitmap_reset(visited_bitmap, current_cell);

	// If this recursion branch is part of a candidate solution then we keep track of it
	// Last candidate solution must be the best one. (We don't mark the original source)
	if(found_solution && step)
		solution[step - 1] = current_cell;

	return;
}

unsigned int ComputeMinTour(unsigned int result[n_prc_tot], obstacles_t *obstacles, topology_t topology,
		unsigned int source, unsigned int dest) {

	unsigned int a_star_solution[n_prc_tot];
	unsigned int min_steps[n_prc_tot];

	rootsim_bitmap visited_bitmap[bitmap_required_size(n_prc_tot)];

	// This sets the min_steps entries at UINT_MAX
	memset(min_steps, UCHAR_MAX, sizeof(unsigned int) * n_prc_tot);

	// It is sufficient to mark the obstacles as visited to skip them, this saves several resources
	memcpy(visited_bitmap, obstacles, bitmap_required_size(n_prc_tot));

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

	compute_edge();
	a_star(visited_bitmap, topology, source, dest, 0, min_steps, a_star_solution);

	if(min_steps[dest] != UINT_MAX) {
		memcpy(result, a_star_solution, sizeof(unsigned int) * min_steps[dest]);
	}

	return min_steps[dest];
}
