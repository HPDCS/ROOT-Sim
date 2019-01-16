/**
 * @file scheduler/process.c
 *
 * @brief Generic LP management functions
 *
 * Generic LP management functions
 *
 * @copyright
 * Copyright (C) 2008-2019 HPDCS Group
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
 * @author Francesco Quaglia
 *
 * @date December 14, 2017
 */

#include <limits.h>

#include <core/core.h>
#include <core/init.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>

// TODO: see issue #121 to see how to make this ugly hack disappear
__thread unsigned int __lp_counter = 0;
__thread unsigned int __lp_bound_counter = 0;

/// Maintain LPs' simulation and execution states
struct lp_struct **lps_blocks = NULL;

/** Each KLT has a binding towards some LPs. This is the structure used
 *  to keep track of LPs currently being handled
 */
__thread struct lp_struct **lps_bound_blocks = NULL;

void initialize_binding_blocks(void)
{
	lps_bound_blocks =
	    (struct lp_struct **)rsalloc(n_prc * sizeof(struct lp_struct *));
	bzero(lps_bound_blocks, sizeof(struct lp_struct *) * n_prc);
}

void initialize_lps(void)
{
	unsigned int i, j;
	unsigned int lid = 0;
	struct lp_struct *lp;
	unsigned int local = 0;
	GID_t gid;

	// First of all, determine what LPs should be locally hosted.
	// Only for them, we are creating struct lp_structs here.
	distribute_lps_on_kernels();

	// We now know how many LPs should be locally hosted. Prepare
	// the place for their control blocks.
	lps_blocks =
	    (struct lp_struct **)rsalloc(n_prc * sizeof(struct lp_struct *));

	// We now iterate over all LP Gids. Everytime that we find an LP
	// which should be locally hosted, we create the local lp_struct
	// process control block.
	for (i = 0; i < n_prc_tot; i++) {
		set_gid(gid, i);
		if (find_kernel_by_gid(gid) != kid)
			continue;

		// Initialize the control block for the current lp
		lp = (struct lp_struct *)rsalloc(sizeof(struct lp_struct));
		bzero(lp, sizeof(struct lp_struct));
		lps_blocks[local++] = lp;

		if (local > n_prc) {
			printf("reached local %d\n", local);
			fflush(stdout);
			abort();
		}
		// Initialize memory map
		initialize_memory_map(lp);

		// Allocate memory for the outgoing buffer
		lp->outgoing_buffer.max_size = INIT_OUTGOING_MSG;
		lp->outgoing_buffer.outgoing_msgs =
		    rsalloc(sizeof(msg_t *) * INIT_OUTGOING_MSG);

		// Initialize bottom halves msg channel
		lp->bottom_halves = init_channel();

		// We sequentially assign lids, and use the current gid
		lp->lid.to_int = lid++;
		lp->gid = gid;

		// Which version of OnGVT and ProcessEvent should we use?
		if (rootsim_config.snapshot == SNAPSHOT_FULL) {
			lp->OnGVT = &OnGVT_light;
			lp->ProcessEvent = &ProcessEvent_light;
		}		// TODO: add here an else for ISS

		// Allocate LP stack
		lp->stack = get_ult_stack(LP_STACK_SIZE);

		// Set the initial checkpointing period for this LP.
		// If the checkpointing period is fixed, this will not change during the
		// execution. Otherwise, new calls to this function will (locally) update
		// this.
		set_checkpoint_period(lp, rootsim_config.ckpt_period);

		// Initially, every LP is ready
		lp->state = LP_STATE_READY;

		// There is no current state layout at the beginning
		lp->current_base_pointer = NULL;

		// Initialize the queues
		lp->queue_in = new_list(msg_t);
		lp->queue_out = new_list(msg_hdr_t);
		lp->queue_states = new_list(state_t);
		lp->rendezvous_queue = new_list(msg_t);

		// No event has been processed so far
		lp->bound = NULL;

		// We have no information about messages still to be delivered to this LP
		lp->outgoing_buffer.min_in_transit = rsalloc(sizeof(simtime_t) * n_cores);
		for (j = 0; j < n_cores; j++) {
			lp->outgoing_buffer.min_in_transit[j] = INFTY;
		}

#ifdef HAVE_CROSS_STATE
		// No read/write dependencies open so far for the LP. The current lp is always opened
		lp->ECS_index = 0;
		lp->ECS_synch_table[0] = LidToGid(lp);	// LidToGid for distributed ECS
#endif

		// Create User-Level Thread
		context_create(&lp->context, LP_main_loop, NULL, lp->stack,
			       LP_STACK_SIZE);
	}
}

// This works only for locally-hosted LPs!
struct lp_struct *find_lp_by_gid(GID_t gid)
{
	foreach_lp(lp) {
		if (lp->gid.to_int == gid.to_int)
			return lp;
	}
	return NULL;
}
