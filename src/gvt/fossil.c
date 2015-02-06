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
* @file fossil.c
* @brief In this module all the housekeeping operations related to GVT computation phase
* 	 are present.
* @author Alessandro Pellegrini
*/



#include <arch/thread.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <mm/state.h>
#include <scheduler/process.h>
#include <statistics/statistics.h>


/// Counter for the invocations of adopt_new_gvt. This is used to determine whether a consistent state must be reconstructed
static unsigned long long snapshot_cycles;


/**
* Determine which snapshots in the state queue can be free'd because are placed before the current time barrier.
*
* Queues are cleaned by deleting all the events the timestamp of which is STRICTLY lower than the time barrier.
* Since state_pointer points to an event in queue_in, the state queue must be cleaned after the input queue.
*
* @author Francesco Quaglia
*
* @param lid The logical process' local identifier
* @param time_barrier The current barrier
*/
void fossil_collection(unsigned int lid, simtime_t time_barrier) {
	state_t *state;
	msg_t *last_kept_event;
	double committed_events;
	
	time_barrier = 0.7 * time_barrier;

	// State list must be handled differently, as nodes point to malloc'd
	// nodes. We therefore manually scan the list and free the memory.
	while( (state = list_head(LPS[lid]->queue_states)) != NULL && state->lvt < time_barrier) {
		log_delete(list_head(LPS[lid]->queue_states)->log);
		state->last_event = (void *)0xDEADBABE;
		list_pop(LPS[lid]->queue_states);
	}

	// Determine queue pruning horizon
	last_kept_event = list_head(LPS[lid]->queue_states)->last_event;

	// Truncate the input queue, accounting for the event which is pointed by the lastly kept state
	committed_events = (double)list_trunc_before(LPS[lid]->queue_in, timestamp, last_kept_event->timestamp);
	statistics_post_lp_data(lid, STAT_COMMITTED, committed_events);

	// Truncate the output queue
	list_trunc_before(LPS[lid]->queue_out, send_time, last_kept_event->timestamp);

}



/**
* This function is used by Master and Slave Kernels to determine the time barrier
* and perform some housekeeping once the new GVT value has been computed.
*
* @author Francesco Quaglia
*/
simtime_t adopt_new_gvt(simtime_t new_gvt) {

	register unsigned int i;

	state_t *time_barrier_pointer[n_prc_per_thread];
	simtime_t local_time_barrier = INFTY;
	simtime_t lp_time_barrier;
	bool compute_snapshot;

	// Snapshot should be recomputed only periodically
	snapshot_cycles++;
	compute_snapshot = ((snapshot_cycles % rootsim_config.gvt_snapshot_cycles) == 0);

	// Precompute the time barrier for each process
	for (i = 0; i < n_prc_per_thread; i++) {

		time_barrier_pointer[i] = find_time_barrier(LPS_bound[i]->lid, new_gvt);

		lp_time_barrier = time_barrier_pointer[i]->lvt;
		if (lp_time_barrier > -1) {
			local_time_barrier = min(local_time_barrier, lp_time_barrier);
		}
	}

	// If needed, call the CCGS subsystem
	if(compute_snapshot) {
		ccgs_compute_snapshot(time_barrier_pointer, new_gvt);
	}


	for(i = 0; i < n_prc_per_thread; i++) {

		// Execute the fossil collection
		fossil_collection(LPS_bound[i]->lid, time_barrier_pointer[i]->lvt);

		// Actually release memory buffer allocated by the LPs and then released via free() calls
		clean_buffers_on_gvt(LPS_bound[i]->lid, time_barrier_pointer[i]->lvt);
	}

	return local_time_barrier;
}

