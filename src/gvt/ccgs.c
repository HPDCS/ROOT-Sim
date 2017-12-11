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
* @file ccgs.c
* @brief Consistent and Committed Global State (CCGS) is a subsystem that (poeriodically)
* 	recomputes a global state on which the LPs can inspect the simulation trajectory
* 	and determine whether the simulation can stop, by relying on the OnGVT() callback.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/

#include <stdbool.h>
#include <core/core.h>
#include <mm/dymelor.h>
#include <mm/state.h>
#include <communication/communication.h>
#include <communication/mpi.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>



/// This variable is an aggregate result for the distributed termination detection
static bool ccgs_completed_simulation = false;

/// In case termination detection is incremental, this array keeps track of LPs that think the simulation can be halted already
static bool lps_termination[MAX_LPs];



inline bool ccgs_can_halt_simulation(void) {
	#ifdef HAS_MPI
	return (ccgs_completed_simulation && all_kernels_terminated());
	#else
	return ccgs_completed_simulation;
	#endif
}


// Deve essere chiamata da un solo thread al GVT
void ccgs_reduce_termination(void) {
	register unsigned int i;
	bool termination = true;

	/* Local termination:  all LPs need to be terminated */
	for(i = 0; i < n_prc; i++) {
		termination &= lps_termination[i];
	}

	#ifdef HAS_MPI
	/* If terminated locally check for global termination
	 * All other kernel need to terminated
	 */
	if(!ccgs_completed_simulation && termination){
		broadcast_termination();
	}
	#endif

	ccgs_completed_simulation = termination;
}


/**
* This function rebuilds a simulation state aligned to the new GVT for every LP.
* In this way, each LP is asked (via the OnGVT() callback) to check whether the
* simulation can
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param time_barrier_pointer An array containing the time barrier states of the LPs
* 		as computed by the GVT subsystem
*/
void ccgs_compute_snapshot(state_t *time_barrier_pointer[], simtime_t gvt) {

	bool check_res = true;
	register unsigned int i;
	register unsigned int lid;

	state_t temporary_log;
//	msg_t *realignment_evt;
	(void)gvt; // This is required in state reconstruction which is currently commented out

	for(i = 0; i < n_prc_per_thread; i++) {

		lid = LPS_bound[i]->lid;

		// If termination detection is incremental, we skip the current LP
		if(rootsim_config.check_termination_mode == INCR_CKTRM && lps_termination[lid]) {
			continue;
		}

		if(time_barrier_pointer[i] == NULL)
			continue;

		// TODO: realign LogState and RestoreState to be compliant with the execution in the committed portion

		// Log the current state so that after we can restore it.
		current_lp = lid;
		current_lvt = lvt(lid);
		temporary_log.log = log_state(lid);
		temporary_log.state = LPS[lid]->state;
		temporary_log.base_pointer = LPS[lid]->current_base_pointer;

		// Restore the time barrier state
		current_lvt = time_barrier_pointer[i]->lvt;
		log_restore(lid, time_barrier_pointer[i]);
		LPS[lid]->state = time_barrier_pointer[i]->state;
		LPS[lid]->current_base_pointer = time_barrier_pointer[i]->base_pointer;

/*
		// If the LP is not blocked, we can reconstruct the state exactly to the GVT
		if(!is_blocked_state(LPS[lid]->state))  {
			// Realign the state to the current GVT value
			realignment_evt = list_next(time_barrier_pointer[i]->last_event);
			while(realignment_evt != NULL && realignment_evt->timestamp < gvt) {
				realignment_evt = list_next(realignment_evt);
			}

			// TODO: LPS[lid]->current_base_pointer can be removed as a parameter
			silent_execution(lid, LPS[lid]->current_base_pointer, list_next(time_barrier_pointer[i]->last_event), realignment_evt);
		}
*/

		// Call the application to check termination
//		printf("[%d] inside OnGVT CBP: %p\n",lid,LPS[lid]->current_base_pointer);
		lps_termination[lid] = OnGVT[lid](LidToGid(lid), LPS[lid]->current_base_pointer);
		check_res &= lps_termination[lid];

		// Early stop
		if(rootsim_config.check_termination_mode == INCR_CKTRM && !check_res) {
			break;
		}

		// Restore the current state
		current_lvt = LPS[lid]->bound->timestamp;
		LPS[lid]->state = temporary_log.state;
		LPS[lid]->current_base_pointer = temporary_log.base_pointer;
		log_restore(lid, &temporary_log);
		log_delete(temporary_log.log);
	}

	// No real LP is running now!
	current_lp = IDLE_PROCESS;
	current_lvt = -1.0;

}

