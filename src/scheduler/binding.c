/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file binding.c
* @brief Implements load sharing rules for LPs among worker threads
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/

#include <stdbool.h>
#include <string.h>

#include <scheduler/process.h>
#include <scheduler/binding.h>

/// A guard to know whether this is the first invocation or not
static __thread bool first_lp_binding = true;

/** Each KLT has a binding towards some LPs. This is the structure used
 *  to keep track of LPs currently being handled
 */
__thread LP_state **LPS_bound = NULL;



/**
* Performs a (deterministic) block allocation between LPs and KLTs
*
* @author Alessandro Pellegrini
*/
static inline void LPs_block_binding(void) {
	unsigned int i, j;
	unsigned int buf1;
	unsigned int offset;
	unsigned int block_leftover;

	buf1 = (n_prc / n_cores);
	block_leftover = n_prc - buf1 * n_cores;

	if (block_leftover > 0) {
		buf1++;
	}

	n_prc_per_thread = 0;
	i = 0;
	offset = 0;
	while (i < n_prc) {
		j = 0;
		while (j < buf1) {
			if(offset == tid) {
				LPS_bound[n_prc_per_thread++] = LPS[i];
				LPS[i]->worker_thread = tid;
			}
			i++;
			j++;
		}
		offset++;
		block_leftover--;
		if (block_leftover == 0) {
			buf1--;
		}
	}	
}



/**
* Implements the knapsack load sharing policy in:
* 
* Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia
* A Load Sharing Architecture for Optimistic Simulations on Multi-Core Machines
* In Proceedings of the 19th International Conference on High Performance Computing (HiPC)
* Pune, India, IEEE Computer Society, December 2012.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/
static inline void LP_knapsack(void) {
	//~ LPS_bound[n_prc_per_thread++] = LPS[i];
	//~ LPS[i]->worker_thread = tid;
}


/**
* This function is used to create a temporary binding between LPs and KLT.
* The first time this function is called, each worker thread sets up its data
* structures, and the performs a (deterministic) block allocation. This is
* because no runtime data is available at the time, so we "share" the load
* as the number of LPs.
* Then, successive invocations, will use the knapsack load sharing policy

* @author Alessandro Pellegrini
* @author Roberto Vitali
*/
void rebind_LPs(void) {

	if(first_lp_binding) {
		first_lp_binding = false;
		
		// Binding metadata are used in the platform to perform
		// operations on LPs in isolation
		LPS_bound = rsalloc(sizeof(LP_state *) * n_prc);
		bzero(LPS_bound, sizeof(LP_state *) * n_prc);

		LPs_block_binding();

		return;
	}
}
