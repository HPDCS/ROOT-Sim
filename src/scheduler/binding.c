/**
 * @file scheduler/binding.c
 *
 * @brief Load sharing rules across worker threads
 *
 * Implements load sharing rules for LPs among worker threads
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
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <arch/atomic.h>
#include <arch/thread.h>
#include <core/core.h>
#include <core/init.h>
#include <datatypes/list.h>
#include <powercap/powercap.h>
#include <scheduler/binding.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>

#include <arch/thread.h>

#define REBIND_INTERVAL 100

struct lp_cost_id {
	double workload_factor;
	unsigned int id;
};

struct lp_cost_id *lp_cost;

static unsigned int *new_LPS_binding;

static volatile int binding_acquire_phase = 0;
static __thread int local_binding_acquire_phase = 0;

static volatile int binding_phase = 0;
static __thread int local_binding_phase = 0;

static atomic_t worker_thread_reduction;
static atomic_t worker_thread_installation;

static bool rebinding_triggered = false;
static bool rebinding_completed = true;

/**
* Performs a (deterministic) block allocation between LPs and WTs
*
* @author Alessandro Pellegrini
*/
static inline void LPs_block_binding(void)
{
	unsigned int i, j;
	unsigned int buf1;
	unsigned int offset;
	unsigned int block_leftover;
	struct lp_struct *lp;

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
			if (offset == local_tid) {
				lp = lps_blocks[i];
				LPS_bound_set(n_prc_per_thread++, lp);
				lp->worker_thread = local_tid;
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
* Convenience function to compare two elements of struct lp_cost_id.
* This is used for sorting the LP vector in LP_knapsack()
*
* @author Alessandro Pellegrini
*
* @param a Pointer to the first element
* @param b Pointer to the second element
*
* @return The comparison between a and b
*/
static int compare_lp_cost(const void *a, const void *b)
{
	struct lp_cost_id *A = (struct lp_cost_id *)a;
	struct lp_cost_id *B = (struct lp_cost_id *)b;

	return (B->workload_factor - A->workload_factor);
}

/**
* Implements the knapsack load sharing policy in:
*
* Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia
* A Load Sharing Architecture for Optimistic Simulations on Multi-Core Machines
* In Proceedings of the 19th International Conference on High Performance Computing (HiPC)
* Pune, India, IEEE Computer Society, December 2012.
*
*
* @author Alessandro Pellegrini
*/
static inline void LP_knapsack(void)
{
	register unsigned int i, j;
	double reference_knapsack = 0;
	bool assigned;
	double assignments[active_threads];

	if (!master_thread())
		return;

	// Estimate the reference knapsack
	for (i = 0; i < n_prc; i++) {
		reference_knapsack += lp_cost[i].workload_factor;
	}
	reference_knapsack /= active_threads;

	// Sort the expected times
	qsort(lp_cost, n_prc, sizeof(struct lp_cost_id), compare_lp_cost);

	// At least one LP per thread
	bzero(assignments, sizeof(double) * n_cores);
	j = 0;
	for (i = 0; i < active_threads; i++) {
		assignments[j] += lp_cost[i].workload_factor;
		new_LPS_binding[i] = j;
		j++;
	}

	// Suboptimal approximation of knapsack
	for (; i < n_prc; i++) {
		assigned = false;

		for (j = 0; j < active_threads; j++) {
			// Simulate assignment
			if (assignments[j] + lp_cost[i].workload_factor <= reference_knapsack) {
				assignments[j] += lp_cost[i].workload_factor;
				new_LPS_binding[i] = j;
				assigned = true;
				break;
			}
		}

		if (assigned == false)
			break;
	}

	// Check for leftovers
	if (i < n_prc) {
		j = 0;
		for (; i < n_prc; i++) {
			new_LPS_binding[i] = j;
			j = (j + 1) % active_threads;
		}
	}
}

static void post_local_reduction(void)
{
	unsigned int i = 0;
	msg_t *first_evt, *last_evt;

	foreach_bound_lp(lp) {
		first_evt = list_head(lp->queue_in);
		last_evt = list_tail(lp->queue_in);

		lp_cost[lp->lid.to_int].id = i++; // TODO: do we really need this?
		lp_cost[lp->lid.to_int].workload_factor = list_sizeof(lp->queue_in);
		lp_cost[lp->lid.to_int].workload_factor *= statistics_get_lp_data(lp, STAT_GET_EVENT_TIME_LP);
		lp_cost[lp->lid.to_int].workload_factor /= (last_evt->timestamp - first_evt->timestamp);
	}
}

static void install_binding(void)
{
	unsigned int i = 0;

	n_prc_per_thread = 0;

	//~ if(master_thread()) {
		//~ foreach_lp(lp) {
			//~ printf("Lp %d installed on thread %d\n", lp->lid.to_int, new_LPS_binding[i]);
		//~ }
	//~ }

	foreach_lp(lp) {
		if (new_LPS_binding[i++] == local_tid) {
			LPS_bound_set(n_prc_per_thread++, lp);

			if (local_tid != lp->worker_thread) {
				lp->worker_thread = local_tid;
			}
		}
	}

	//~ for(i = 0; i < n_prc; i++)
		//~ if(lps_bound_blocks[i])
			//~ printf("%d: LP %d is %sbound\n", tid, lps_bound_blocks[i]->lid.to_int, i < n_prc_per_thread ? "" : "not ");
}


/**
* This function is used to create a temporary binding between LPs and KLT.
* The first time this function is called, each worker thread sets up its data
* structures, and the performs a (deterministic) block allocation. This is
* because no runtime data is available at the time, so we "share" the load
* as the number of LPs.
* Then, successive invocations, will use the knapsack load sharing policy

* @author Alessandro Pellegrini
*/
void rebind_LPs(void)
{
	double committed_per_second;

	if(rootsim_config.powercap > 0)
		sample_average_powercap_violation();
	
	if (master_thread()) {
		if (unlikely(rebinding_triggered && rebinding_completed)) {
			rebinding_triggered = false;
			rebinding_completed = false;

			// The old number of active threads must participate in the reduction
			atomic_set(&worker_thread_reduction, active_threads);

			if(rootsim_config.powercap > 0) {
				// Retrieve the current throughput value and decide
				// upon a new configuration of threads.
				committed_per_second = statistics_get_current_throughput();
				active_threads = start_heuristic(committed_per_second); // threads could go to sleep here
				printf("Active threads set to %d\n", active_threads);
			}

			binding_phase++;
		}

		if (atomic_read(&worker_thread_reduction) == 0) {
			LP_knapsack();
	
			// Possibly wake up some threads
			if(rootsim_config.powercap > 0)
				wake_up_sleeping_threads();

			atomic_set(&worker_thread_installation, active_threads);
			binding_acquire_phase++;
		}
	}

	if (local_binding_phase < binding_phase) {
		local_binding_phase = binding_phase;
		post_local_reduction();
		atomic_dec(&worker_thread_reduction);

		// Go to sleep if the heuristic decided to do so
		if(rootsim_config.powercap > 0)
			check_running_array(tid);
	}

	if (local_binding_acquire_phase < binding_acquire_phase) {
		local_binding_acquire_phase = binding_acquire_phase;
		install_binding();

#ifdef HAVE_PREEMPTION
		reset_min_in_transit(local_tid);
#endif
		atomic_dec(&worker_thread_installation);
		while(atomic_read(&worker_thread_installation) > 0);
		if(master_thread()) {
			atomic_set(&worker_thread_reduction, active_threads);
			rebinding_completed = true;
		}

		//~ if (thread_barrier(&all_thread_barrier)) {
                       //~ atomic_set(&worker_thread_reduction, active_threads);
                       //~ rebinding_completed = true;
                //~ }
	}
}

/**
* Other subsystems might request to rebind LPs and threads by relying on
* this internal API. It allows to specify the number of threads that should
* participate in the rebinding phase.
* 
* @author Alessandro Pellegrini
*/
void trigger_rebinding(void)
{
	if(!rebinding_triggered) {
		rebinding_triggered = true;
	}
}


/**
* This function _must_ be called before entering the actual main loop.
* Each worker thread sets up its data structures, and the performs a
* (deterministic) block allocation. This is because no runtime data
* is available at the time, so we "share" the load as the number of LPs.
* Then, successive invocations, will use the knapsack load sharing policy

* @author Alessandro Pellegrini
*/
void initial_binding(void)
{
	active_threads = n_cores;
	initialize_binding_blocks();

	LPs_block_binding();

	if (master_thread()) {
		new_LPS_binding = rsalloc(sizeof(int) * n_prc);

		lp_cost = rsalloc(sizeof(struct lp_cost_id) * n_prc);
		memset(lp_cost, 0, sizeof(struct lp_cost_id) * n_prc);

		atomic_set(&worker_thread_reduction, active_threads);
	}
}
