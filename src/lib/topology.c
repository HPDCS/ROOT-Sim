#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>


unsigned int FindReceiver(int topology) {

	// receiver is not unsigned, because we exploit -1 as a border case in the bidring topology.
	int receiver;
	unsigned int ret;
 	double u;

	switch_to_platform_mode();


 	// These must be unsigned. They are not checked for negative (wrong) values,
 	// but they would overflow, and are caught by a different check.
 	unsigned int edge;
 	unsigned int x, y, nx, ny;

	switch(topology) {

		case TOPOLOGY_HEXAGON:

			#define NW	0
			#define W	1
			#define SW	2
			#define SE	3
			#define E	4
			#define NE	5

			// Convert linear coords to hexagonal coords
			edge = sqrt(n_prc_tot);
			x = current_lp % edge;
			y = current_lp / edge;

			// Sanity check!
			if(edge * edge != n_prc_tot) {
				rootsim_error(true, "Hexagonal map wrongly specified!\n");
				return 0;
			}

			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = current_lp;
				break;
			}

			// Select a random neighbour once, then move counter clockwise
			receiver = 6 * Random();
			bool invalid = false;

			// Find a random neighbour
			do {
				if(invalid) {
					receiver = (receiver + 1) % 6;
				}

				switch(receiver) {
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
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

				invalid = true;

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			#undef NE
			#undef NW
			#undef W
			#undef SW
			#undef SE
			#undef E

			break;


		case TOPOLOGY_SQUARE:

			#define N	0
			#define W	1
			#define S	2
			#define E	3

			// Convert linear coords to square coords
			edge = sqrt(n_prc_tot);
			x = current_lp % edge;
			y = current_lp / edge;

			// Sanity check!
			if(edge * edge != n_prc_tot) {
				rootsim_error(true, "Square map wrongly specified!\n");
				return 0;
			}


			// Very simple case!
			if(n_prc_tot == 1) {
				receiver = current_lp;
				break;
			}

			// Find a random neighbour
			do {

				receiver = 4 * Random();
				if(receiver == 4) {
					receiver = 3;
				}

				switch(receiver) {
					case N:
						nx = x;
						ny = y - 1;
						break;
					case S:
						nx = x;
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
						rootsim_error(true, "Met an impossible condition at %s:%d. Aborting...\n", __FILE__, __LINE__);
				}

			// We don't check is nx < 0 || ny < 0, as they are unsigned and therefore overflow
			} while(nx >= edge || ny >= edge);

			// Convert back to linear coordinates
			receiver = (ny * edge + nx);

			#undef N
			#undef W
			#undef S
			#undef E

			break;



		case TOPOLOGY_MESH:

			receiver = (int)(n_prc_tot * Random());
			break;



		case TOPOLOGY_BIDRING:

			u = Random();

			if (u < 0.5) {
				receiver = current_lp - 1;
			} else {
				receiver= current_lp + 1;
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

			receiver= current_lp + 1;

			if ((unsigned int)receiver == n_prc_tot) {
				receiver = 0;
			}

			break;


		case TOPOLOGY_STAR:

			if (current_lp == 0) {
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

