/**
* @file gvt/fossil.c
*
* @brief Housekeeping operations
*
* In this module all the housekeeping operations related to GVT computation phase
* are present.
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
*/

#include <arch/thread.h>
#include <core/init.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <mm/state.h>
#include <mm/mm.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>

/// Counter for the invocations of adopt_new_gvt. This is used to determine whether a consistent state must be reconstructed
static unsigned long long snapshot_cycles;

/**
* Determine which snapshots in the state queue can be free'd because are placed before the current time barrier.
*
* Queues are cleaned by deleting all the events the timestamp of which is STRICTLY lower than the time barrier.
* Since state_pointer points to an event in queue_in, the state queue must be cleaned after the input queue.
*
* @param lp A pointer to the lp_struct for which we want to recollect memory
* @param time_barrier The current barrier
*/
void fossil_collection(struct lp_struct *lp, simtime_t time_barrier)
{
	state_t *state;
	msg_t *last_kept_event;
	double committed_events;

	// State list must be handled specifically, as nodes point to malloc'd
	// nodes. We therefore manually scan the list and free the memory.
	while ((state = list_head(lp->queue_states)) != NULL
	       && state->lvt < time_barrier) {
		log_delete(state->log);
		if(&topology_settings && topology_settings.write_enabled)
			rsfree(state->topology);
		if(&abm_settings)
			rsfree(state->region_data);
#ifndef NDEBUG
		state->last_event = (void *)0xDEADBABE;
#endif
		list_pop(lp->queue_states);
	}

	// Determine queue pruning horizon
	state = list_head(lp->queue_states);
	last_kept_event = state->last_event;

	// Truncate the input queue, accounting for the event which is pointed by the lastly kept state
	committed_events =
	    (double)list_trunc(lp->queue_in, timestamp,
			       last_kept_event->timestamp, msg_release);
	statistics_post_data(lp, STAT_COMMITTED, committed_events);

	// Truncate the output queue
	list_trunc(lp->queue_out, send_time, last_kept_event->timestamp,
		   msg_hdr_release);
}

/**
* This function is used by Master and Slave Kernels to determine the time barrier
* and perform some housekeeping once the new GVT value has been computed.
*
* @param new_gvt This is a new GVT value which has been computed and can be used
*        to perform fossil collection and to activate CCGS
*/
void adopt_new_gvt(simtime_t new_gvt)
{
	unsigned int i;

	state_t *time_barrier_pointer[n_prc_per_thread];
	bool compute_snapshot;

	// Snapshot should be recomputed only periodically
	snapshot_cycles++;
	compute_snapshot =
	    ((snapshot_cycles % rootsim_config.gvt_snapshot_cycles) == 0);

	// Precompute the time barrier for each process
	i = 0;
	foreach_bound_lp(lp) {
		time_barrier_pointer[i++] = find_time_barrier(lp, new_gvt);
	}

	// If needed, call the CCGS subsystem
	if (compute_snapshot)
		ccgs_compute_snapshot(time_barrier_pointer, new_gvt);

	i = 0;
	foreach_bound_lp(lp) {
		if (time_barrier_pointer[i] == NULL)
			continue;

		// Execute the fossil collection
		fossil_collection(lp, time_barrier_pointer[i]->lvt);

		// Actually release memory buffer allocated by the LPs and then released via free() calls
		clean_buffers_on_gvt(lp, time_barrier_pointer[i]->lvt);

		i++;
	}
}
