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
#include <mm/mm.h>
#include <mm/state.h>
#include <communication/communication.h>
#include <communication/mpi.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>

/// This variable is an aggregate result for the distributed termination detection
static bool ccgs_completed_simulation = false;

/// In case termination detection is incremental, this array keeps track of LPs that think the simulation can be halted already
static bool *lps_termination;

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
	register unsigned int i;
	bool termination = true;

	/* Local termination:  all LPs need to be terminated */
	for (i = 0; i < n_prc; i++) {
		termination &= lps_termination[i];
	}

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

		// If termination detection is incremental, we skip the current LP
		if (rootsim_config.check_termination_mode == CKTRM_INCREMENTAL && lps_termination[lp->lid.to_int]) {
			continue;
		}

		if (time_barrier_pointer[i] == NULL)
			continue;

		// Call the application to check termination
		lps_termination[lp->lid.to_int] = time_barrier_pointer[i]->simulation_completed;
		check_res &= lps_termination[lp->lid.to_int];

		// Early stop
		if (rootsim_config.check_termination_mode == CKTRM_INCREMENTAL && !check_res) {
			break;
		}

	}

	// No real LP is running now!
	current = NULL;
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
 * @param lp	The LP for which to call termination detection
 * @return 	@c true if the simulation should terminate according to the LP,
 * 		@c false otherwise.
 */
bool ccgs_lp_can_halt(struct lp_struct *lp)
{
	if (rootsim_config.simulation_time > 0 || rootsim_config.wallclock_time > 0)
		return false;

	if (rootsim_config.check_termination_mode == CKTRM_INCREMENTAL && lps_termination[lp->lid.to_int]) {
		return true;
	}
	return lp->OnGVT(lp->gid.to_int, lp->current_base_pointer);
}

void ccgs_init(void)
{
	lps_termination = rsalloc(sizeof(bool) * n_prc);
	memset(lps_termination, 0, sizeof(bool) * n_prc);
}

void ccgs_fini(void)
{
	rsfree(lps_termination);
}
