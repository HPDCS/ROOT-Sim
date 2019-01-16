/**
*                       Copyright (C) 2008-2019 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file main.c
* @brief This module contains the entry point for the simulator, and some core functions
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/

#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>

#include <core/core.h>
#include <arch/thread.h>
#include <statistics/statistics.h>
#include <gvt/ccgs.h>
#include <scheduler/binding.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/mm.h>

#ifdef HAVE_CROSS_STATE
#include <mm/ecs.h>
#endif

#include <serial/serial.h>
#include <communication/mpi.h>

#define _INIT_FROM_MAIN
#include <core/init.h>
#undef _INIT_FROM_MAIN

/**
 * This jump buffer allows rootsim_error, in case of a failure, to jump
 * out of any point in the code to the final part of the loop in which
 * all threads synchronize. This avoids side effects like, e.g., accessing
 * a NULL pointer.
 */

jmp_buf exit_jmp;

/**
* This function checks the different possibilities for termination detection termination.
*/
static bool end_computing(void)
{

	// Did CCGS decide to terminate the simulation?
	if (ccgs_can_halt_simulation()) {
		return true;
	}
	// Termination detection based on passed (committed) simulation time
	if (rootsim_config.simulation_time != 0 && (int)get_last_gvt() >= rootsim_config.simulation_time) {
		return true;
	}
	// If some KLT has encountered an error condition, we neatly shut down the simulation
	if (simulation_error()) {
		return true;
	}

	if (user_requested_exit())
		return true;

	return false;
}

#ifdef HAVE_PREEMPTION
extern atomic_t preempt_count;
extern atomic_t overtick_platform;
extern atomic_t would_preempt;
#endif

/**
* This function implements the main simulation loop
*
* @param arg This argument should be always set to NULL
*/

static void *main_simulation_loop(void *arg) __attribute__((noreturn));
static void *main_simulation_loop(void *arg)
{

	(void)arg;

	simtime_t my_time_barrier = -1.0;

#ifdef HAVE_CROSS_STATE
	lp_alloc_thread_init();
#endif

	// Do the initial (local) LP binding, then execute INIT at all (local) LPs
	initialize_worker_thread();

#ifdef HAVE_MPI
	syncronize_all();
#endif

	// Notify the statistics subsystem that we are now starting the actual simulation
	if (master_kernel() && master_thread()) {
		statistics_start();
		printf("****************************\n"
		       "*    Simulation Started    *\n"
		       "****************************\n");
	}

	if (setjmp(exit_jmp) != 0) {
		goto leave_for_error;
	}

	while (!end_computing()) {
		// Recompute the LPs-thread binding
		rebind_LPs();

#ifdef HAVE_MPI
		// Check whether we have new ingoing messages sent by remote instances
		receive_remote_msgs();
		prune_outgoing_queues();
#endif
		// Forward the messages from the kernel incoming message queue to the destination LPs
		process_bottom_halves();

		// Activate one LP and process one event. Send messages produced during the events' execution
		schedule();

		my_time_barrier = gvt_operations();

		// Only a master thread on master kernel prints the time barrier
		if (master_kernel() && master_thread() && D_DIFFER(my_time_barrier, -1.0)) {
			if (rootsim_config.verbose == VERBOSE_INFO || rootsim_config.verbose == VERBOSE_DEBUG) {
#ifdef HAVE_PREEMPTION
				printf
				    ("TIME BARRIER %f - %d preemptions - %d in platform mode - %d would preempt\n",
				     my_time_barrier,
				     atomic_read(&preempt_count),
				     atomic_read(&overtick_platform),
				     atomic_read(&would_preempt));
#else
				printf("TIME BARRIER %f\n", my_time_barrier);

#endif

				fflush(stdout);
			}
		}
#ifdef HAVE_MPI
		collect_termination();
#endif
	}

 leave_for_error:
	thread_barrier(&all_thread_barrier);

	// If we're exiting due to an error, we neatly shut down the simulation
	if (simulation_error()) {
		simulation_shutdown(EXIT_FAILURE);
	}
	simulation_shutdown(EXIT_SUCCESS);
}

/**
* This function implements the ROOT-Sim's entry point
*
* @param argc Number of arguments passed to ROOT-Sim
* @param argv Arguments passed to ROOT-Sim
*
* @return Exit code
*/
int main(int argc, char **argv)
{
#ifdef HAVE_MPI
	volatile int __wait = 0;
	char hostname[256];

	if ((getenv("WGDB")) != NULL && *(getenv("WGDB")) == '1') {
		gethostname(hostname, sizeof(hostname));
		printf("PID %d on %s ready for attach\n", getpid(), hostname);
		fflush(stdout);

		while (__wait == 0)
			sleep(5);
	}
#endif

	SystemInit(argc, argv);

	if (rootsim_config.core_binding)
		set_affinity(0);

	if (rootsim_config.serial) {
		serial_simulation();
	} else {

		// The number of locally required threads is now set. Detach them and then join the main simulation loop
		if (!simulation_error()) {
			if (n_cores > 1) {
				create_threads(n_cores - 1, main_simulation_loop, NULL);
			}

			main_simulation_loop(NULL);
		}
	}

	return 0;
}
