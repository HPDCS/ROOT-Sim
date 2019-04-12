/**
* @file core/core.c
*
* @brief Core ROOT-Sim functionalities
*
* Core ROOT-Sim functionalities
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
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @date 3/18/2011
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <signal.h>

#include <arch/thread.h>
#include <core/core.h>
#include <core/init.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>
#include <mm/mm.h>

/// Barrier for all worker threads
barrier_t all_thread_barrier;

/// Mapping between kernel instances and logical processes
unsigned int *kernel;

/// Identifier of the local kernel
unsigned int kid;

/// Total number of simulation kernel instances running
unsigned int n_ker;

/// Total number of cores required for simulation
unsigned int n_cores;

/// Total number of logical processes running in the simulation
unsigned int n_prc_tot;

/// Number of logical processes hosted by the current kernel instance
unsigned int n_prc;

/// This global variable holds the configuration for the current simulation
simulation_configuration rootsim_config;

/// Flag to notify all workers that there was an error
static bool sim_error = false;

/// This flag tells whether we are exiting from the kernel of from userspace
bool exit_silently_from_kernel = false;

/// This flag is set when the initialization of the simulator is complete, with no errors
static bool init_complete = false;

bool user_exit_flag = false;

/**
 * This function is used to terminate with not much pain the simulation
 * if the user model inadvertently calls exit(). It displays a warning
 * message, and then tries to silently shutdown.
 * The software enters this function using the standard atexit() API.
 *
 * @author Alessandro Pellegrini
 */
void exit_from_simulation_model(void)
{

	if (likely(!init_complete))
		return;

	if (unlikely(!exit_silently_from_kernel)) {
		exit_silently_from_kernel = true;

		printf("Warning: exit() has been called from the model.\n"
		       "The simulation will now halt, but its unlikely what you really wanted...\n"
		       "You should use OnGVT() instead. See the manpages for an explanation.\n");

		simulation_shutdown(EXIT_FAILURE);
	}
}

inline bool user_requested_exit(void)
{
	return user_exit_flag;
}

static void handle_signal(int signum)
{
	if (signum == SIGINT) {
		user_exit_flag = true;
	}
}

/**
* This function initilizes basic functionalities within ROOT-Sim. In particular, it
* creates a mapping between logical processes and kernel instances.
*
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
*/
void base_init(void)
{
	struct sigaction new_act = { 0 };

	barrier_init(&all_thread_barrier, n_cores);

	// complete the sigaction struct init
	new_act.sa_handler = handle_signal;
	// we set the signal action so that it auto disarms itself after the first invocation
	new_act.sa_flags = SA_RESETHAND;
	// register the signal handler
	sigaction(SIGINT, &new_act, NULL);
	// register the exit function
	atexit(exit_from_simulation_model);
}

/**
* This function finalizes the core structures of ROOT-Sim, just before terminating a simulation
*
* @author Roberto Vitali
*
*/
void base_fini(void)
{
}

/**
* Creates a mapping between logical processes and kernel instances
*
* @author Francesco Quaglia
*
* @param gid The logical process' global identifier
* @return The id of the kernel currently hosting the logical process
*/
__attribute__((pure))
unsigned int find_kernel_by_gid(GID_t gid)
{
	// restituisce il kernel su cui si trova il processo identificato da gid
	return kernel[gid.to_int];
}

/**
* This function calls all the finalization functions exposed by subsystems and then
* exits.
*
* @author Alessandro Pellegrini
*
* @param code The exit code to be returned by the process
*/
void simulation_shutdown(int code)
{

	exit_silently_from_kernel = true;

	statistics_stop(code);

	if (likely(rootsim_config.serial == false)) {

		thread_barrier(&all_thread_barrier);

		if (master_thread()) {
			statistics_fini();
			gvt_fini();
			communication_fini();
			scheduler_fini();
			base_fini();
		}

		thread_barrier(&all_thread_barrier);
	}

	exit(code);
}

inline bool simulation_error(void)
{
	return sim_error;
}

/**
* A variadic function which prints out error messages. If the errors are marked as fatal,
* the simulation is correctly shut down.
*
* @author Alessandro Pellegrini
*
* @param fatal This flag marks an error as fatal (true) or not (false)
* @param msg The error message to be printed out. This can be specified as in the printf()
*        format message, thus a matching number of extra parameters can be passed.
*
* @todo If a fatal error is received, write it on the log file as well!
*/
void _rootsim_error(bool fatal, const char *msg, ...)
{
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);
	fflush(stderr);

	if (fatal) {
		if (rootsim_config.serial) {
			exit(EXIT_FAILURE);
		} else {

			if (!init_complete) {
				exit(EXIT_FAILURE);
			}

			// Notify all KLT to shut down the simulation
			sim_error = true;

			// Bye bye main loop!
			longjmp(exit_jmp, 1);
		}
	}
}

/**
* This function maps logical processes onto kernel instances
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/
void distribute_lps_on_kernels(void)
{
	register unsigned int i = 0;
	unsigned int j;
	unsigned int buf1;
	int offset;
	int block_leftover;

	// Sanity check on number of LPs
	if (n_prc_tot < n_ker) {
		rootsim_error(true, "Unable to allocate %d logical processes on %d kernels: must have at least %d LPs\n", n_prc_tot, n_ker, n_ker);
	}

	kernel = (unsigned int *)rsalloc(sizeof(unsigned int) * n_prc_tot);

	switch (rootsim_config.lps_distribution) {

	case LP_DISTRIBUTION_BLOCK:
		buf1 = (n_prc_tot / n_ker);
		block_leftover = n_prc_tot - buf1 * n_ker;

		// It is a hack to bypass the first check that set offset to 0
		if (block_leftover > 0)
			buf1++;

		offset = 0;
		while (i < n_prc_tot) {
			j = 0;
			while (j < buf1) {
				kernel[i] = offset;

				if (kernel[i] == kid)
					n_prc++;

				i++;
				j++;
			}
			offset++;
			block_leftover--;
			if (block_leftover == 0)
				buf1--;
		}
		break;

	case LP_DISTRIBUTION_CIRCULAR:
		for (i = 0; i < n_prc_tot; i++) {
			kernel[i] = i % n_ker;

			if (kernel[i] == kid)
				n_prc++;
		}
		break;
	}
}

/**
 * This function records that the initialization is complete.
 */
void initialization_complete(void)
{
	init_complete = true;
}
