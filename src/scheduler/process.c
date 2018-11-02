/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
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
* @file process.c
* @brief This header defines type-safe operations for LP control block management
* @author Alessandro Pellegrini
*
* @date December 14, 2017
*/

#include <limits.h>

#include <core/core.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>

LID_t idle_process;

/// Maintain LPs' simulation and execution states
LP_State **lps_blocks = NULL;

/** Each KLT has a binding towards some LPs. This is the structure used
 *  to keep track of LPs currently being handled
 */
__thread LP_State **lps_bound_blocks = NULL;

void initialize_control_blocks(void) {
	register unsigned int i;

	// Initialize the idle_process identifier
	set_lid(idle_process, UINT_MAX);

	// Allocate LPS control blocks
	lps_blocks = (LP_State **)rsalloc(n_prc * sizeof(LP_State *));
	for (i = 0; i < n_prc; i++) {
		lps_blocks[i] = (LP_State *)rsalloc(sizeof(LP_State));
		bzero(lps_blocks[i], sizeof(LP_State));

		// Allocate memory for the outgoing buffer
		lps_blocks[i]->outgoing_buffer.max_size = INIT_OUTGOING_MSG;
		lps_blocks[i]->outgoing_buffer.outgoing_msgs = rsalloc(sizeof(msg_t *) * INIT_OUTGOING_MSG);

		// Initialize bottom halves msg channel
		lps_blocks[i]->bottom_halves = init_channel();

		// That's the only sequentially executed place where we can set the lid
		lps_blocks[i]->lid.id = i;
	}
}

void initialize_binding_blocks(void) {
	lps_bound_blocks = (LP_State **)rsalloc(n_prc * sizeof(LP_State *));
	bzero(lps_bound_blocks, sizeof(LP_State *) * n_prc);
}

inline void LPS_bound_set(unsigned int entry, LP_State *lp_block) {
	lps_bound_blocks[entry] = lp_block;
}

inline int LPS_bound_foreach(int (*f)(LID_t, GID_t, unsigned int, void *), void *data) {
        LID_t lid;
        GID_t gid;
        unsigned int i;
	
        int ret = 0;

        for(i = 0; i < n_prc_per_thread; i++) {
		lid = LPS_bound(i)->lid;
                gid = LidToGid(lid);
                ret = f(lid, gid, lid_to_int(lid), data);
                if(unlikely(ret != 0))
                        break;
        }

        return ret;

}

inline int LPS_foreach(int (*f)(LID_t, GID_t, unsigned int, void *), void *data) {
	LID_t lid;
	GID_t gid;
	unsigned int i;
	int ret = 0;

	for(i = 0; i < n_prc; i++) {
		set_lid(lid, i);
		gid = LidToGid(lid);
		ret = f(lid, gid, i, data);
		if(unlikely(ret != 0))
			break;
	}

	return ret;
}

