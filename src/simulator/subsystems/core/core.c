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

#include <arch/thread.h>
#include <core/core.h>
#include <scheduler/process.h>
#include <statistics/statistics.h>
#include <mm/malloc.h>
#include <gvt/gvt.h>
#include <mm/dymelor.h>


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

/// Number of logical processes handled by a kernel instance
unsigned int n_prc_per_kernel[N_KER_MAX];

/// Used to map global ids to local ids in one kernel instance's scope
unsigned int *kernel_lid_to_gid[N_KER_MAX];

/// This global variable holds the configuration for the current simulation
simulation_configuration rootsim_config;

// Function Pointers to access functions implemented at application level
bool (**OnGVT)(int gid, void *snapshot);
void (**ProcessEvent)(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);

/// Flag to notify all workers that there was an error
static bool sim_error = false;

/// This variable is used by rootsim_error to know whether fatal errors involve stopping MPI or not
bool mpi_is_initialized = false;

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

	n_prc = 0;
	ProcessEvent = rsalloc(sizeof(void *) * n_prc_tot);
	OnGVT = rsalloc(sizeof(void *) * n_prc_tot);
	to_lid = (unsigned int *)rsalloc(sizeof(unsigned int) * n_prc_tot);
	to_gid = (unsigned int *)rsalloc(sizeof(unsigned int) * n_prc_tot);

	for(i = 0; i < N_KER_MAX; i++)
		kernel_lid_to_gid[i] = (unsigned int *)rsalloc(sizeof(unsigned int) * n_prc_tot);

	for (i = 0; i < n_prc_tot; i++) {

		if (rootsim_config.snapshot == FULL_SNAPSHOT) {
			OnGVT[i] = &OnGVT_light;
			ProcessEvent[i] = &ProcessEvent_light;
		}

		if (GidToKernel(i) == kid) { // If the i-th logical process is hosted by this kernel
			to_lid[i] = n_prc;
			to_gid[n_prc] = i;
			n_prc++;
		} else if (kernel[i] < n_ker) { // If not
			to_lid[i] = -1;
		} else { // Sanity check
			rootsim_error(true, "Invalid mapping: there is no kernel %d!\n", kernel[i]);
		}

		// TODO: questo modo di assegnare le risorse è un po' malato... n_prc_per_kernel di fatto serve solo qui!!!!
		kernel_lid_to_gid[kernel[i]][n_prc_per_kernel[kernel[i]]] = i;
		n_prc_per_kernel[kernel[i]]++;
	}
	
	
	for (i = n_prc; i < n_prc_tot; i++) {
		to_gid[i] = -1;
	}
}




/**
* This function finalizes the core structures of ROOT-Sim, just before terminating a simulation
*
* @author Roberto Vitali
*
*/
// TODO: controllare cosa serve davvero qui
void base_fini(void){
	unsigned int i;
	rsfree(kernel);

	for(i = 0; i < N_KER_MAX; i++) {
		rsfree(kernel_lid_to_gid[i]);
	}
	rsfree(to_gid);
	rsfree(to_lid);
	rsfree(OnGVT);
	rsfree(ProcessEvent);
}





/**
* Creates a mapping between logical processes' local and global identifiers
*
* @author Francesco Quaglia
*
* @param lid The logical process' local identifier
* @return The global identifier of the logical process locally identified by lid
*/
unsigned int LidToGid(unsigned int lid) {
	return to_gid[lid];
}


/**
* Creates a mapping between logical processes' global and local identifiers
*
* @author Francesco Quaglia
*
* @param gid The logical process' global identifier
* @return The local identifier of the logical process globally identified by lid,
*         -1 if the process is not hosted by the current kernel.
*/
unsigned int GidToLid(unsigned int gid) {
	return to_lid[gid];
}




/**
* Creates a mapping between logical processes and kernel instances
*
* @author Francesco Quaglia
*
* @param gid The logical process' global identifier
* @return The id of the kernel currently hosting the logical process
*/
unsigned int GidToKernel(unsigned int gid) {
	// restituisce il kernel su cui si trova il processo identificato da gid	
	return kernel[gid];
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
	
	if(mpi_is_initialized) {
		comm_finalize();
		
		// TODO: qui è necessario notificare agli altri kernel che c'è stato un errore ed è necessario fare lo shutdown
		if(master_kernel()) {
		}
	}
	
	if(!rootsim_config.serial) {

		// All kernels must exit at the same time
		if(n_ker > 1) {
//			comm_barrier(MPI_COMM_WORLD);
		}
		
		if(master_thread()) {
			dymelor_fini();
			scheduler_fini();
			gvt_fini();
			communication_fini();
			base_fini();
		}
		
		// TODO: qui ci vuole una barriera tra tutti i thread
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

	fprintf(stderr, (fatal ? "[FATAL ERROR (%d)] " : "[WARNING (%d)] "), kid);

	fprintf(stderr, "%s", buf);\
	fflush(stderr);

	if(fatal) {
		if(rootsim_config.serial) {
			abort();
		} else {
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

