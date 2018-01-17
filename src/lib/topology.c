#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>

static bool first_call = true;
static unsigned int edge; // Don't recompute every time the square root


// TODO: we do not check here for a valid topology
unsigned int FindReceiver(int topology) {
 	double u;
	GID_t sender, receiver;
	int direction;
 	// These must be unsigned. They are not checked for negative (wrong) values,
 	// but they would overflow, and are caught by a different check.
	unsigned int x, y, nx, ny;

	switch_to_platform_mode();

	sender = LidToGid(current_lp);

	if(first_call) {
		edge = sqrt(n_prc_tot);
		// Sanity check!
		if(edge * edge != n_prc_tot) {
			rootsim_error(true, "Hexagonal map wrongly specified!\n");
			return 0;
		}
		first_call = false;
	}


	// Very simple case!
	if(n_prc_tot == 1) {
		receiver = sender;
		goto out;
	}

	switch(topology) {

		case TOPOLOGY_HEXAGON:

			// Convert linear coords to hexagonal coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;


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
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates (this is a GID)
			set_gid(receiver, ny * edge + nx);

			break;


		case TOPOLOGY_SQUARE:

			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			// Find a random neighbour
			do {

				direction = 4 * Random();
				if(direction == 4) {
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
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			set_gid(receiver, ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:

			// Convert linear coords to square coords
			x = gid_to_int(sender) % edge;
			y = gid_to_int(sender) / edge;

			direction = 4 * Random();
			if(direction == 4) {
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

			set_gid(receiver, (unsigned int)(n_prc_tot * Random()));
			break;



		case TOPOLOGY_BIDRING:

			u = Random();

			if (u < 0.5) {
				if(gid_to_int(receiver) == 0) {
					set_gid(receiver, n_prc_tot - 1);
				} else {
					set_gid(receiver, gid_to_int(sender) - 1);
				}
			} else {
				set_gid(receiver, gid_to_int(sender) + 1);

				if (gid_to_int(receiver) == n_prc_tot) {
					set_gid(receiver, 0);
				}
			}

			break;



		case TOPOLOGY_RING:

			set_gid(receiver, gid_to_int(sender) + 1);

			if (gid_to_int(receiver) == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;


		case TOPOLOGY_STAR:

			if (gid_to_int(sender) == 0) {
				do {
					set_gid(receiver, (unsigned int)(n_prc_tot * Random()));
				} while(gid_to_int(receiver) == n_prc_tot);
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
unsigned int GetReceiver(int topology, int direction) {
	GID_t receiver, sender;
	unsigned int x, y, nx, ny;

	switch_to_platform_mode();

	sender = LidToGid(current_lp);
	set_gid(receiver, INVALID_DIRECTION);

	if(first_call) {
		first_call = false;

		edge = sqrt(n_prc_tot);
		// Sanity check!
		if(edge * edge != n_prc_tot) {
			rootsim_error(true, "Hexagonal map wrongly specified!\n");
			return 0;
		}
	}

	// Convert linear coords to square coords
	x = gid_to_int(sender) % edge;
	y = gid_to_int(sender) / edge;

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

			if(nx >= edge || ny >= edge)
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

			if(nx >= edge || ny >= edge)
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
				set_gid(receiver, gid_to_int(sender) - 1);
			else if(direction == DIRECTION_E)
				set_gid(receiver, gid_to_int(sender) + 1);
			else
				goto out;

   			if (gid_to_int(receiver) == UINT_MAX) {
				set_gid(receiver, n_prc_tot - 1);
			}
			if (gid_to_int(receiver) == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;


		case TOPOLOGY_RING:

			if(direction == DIRECTION_E)
				set_gid(receiver, gid_to_int(sender) + 1);
			else
				goto out;

			if (gid_to_int(receiver) == n_prc_tot) {
				set_gid(receiver, 0);
			}

			break;
	}

    out:
	switch_to_application_mode();
	return gid_to_int(receiver);

}
