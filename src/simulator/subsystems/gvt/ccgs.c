/**
*			Copyright (C) 2008-2014 HPDCS Group
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

// TODO: questo qui va tolto, deve sopravvivere solo in communication.c
#include <mpi.h>

#include <stdbool.h>
#include <core/core.h>
#include <mm/dymelor.h>
#include <communication/communication.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>



/// This variable is an aggregate result for the distributed termination detection
static bool ccgs_completed_simulation = false;

/// In case termination detection is incremental, this array keeps track of LPs that think the simulation can be halted already
static bool lps_termination[MAX_LPs];




static void do_compute_snapshot(state_t * __restrict__ time_barrier_pointer[]) {
	bool check_res = true;
	register unsigned int i;
	register unsigned int lid;
	
	for(i = 0; i < n_prc_per_thread; i++) {
		lid = LPS_bound[i]->lid;
			
		// If termination detection is incremental, we skip the current LP
		if(rootsim_config.check_termination_mode == INCR_CKTRM && lps_termination[i]) {
			continue;
		}
		
		// If the termination detection is non-incremental, we check here all the LPs.
		// Otherwise, at least one LP didn't terminate on the previous iteration,
		// because the simulation did not terminate. Therefore, and'ing the result holds.
		lps_termination[lid] = OnGVT[lid](LidToGid(lid), time_barrier_pointer[i]->buffer_state);
		check_res &= lps_termination[lid];
		
		// Early stop
		if(rootsim_config.check_termination_mode == INCR_CKTRM && !check_res) {
			break;
		}
	}
	
	// If there is only one kernel, set the termination locally,
	// otherwise collect all the states, using MPI's Logical AND reduction.
	if(n_ker > 1) {
		comm_reduce(&ccgs_completed_simulation, &check_res, 1, MPI_BYTE, MPI_LAND, 0, MPI_COMM_WORLD);
	} else {
		ccgs_completed_simulation = check_res;
	}
}




inline bool ccgs_can_halt_simulation(void) {
	return ccgs_completed_simulation;
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
void ccgs_compute_snapshot(state_t * time_barrier_pointer[], simtime_t gvt) {

//	return;
	
	register unsigned int i;
	register unsigned int lid;

	state_t temporary_log;

	for(i = 0; i < n_prc_per_thread; i++) {

		lid = LPS_bound[i]->lid;
		
		// If there is no bound, then no event was execute by the LP. CCGS can't do anything here!
		// The same holds for time_barrier_pointer[i].
		// Additionally, if a LP has to rollback near the gvt value, we can enter GVT operations
		// before the actual rollback operation is executed. This might ask to take into account a
		// simulation state which is now inconsistent, and might crash the simulation if the
		// last_event pointer points to an annihilated message.
		// TODO: this last check is now very conservative. We should fine tune the check
		// to see if we're really close to the about-to-rollback time.
		if(LPS[lid]->bound == NULL || time_barrier_pointer[i] == NULL || LPS[lid]->state == LP_STATE_ROLLBACK) {
			return;
		}

		// Log the current state so that after we can restore it.
		current_lp = lid;
		current_lvt = LPS[lid]->bound->timestamp;
		temporary_log.log = log_state(lid);
		
		// Restore the time barrier state
		current_lvt = time_barrier_pointer[i]->lvt;
		log_restore(lid, time_barrier_pointer[i]);
			
		// If the LP is not blocked, we can reconstruct the state exactly to the GVT
		if(!is_blocked_state(LPS[lid]->state))  {
			// Realign the state to the current GVT value
			if(time_barrier_pointer[i]->last_event != NULL) {
				int reproc;
				reproc = silent_execution(lid, time_barrier_pointer[i]->buffer_state, list_next(time_barrier_pointer[i]->last_event), gvt, NULL);
			}
		}
			
		// Restore the current state
		current_lvt = LPS[lid]->bound->timestamp;
		log_restore(lid, &temporary_log);
		log_delete(temporary_log.log);
	}
	
	// No real LP is running now!
	current_lp = IDLE_PROCESS;
	current_lvt = -1.0;

	// Call OnGVT callback, and perform application-defined check termination
	do_compute_snapshot(time_barrier_pointer);
}

