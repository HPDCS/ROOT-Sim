/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file base.c
* @brief This module implements core functionalities for ROOT-Sim and declares
*        core global variables for the simulator
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/18/2011
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

#include <arch/thread.h>
#include <core/core.h>
#include <core/init.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>
#include <mm/dymelor.h>

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

/// Used to map a global id to a local id
unsigned int *to_lid;

/// Used to map a local id to a global id
unsigned int *to_gid;

/// This global variable holds the configuration for the current simulation
simulation_configuration rootsim_config;

// Function Pointers to access functions implemented at application level
bool (**OnGVT)(unsigned int me, void *snapshot);
void (**ProcessEvent)(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);

/// Flag to notify all workers that there was an error
static bool sim_error = false;

/// This flag tells whether we are exiting from the kernel of from userspace
bool exit_silently_from_kernel = false;

/// This flag is set when the initialization of the simulator is complete, with no errors
bool init_complete = false;



/**
 * This function is used to terminate with not much pain the simulation
 * if the user model inadvertently calls exit(). It displays a warning
 * message, and then tries to silently shutdown.
 * The software enters this function using the standard atexit() API.
 *
 * @author Alessandro Pellegrini
 */
void exit_from_simulation_model(void) {

	if(!init_complete)
		return;

	if(!exit_silently_from_kernel) {
		exit_silently_from_kernel = true;

		printf("Warning: exit() has been called from the model.\n"
		       "The simulation will now halt, but its unlikely what you really wanted...\n"
		       "You should use OnGVT() instead. See the manpages for an explanation.\n");

		simulation_shutdown(EXIT_FAILURE);
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
void base_init(void) {
	register unsigned int i;
	GID_t gid;

	if(rootsim_config.num_controllers > 0)
		barrier_init(&all_thread_barrier, rootsim_config.num_controllers);
	else
		barrier_init(&all_thread_barrier, n_cores);

	n_prc = 0;
	ProcessEvent = rsalloc(sizeof(void *) * n_prc_tot);
	OnGVT = rsalloc(sizeof(void *) * n_prc_tot);
	to_lid = (unsigned int *)rsalloc(sizeof(unsigned int) * n_prc_tot);
	to_gid = (unsigned int *)rsalloc(sizeof(unsigned int) * n_prc_tot);

	for (i = 0; i < n_prc_tot; i++) {

		if (rootsim_config.snapshot == FULL_SNAPSHOT) {
			OnGVT[i] = &OnGVT_light;
			ProcessEvent[i] = &ProcessEvent_light;
		} // TODO: add here an else for ISS

		set_gid(gid, i);
		if (GidToKernel(gid) == kid) { // If the i-th logical process is hosted by this kernel
			to_lid[i] = n_prc;
			to_gid[n_prc] = i;
			n_prc++;
		} else if (kernel[i] < n_ker) { // If not
			to_lid[i] = UINT_MAX;
		} else { // Sanity check
			rootsim_error(true, "Invalid mapping: there is no kernel %d!\n", kernel[i]);
		}
	}

	atexit(exit_from_simulation_model);
}




/**
* This function finalizes the core structures of ROOT-Sim, just before terminating a simulation
*
* @author Roberto Vitali
*
*/
// TODO: controllare cosa serve davvero qui
void base_fini(void){
	rsfree(kernel);
	rsfree(to_gid);
	rsfree(to_lid);
	rsfree(OnGVT);
	rsfree(ProcessEvent);
}





/**
* Creates a mapping between logical processes' local and global identifiers
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The logical process' local identifier
* @return The global identifier of the logical process locally identified by lid
*/
GID_t LidToGid(LID_t lid) {
	GID_t ret;
	set_gid(ret, to_gid[lid_to_int(lid)]);
	return ret;
}


/**
* Creates a mapping between logical processes' global and local identifiers
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param gid The logical process' global identifier
* @return The local identifier of the logical process globally identified by gid
*/
LID_t GidToLid(GID_t gid) {
	LID_t ret;
	set_lid(ret, to_lid[gid_to_int(gid)]);
	return ret;
}


/**
* Creates a mapping between logical processes and kernel instances
*
* @author Francesco Quaglia
*
* @param gid The logical process' global identifier
* @return The id of the kernel currently hosting the logical process
*/
unsigned int GidToKernel(GID_t gid) {
	// restituisce il kernel su cui si trova il processo identificato da gid
	return kernel[gid_to_int(gid)];
}




/**
* This function calls all the finalization functions exposed by subsystems and then
* exits.
*
* @author Alessandro Pellegrini
*
* @param code The exit code to be returned by the process
*/
void simulation_shutdown(int code) {

	exit_silently_from_kernel = true;

	statistics_stop(code);

	if(!rootsim_config.serial) {

		thread_barrier(&all_thread_barrier);

		if(master_thread()) {
			statistics_fini();
			dymelor_fini();
			scheduler_fini();
			gvt_fini();
			communication_fini();
			base_fini();
		}

		thread_barrier(&all_thread_barrier);
	}

	exit(code);
}



inline bool simulation_error(void) {
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
void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);\
	fflush(stderr);

	if(fatal) {
		if(rootsim_config.serial) {
			abort();
		} else {

			if(!init_complete) {
				exit(EXIT_FAILURE);
			}

			// Notify all KLT to shut down the simulation
			sim_error = true;
		}
	}
}





/**
* This function maps logical processes onto kernel instances
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/
void distribute_lps_on_kernels(void) {
	register unsigned int i = 0;
	unsigned int j;
	unsigned int buf1;
	int offset;
	int block_leftover;

	// Sanity check on number of LPs
	if(n_prc_tot < n_ker)
		rootsim_error(true, "Unable to allocate %d logical processes on %d kernels: must have at least %d LPs\n", n_prc_tot, n_ker, n_ker);

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
			}
			break;
	}
}


/**
 * This function records that the initialization is complete.
 */
void initialization_complete(void) {
	init_complete = true;
}
