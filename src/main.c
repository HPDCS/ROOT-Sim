/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file main.c
* @brief This module contains the entry point for the simulator, and some core functions
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/16/2011
*/


#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <core/core.h>
#include <arch/thread.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <mm/dymelor.h>
#include <mm/modules/ktblmgr/ktblmgr.h>
#include <serial/serial.h>


#define _INIT_FROM_MAIN
#include <core/init.h>
#undef _INIT_FROM_MAIN



#if defined(FINE_GRAIN_DEBUG) || defined(STEP_EXEC)
barrier_t debug_barrier;
#endif


/**
* This function checks the different possibilities for termination detection termination.
*/
static bool end_computing(void) {

	// Did CCGS decide to terminate the simulation?
	if(ccgs_can_halt_simulation()) {
		return true;
	}
	
	// Termination detection based on passed (committed) simulation time
	if(rootsim_config.simulation_time != 0 && (int)get_last_gvt() >= rootsim_config.simulation_time) {
		return true;
	}
		
	// If some KLT has encountered an error condition, we neatly shut down the simulation
	if(simulation_error()) {
		return true;
	}
	
	return false;
}


/**
* This function implements the main simulation loop
*
* @param arg This argument should be always set to NULL
*
* @author Francesco Quaglia
*/
static void *main_simulation_loop(void *arg) __attribute__ ((noreturn));
static void *main_simulation_loop(void *arg) {
	
	(void)arg;

	int kid_num_digits = (int)ceil(log10(n_ker));
	simtime_t my_time_barrier = -1.0;

	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	lp_alloc_thread_init();
	#endif
	
	// Worker Threads synchronization barrier: they all should start working together
	thread_barrier(&all_thread_barrier);
	
	while (!end_computing()) {
			
		// Recompute the LPs-thread binding
		rebind_LPs();

		// Check whether we have new ingoing messages sent by remote instances
		// and then process bottom halves
//		messages_checking();
		process_bottom_halves();

		// Activate one LP and process one event. Send messages produced during the events' execution
		schedule();

		my_time_barrier = gvt_operations();

		// Only a master thread can return a value different from -1
		if (D_DIFFER(my_time_barrier, -1.0)) {
			if (rootsim_config.verbose == VERBOSE_INFO || rootsim_config.verbose == VERBOSE_DEBUG) {
				printf("(%0*d) MY TIME BARRIER %f\n", kid_num_digits, kid, my_time_barrier);
				fflush(stdout);
			}
		}
	}

	// If we're exiting due to an error, we neatly shut down the simulation
	if(simulation_error()) {
		simulation_shutdown(EXIT_FAILURE);
	}
	simulation_shutdown(EXIT_SUCCESS);
}


/**
* This function implements the ROOT-Sim's entry point
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @param argc Number of arguments passed to ROOT-Sim
* @param argv Arguments passed to ROOT-Sim
* @return Exit code
*/


int main(int argc, char **argv) {

	SystemInit(argc, argv);

	if(rootsim_config.serial) {
		serial_simulation();
	} else {
	
		// The number of locally required threads is now set. Detach them and then join the main simulation loop
		if(!simulation_error()) {
			if(n_cores > 1) {
				create_threads(n_cores - 1, main_simulation_loop, NULL);
			}
		
			main_simulation_loop(NULL);
		}
	}
}

