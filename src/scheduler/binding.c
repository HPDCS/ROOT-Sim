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


static __thread bool already_allocated = false;


/**
* This function is used to create a temporary binding between LPs and KLT.
* Whenever it is invoked, the binding is recreated, depending on the specified
* policy. Currently, only a fixed binding is implemented, so calling again this
* function deterministically regenerates the same binding.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/
void rebind_LPs(void) {
	unsigned int i, j;
	unsigned int buf1;
	unsigned int offset;
	unsigned int block_leftover;

	// This is a guard because it's meaningless to recalculate a static
	// LP allocation now.
	if(already_allocated) {
		return;
	}

	already_allocated = true;

	if(LPS_bound == NULL) {
		LPS_bound = rsalloc(sizeof(LP_state *) * n_prc);
		bzero(LPS_bound, sizeof(LP_state *) * n_prc);
	}

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
