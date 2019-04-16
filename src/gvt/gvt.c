/**
* @file gvt/gvt.c
*
* @brief Global Virtual Time
*
* This module implements the GVT reduction. The current implementation
* is non blocking for observable simulation plaftorms.
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
* @author Tommaso Tocci
*
* @date June 14, 2014
*/

#include <ROOT-Sim.h>
#include <arch/thread.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>
#include <mm/mm.h>
#include <communication/mpi.h>
#include <communication/gvt.h>

enum kernel_phases {
	kphase_start,
#ifdef HAVE_MPI
	kphase_white_msg_redux,
#endif
	kphase_kvt,
#ifdef HAVE_MPI
	kphase_gvt_redux,
#endif
	kphase_fossil,
	kphase_idle
};

enum thread_phases {
	tphase_A,
	tphase_send,
	tphase_B,
	tphase_aware,
	tphase_idle
};

// Timer to know when we have to start GVT computation.
// Each thread could start the GVT reduction phase, so this
// is a per-thread variable.
timer gvt_timer;

timer gvt_round_timer;

#ifdef HAVE_MPI
static unsigned int init_kvt_tkn;
static unsigned int commit_gvt_tkn;
#endif

/* Data shared across threads */

static volatile enum kernel_phases kernel_phase = kphase_idle;

static unsigned int init_completed_tkn;
static unsigned int commit_kvt_tkn;
static unsigned int idle_tkn;

static atomic_t counter_initialized;
static atomic_t counter_kvt;
static atomic_t counter_finalized;

/// To be used with CAS to determine who is starting the next GVT reduction phase
static volatile unsigned int current_GVT_round = 0;

/// How many threads have left phase A?
static atomic_t counter_A;

/// How many threads have left phase send?
static atomic_t counter_send;

/// How many threads have left phase B?
static atomic_t counter_B;

/** Keep track of the last computed gvt value. Its a per-thread variable
 * to avoid synchronization on it, but eventually all threads write here
 * the same exact value.
 * The 'adopted_last_gvt' version is used to maintain the adopted gvt
 * value in a temporary variable. It is then copied to last_gvt during
 * the end phase, to avoid possible critical races when checking the
 * termination upon reaching a certain simulation time value.
 */
static __thread simtime_t last_gvt = 0.0;

// last agreed KVT
static volatile simtime_t new_gvt = 0.0;

/// What is my phase? All threads start in the initial phase
static __thread enum thread_phases thread_phase = tphase_idle;

/// Per-thread GVT round counter
static __thread unsigned int my_GVT_round = 0;

/// The local (per-thread) minimum. It's not TLS, rather an array, to allow reduction by master thread
static simtime_t *local_min;

static simtime_t *local_min_barrier;

/**
* Initialization of the GVT subsystem.
*/
void gvt_init(void)
{
	unsigned int i;

	// This allows the first GVT phase to start
	atomic_set(&counter_finalized, 0);

	// Initialize the local minima
	local_min = rsalloc(sizeof(simtime_t) * n_cores);
	local_min_barrier = rsalloc(sizeof(simtime_t) * n_cores);
	for (i = 0; i < n_cores; i++) {
		local_min[i] = INFTY;
		local_min_barrier[i] = INFTY;
	}

	timer_start(gvt_timer);

	// Initialize the CCGS subsystem
	ccgs_init();
}

/**
* Finalizer of the GVT subsystem.
*/
void gvt_fini(void)
{
	// Finalize the CCGS subsystem
	ccgs_fini();

#ifdef HAVE_MPI
	if ((kernel_phase == kphase_idle && !master_thread() && gvt_init_pending()) || kernel_phase == kphase_start) {
		join_white_msg_redux();
		wait_white_msg_redux();
		join_gvt_redux(-1.0);
	} else if (kernel_phase == kphase_white_msg_redux || kernel_phase == kphase_kvt) {
		wait_white_msg_redux();
		join_gvt_redux(-1.0);
	}
#endif
}

/**
 * This function returns the last computed GVT value at each thread.
 * It can be safely used concurrently to keep track of the evolution of
 * the committed trajectory. It's so far mainly used for termination
 * detection based on passed simulation time.
 */
inline simtime_t get_last_gvt(void)
{
	return last_gvt;
}

static inline void reduce_local_gvt(void)
{
	foreach_bound_lp(lp) {
		// If no message has been processed, local estimate for
		// GVT is forced to 0.0. This can happen, e.g., if
		// GVT is computed very early in the run
		if (unlikely(lp->bound == NULL)) {
			local_min[local_tid] = 0.0;
			break;
		}

		// GVT inheritance: if the current LP has no scheduled
		// events, we can safely assume that it should not
		// participate to the computation of the GVT, because any
		// event to it will appear *after* the GVT
		if (lp->bound->next == NULL)
			continue;

		local_min[local_tid] =
		    min(local_min[local_tid], lp->bound->timestamp);
	}
}

simtime_t GVT_phases(void)
{
	unsigned int i;

	if (thread_phase == tphase_A) {
#ifdef HAVE_MPI
		// Check whether we have new ingoing messages sent by remote instances
		receive_remote_msgs();
#endif
		process_bottom_halves();

		reduce_local_gvt();

		thread_phase = tphase_send;	// Entering phase send
		atomic_dec(&counter_A);	// Notify finalization of phase A
		return -1.0;
	}

	if (thread_phase == tphase_send && atomic_read(&counter_A) == 0) {
#ifdef HAVE_MPI
		// Check whether we have new ingoing messages sent by remote instances
		receive_remote_msgs();
#endif
		process_bottom_halves();
		schedule();
		thread_phase = tphase_B;
		atomic_dec(&counter_send);
		return -1.0;
	}

	if (thread_phase == tphase_B && atomic_read(&counter_send) == 0) {
#ifdef HAVE_MPI
		// Check whether we have new ingoing messages sent by remote instances
		receive_remote_msgs();
#endif
		process_bottom_halves();

		reduce_local_gvt();

#ifdef HAVE_MPI
		// WARNING: local thread cannot send any remote
		// message between the two following calls
		exit_red_phase();
		local_min[local_tid] =
		    min(local_min[local_tid], min_outgoing_red_msg[local_tid]);
#endif

		thread_phase = tphase_aware;
		atomic_dec(&counter_B);

		if (atomic_read(&counter_B) == 0) {
			simtime_t agreed_vt = INFTY;
			for (i = 0; i < n_cores; i++) {
				agreed_vt = min(local_min[i], agreed_vt);
			}
			return agreed_vt;
		}
		return -1.0;
	}

	return -1.0;
}

bool start_new_gvt(void)
{
#ifdef HAVE_MPI
	if (!master_kernel()) {
		//Check if we received a new GVT init msg
		return gvt_init_pending();
	}
#endif

	// Has enough time passed since the last GVT reduction?
	return timer_value_milli(gvt_timer) >
	    (int)rootsim_config.gvt_time_period;
}

/**
* This is the entry point from the main simulation loop to the GVT subsystem.
* This function is not executed in case of a serial simulation, and is executed
* concurrently by different worker threads in case of a parallel one.
* All the operations here implemented must be re-entrant.
* Any state variable of the GVT implementation must be declared statically and globally
* (in case, on a per-thread basis).
* This function is called at every simulation loop, so at the beginning the code should
* check whether a GVT computation is occurring, or if a computation must be started.
*
* @return The newly computed GVT value, or -1.0. Only a Master Thread should return a value
* 	  different from -1.0, to avoid generating too much information. If every thread
* 	  will return a value different from -1.0, nothing will be broken, but all the values
* 	  will be shown associated with the same kernel id (no way to distinguish between
* 	  different threads here).
*/
simtime_t gvt_operations(void)
{

	// GVT reduction initialization.
	// This is different from the paper's pseudocode to reduce
	// slightly the number of clock reads
	if (kernel_phase == kphase_idle) {

		if (start_new_gvt() &&
		    iCAS(&current_GVT_round, my_GVT_round, my_GVT_round + 1)) {

			timer_start(gvt_round_timer);

#ifdef HAVE_MPI
			//inform all the other kernels about the new gvt
			if (master_kernel()) {
				broadcast_gvt_init(current_GVT_round);
			} else {
				gvt_init_clear();
			}
#endif

			// Reduce the current CCGS termination detection
			ccgs_reduce_termination();

			/* kernel GVT round setup */

#ifdef HAVE_MPI
			flush_white_msg_recv();

			init_kvt_tkn = 1;
			commit_gvt_tkn = 1;
#endif

			init_completed_tkn = 1;
			commit_kvt_tkn = 1;
			idle_tkn = 1;

			atomic_set(&counter_initialized, n_cores);
			atomic_set(&counter_kvt, n_cores);
			atomic_set(&counter_finalized, n_cores);

			atomic_set(&counter_A, n_cores);
			atomic_set(&counter_send, n_cores);
			atomic_set(&counter_B, n_cores);

			kernel_phase = kphase_start;

			timer_restart(gvt_timer);
		}
	}

	/* Thread setup phase:
	 * each thread needs to setup its own local context
	 * before to partecipate to the new GVT round */
	if (kernel_phase == kphase_start && thread_phase == tphase_idle) {

		// Someone has modified the GVT round (possibly me).
		// Keep track of this update
		my_GVT_round = current_GVT_round;

#ifdef HAVE_MPI
		enter_red_phase();
#endif

		local_min[local_tid] = INFTY;

		thread_phase = tphase_A;
		atomic_dec(&counter_initialized);
		if (atomic_read(&counter_initialized) == 0) {
			if (iCAS(&init_completed_tkn, 1, 0)) {
#ifdef HAVE_MPI
				join_white_msg_redux();
				kernel_phase = kphase_white_msg_redux;
#else
				kernel_phase = kphase_kvt;
#endif
			}
		}
		return -1.0;
	}

#ifdef HAVE_MPI
	if (kernel_phase == kphase_white_msg_redux
	    && white_msg_redux_completed() && all_white_msg_received()) {
		if (iCAS(&init_kvt_tkn, 1, 0)) {
			flush_white_msg_sent();
			kernel_phase = kphase_kvt;
		}
		return -1.0;
	}
#endif

	/* KVT phase:
	 * make all the threads agree on a common virtual time for this kernel */
	if (kernel_phase == kphase_kvt && thread_phase != tphase_aware) {
		simtime_t kvt = GVT_phases();
		if (D_DIFFER(kvt, -1.0)) {
			if (iCAS(&commit_kvt_tkn, 1, 0)) {

#ifdef HAVE_MPI
				join_gvt_redux(kvt);
				kernel_phase = kphase_gvt_redux;

#else
				new_gvt = kvt;
				kernel_phase = kphase_fossil;

#endif
			}
		}
		return -1.0;
	}

#ifdef HAVE_MPI
	if (kernel_phase == kphase_gvt_redux && gvt_redux_completed()) {
		if (iCAS(&commit_gvt_tkn, 1, 0)) {
			int gvt_round_time = timer_value_micro(gvt_round_timer);
			statistics_post_data(current, STAT_GVT_ROUND_TIME, gvt_round_time);

			new_gvt = last_reduced_gvt();
			kernel_phase = kphase_fossil;
		}
		return -1.0;
	}
#endif

	/* GVT adoption phase:
	 * the last agreed GVT needs to be adopted by every thread */
	if (kernel_phase == kphase_fossil && thread_phase == tphase_aware) {

		// Execute fossil collection and termination detection
		// Each thread stores the last computed value in last_gvt,
		// while the return value is the gvt only for the master
		// thread. To check for termination based on simulation time,
		// this variable must be explicitly inspected using
		// get_last_gvt()
		adopt_new_gvt(new_gvt);

		// Dump statistics
		statistics_on_gvt(new_gvt);

		last_gvt = new_gvt;

		thread_phase = tphase_idle;
		atomic_dec(&counter_finalized);

		if (atomic_read(&counter_finalized) == 0) {
			if (iCAS(&idle_tkn, 1, 0)) {
				kernel_phase = kphase_idle;
			}
		}
		return last_gvt;
	}

	return -1.0;
}
