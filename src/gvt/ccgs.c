/**
* @file gvt/ccgs.c
*
* @brief Consistent and Committed Global State
*
* Consistent and Committed Global State (CCGS) is a subsystem that (poeriodically)
* recomputes a global state on which the LPs can inspect the simulation trajectory
* and determine whether the simulation can stop, by relying on the OnGVT() callback.
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
* @author Francesco Quaglia
* @author Paolo Romano
* @author Alessandro Pellegrini
* @author Diego Cucuzzo
* @author Stefano D’Alessio
*
* @date 2007
*/

#include <stdbool.h>
#include <core/core.h>
#include <core/init.h>
#include <datatypes/bitmap.h>
#include <mm/mm.h>
#include <mm/state.h>
#include <communication/communication.h>
#include <communication/mpi.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>

/// This variable is an aggregate result for the distributed termination detection
static bool ccgs_completed_simulation = false;

/// In case termination detection is incremental, this bitmap keeps track of LPs that agreed upon simulation termination
static rootsim_bitmap *lps_termination;

inline bool ccgs_can_halt_simulation(void)
{
#ifdef HAVE_MPI
	return (ccgs_completed_simulation && all_kernels_terminated());
#else
	return ccgs_completed_simulation;
#endif
}

// Deve essere chiamata da un solo thread al GVT
void ccgs_reduce_termination(void)
{
	bool termination = true;

	/* Local termination:  all LPs need to agree */
#define reduce_local_termination(x) termination &= x
	bitmap_foreach_set(lps_termination, bitmap_required_size(n_prc), reduce_local_termination);
#undef reduce_local_termination

#ifdef HAVE_MPI
	/* If terminated locally check for global termination
	 * All other kernel need to terminated
	 */
	if (unlikely(!ccgs_completed_simulation && termination)) {
		broadcast_termination();
	}
#endif

	ccgs_completed_simulation = termination;
}

/**
* This function rebuilds a simulation state aligned to the new GVT for every LP.
* In this way, each LP is asked (via the OnGVT() callback) to check whether the
* simulation can terminate.
* Two different execution modes are available for this function, depending on
* the CCGS level in rootsim_config: committed and consistent.
* The committed version passes to the simulation model for validation the first
* simulation state that is found committed before the GVT commitment horizon.
* Nevertheless, two states passed to two different LPs can be associated with
* different timestamps.
* Running this function in consistent state, realignes the state of all LPs
* to the same simulation time, namely the GVT. This allows all LPs to agree
* on a consistent simulation time for termination detection. This induces, of course,
* an additional overhead, because the runtime environment has to reprocess in
* silent execution multiple events.
*
* @param time_barrier_pointer An array containing the time barrier states of the LPs
* 		as computed by the GVT subsystem
* @param gvt The Global Virtual Time value at which simulation states should be realigned
*               to generate a snapshot inspection which is also consistent
*/
void ccgs_compute_snapshot(state_t *time_barrier_pointer[])
{
	int i = -1;
	bool check_res = true;

	foreach_bound_lp(lp) {
		i++;

		// If termination detection is incremental, we could skip the current LP if already agreed for termination
		if (rootsim_config.check_termination_mode == CKTRM_INCREMENTAL && bitmap_check(lps_termination, lp->lid.to_int)) {
			continue;
		}

		if (time_barrier_pointer[i] == NULL)
			continue;

		// Check if after this event the LP agreed upon termination
		if(time_barrier_pointer[i]->last_event->simulation_completed)
			bitmap_set(lps_termination, lp->lid.to_int);
		check_res &= time_barrier_pointer[i]->last_event->simulation_completed;

		// Early stop
		if (rootsim_config.check_termination_mode == CKTRM_INCREMENTAL && !check_res) {
			break;
		}
	}
}


/**
 * @brief Evaluate if the simulation can be halted according to some LP.
 *
 * This function checks the current configuration for termination detection.
 * Depending on it, it will decide whether to call OnGVT() in the model, for a
 * certain LP.
 *
 * In particular, if the termination detection is incremental, the LP's OnGVT()
 * will be only called if the LP has not decided to terminate the simulation already.
 *
 * The information about the termination is stored in the `simulation_completed` member
 * of the event pointed by `bound`. This is consistent also with executions using
 * accurate termination detection.
 *
 * @param lp	The LP for which to call termination detection
 */
void ccgs_lp_can_halt(struct lp_struct *lp, bool for_checkpoint)
{
	if (rootsim_config.check_termination_mode == CKTRM_INCREMENTAL && bitmap_check(lps_termination, lp->lid.to_int)) {
		lp->bound->simulation_completed = true;
	}

	if(for_checkpoint || rootsim_config.check_termination_mode == CKTRM_ACCURATE)
		lp->bound->simulation_completed = CanTerminate(lp->gid.to_int, lp->current_base_pointer, lp->bound->timestamp);
}

void ccgs_init(void)
{
	lps_termination = malloc(bitmap_required_size(n_prc));
	bitmap_initialize(lps_termination, n_prc);
}

void ccgs_fini(void)
{
	rsfree(lps_termination);
}
