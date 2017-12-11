#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>

static bool first_call = true;
static unsigned int edge; // Don't recompute every time the square root

unsigned int FindReceiver(int topology) {
	// receiver is not unsigned, because we exploit -1 as a corner case in the bidring topology.
	int receiver;
	unsigned int ret;
 	double u;
	bool invalid = false;
	unsigned int sender;

	if(rootsim_config.serial)
		sender = current_lp;
	else
		sender = LidToGid(current_lp);

 	// These must be unsigned. They are not checked for negative (wrong) values,
 	// but they would overflow, and are caught by a different check.
	unsigned int x, y, nx, ny;

	switch_to_platform_mode();

	if(first_call) {
		edge = sqrt(n_prc_tot);
		// Sanity check!
		if(edge * edge != n_prc_tot) {
			rootsim_error(true, "Hexagonal map wrongly specified!\n");
			return 0;
		}
		first_call = false;
	}

	switch(topology) {

		case TOPOLOGY_HEXAGON:

			// Convert linear coords to hexagonal coords
			x = sender % edge;
			y = sender / edge;

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = sender;
				break;
			}

			// Select a random neighbour once, then move counter clockwise
			receiver = 6 * Random() + 2;

			// Find a random neighbour
			do {
				if(invalid) {
					receiver = ((receiver + 1) % 8);
					receiver = (receiver == 0 ? 2 : receiver);
				}

				switch(receiver) {
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

				invalid = true;

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			break;


		case TOPOLOGY_SQUARE:

			// Convert linear coords to square coords
			x = sender % edge;
			y = sender / edge;

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = sender;
				break;
			}

			// Find a random neighbour
			do {

				receiver = 4 * Random();
				if(receiver == 4) {
					receiver = 3;
				}

				switch(receiver) {
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
			receiver = (ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:

			// Convert linear coords to square coords
			x = sender % edge;
			y = sender / edge;

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = sender;
				break;
			}

			receiver = 4 * Random();
			if(receiver == 4) {
				receiver = 3;
			}

			switch(receiver) {
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
			receiver = (ny * edge + nx);

			break;



		case TOPOLOGY_MESH:

			receiver = (int)(n_prc_tot * Random());
			break;



		case TOPOLOGY_BIDRING:

			u = Random();

			if (u < 0.5) {
				receiver = sender - 1;
			} else {
				receiver= sender + 1;
			}

   			if (receiver == -1) {
				receiver = n_prc_tot - 1;
			}

			// Can't be negative from now on
			if ((unsigned int)receiver == n_prc_tot) {
				receiver = 0;
			}

			break;



		case TOPOLOGY_RING:

			receiver= sender + 1;

			if ((unsigned int)receiver == n_prc_tot) {
				receiver = 0;
			}

			break;


		case TOPOLOGY_STAR:

			if (sender == 0) {
				receiver = (int)(n_prc_tot * Random());
			} else {
				receiver = 0;
			}

			break;

		default:
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	ret = (unsigned int)receiver;

	switch_to_application_mode();
	return ret;

}



int GetReceiver(int topology, int direction) {
	int receiver = -1;
	unsigned int x, y, nx, ny;
	unsigned int sender = LidToGid(current_lp);

	switch_to_platform_mode();

	if(first_call) {
		first_call = false;

		edge = sqrt(n_prc_tot);
		// Sanity check!
		if(edge * edge != n_prc_tot) {
			rootsim_error(true, "Hexagonal map wrongly specified!\n");
			return 0;
		}
	}

	switch(topology) {

		case TOPOLOGY_HEXAGON:

			// Convert linear coords to hexagonal coords
			x = sender % edge;
			y = sender / edge;

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = sender;
				break;
			}

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
				receiver = -1;
			else
				receiver = (ny * edge + nx);

			break;


		case TOPOLOGY_SQUARE:

			// Convert linear coords to square coords
			x = sender % edge;
			y = sender / edge;

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = sender;
				break;
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
					goto out;
			}

			if(nx >= edge || ny >= edge)
				receiver = -1;
			else
				receiver = (ny * edge + nx);

			break;

		case TOPOLOGY_TORUS:
			// Convert linear coords to square coords
			x = sender % edge;
			y = sender / edge;

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = sender;
				break;
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
					goto out;
			}

			// Check for wrapping around
			if(nx >= edge)
				nx = nx % edge;
			if(ny >= edge)
				ny = ny % edge;

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			break;

		case TOPOLOGY_BIDRING:

			if(direction == DIRECTION_W)
				receiver = sender - 1;
			else if(direction == DIRECTION_E)
				receiver = sender + 1;
			else
				goto out;

   			if (receiver == -1) {
				receiver = n_prc_tot - 1;
			}
			if ((unsigned int)receiver == n_prc_tot) {
				receiver = 0;
			}

			break;


		case TOPOLOGY_RING:

			if(direction == DIRECTION_E)
				receiver= sender + 1;
			else
				goto out;

			if ((unsigned int)receiver == n_prc_tot) {
				receiver = 0;
			}

			break;
	}

    out:
	switch_to_application_mode();
	return receiver;

}
