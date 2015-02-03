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
* @file gvt.c
* @brief This module implements the GVT reduction. The current implementation
* 	 is non blocking for observable simulation plaftorms.
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*/


#include <ROOT-Sim.h>
#include <arch/thread.h>
#include <gvt/gvt.h>
#include <core/core.h>
#include <core/timer.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h> // this is for n_prc_per_thread
#include <statistics/statistics.h>



// Defintion of GVT-reduction phases
enum gvt_phases {phase_A, phase_send, phase_B, phase_aware, phase_end};



// Timer to know when we have to start GVT computation.
// Each thread could start the GVT reduction phase, so this
// is a per-thread variable.
timer gvt_timer;



/* Data shared across threads */

/// To be used with CAS to determine who is starting the next GVT reduction phase
static volatile unsigned int current_GVT_round = 0;

/// How many threads have left phase A?
static atomic_t counter_A;

/// How many threads have left phase send?
static atomic_t counter_send;

/// How many threads have left phase B?
static atomic_t counter_B;

/// How many threads are aware that the GVT reduction is over?
static atomic_t counter_aware;

/// How many threads have acquired the new GVT?
static atomic_t counter_end;

/** Flag to start a new GVT reduction phase. Explicitly using an int here,
 *  because 'bool' could be compiler dependent, but we must know the size
 *  beforehand, because we're going to use CAS on this. Changing the type could
 *  entail an undefined behaviour. 'false' and 'true' are usually int's (or can be
 *  converted to them by the compiler), so everything should work here.
 */
static volatile unsigned int GVT_flag = 0;


/** Keep track of the last computed gvt value. Its a per-thread variable
 * to avoid synchronization on it, but eventually all threads write here
 * the same exact value.
 */
static __thread simtime_t last_gvt = 0.0;



/* Per-thread private data */

/// What is my phase? All threads start in the initial phase
static __thread enum gvt_phases my_phase = phase_A;

/// Per-thread GVT round counter
static __thread unsigned int my_GVT_round = 0;

/// The local (per-thread) minimum. It's not TLS, rather an array, to allow reduction by master thread
static simtime_t *local_min;






/**
* Initialization of the GVT subsystem
*
* @author Alessandro Pellegrini
*/
void gvt_init(void) {
	unsigned int i;

	// This allows the first GVT phase to start
	atomic_set(&counter_end, 0);

	// Initialize the local minima
	local_min = malloc(sizeof(simtime_t) * n_cores);
	for(i = 0; i < n_cores; i++) {
		local_min[i] = INFTY;
	}

	timer_start(gvt_timer);
}




/**
* Finalizer of the GVT subsystem
*
* @author Alessandro Pellegrini
*/
void gvt_fini(void){
}


/**
 * This function returns the last computed GVT value at each thread.
 * It can be safely used concurrently to keep track of the evolution of
 * the committed trajectory. It's so far mainly used for termination
 * detection based on passed simulation time.
 */
inline simtime_t get_last_gvt(void) {
	return last_gvt;
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
* @author Alessandro Pellegrini
*
* @return The newly computed GVT value, or -1.0. Only a Master Thread should return a value
* 	  different from -1.0, to avoid generating too much information. If every thread
* 	  will return a value different from -1.0, nothing will be broken, but all the values
* 	  will be shown associated with the same kernel id (no way to distinguish between
* 	  different threads here).
*/
simtime_t gvt_operations(void) {
	register unsigned int i;
	simtime_t new_gvt;

	// These variables are used to check if certain phases are (locally) passed.
	// This allows to implment the same Algorithm 2 in the paper
	// without having to manually interact with the other subsystems from
	// here (at the cost of executing more main-loop cycles) to converge
	// to a correct value of the GVT.
	static __thread bool local_my_GVT_phase_send_executed = false;
	static __thread bool local_my_GVT_phase_B_executed = false;


	// GVT reduction initialization.
	// This is different from the paper's pseudocode to reduce
	// slightly the number of clock reads
	if(GVT_flag == 0 && atomic_read(&counter_end) == 0) {

		// Has enough time passed since the last GVT reduction?
		if ( timer_value_milli(gvt_timer) > (int)rootsim_config.gvt_time_period &&
		    iCAS(&current_GVT_round, my_GVT_round, my_GVT_round + 1)) {

			// Reset atomic counters and make all threads compute the GVT
			atomic_set(&counter_A, n_cores);
			atomic_set(&counter_send, n_cores);
			atomic_set(&counter_B, n_cores);
			atomic_set(&counter_aware, n_cores);
			atomic_set(&counter_end, n_cores);
			GVT_flag = 1;

			timer_restart(gvt_timer);
		}
	}


	if(GVT_flag == 1) {

		if(my_phase == phase_A) {

			// Someone has modified the GVT round (possibly me).
			// Keep track of this update
			my_GVT_round = current_GVT_round;

			for(i = 0; i < n_prc_per_thread; i++) {
				if(LPS_bound[i]->bound != NULL) {
					local_min[tid] = min(local_min[tid], LPS_bound[i]->bound->timestamp);
				} else {
					local_min[tid] = 0.0;
					break;
				}
			}
			my_phase = phase_send;	// Entering phase send
			atomic_dec(&counter_A);	// Notify finalization of phase A
			return -1.0;
		}


		if(my_phase == phase_send && atomic_read(&counter_A) == 0 && !local_my_GVT_phase_send_executed) {
			// Actually execute phase send after I have executed an additional
			// cycle. This guarantees that operations at lines 21, 22, and 23
			// in Algorithm 2 are actually executed, without calling other subsystems
			// from here.
			local_my_GVT_phase_send_executed = true;
			return -1.0;
		}

		if(local_my_GVT_phase_send_executed) {
			// Reset the flag to check whether we have executed an
			// additional cycle in case of the send phase
			local_my_GVT_phase_send_executed = false;

			my_phase = phase_B;
			atomic_dec(&counter_send);
			return  -1.0;
		}

		if(my_phase == phase_B && atomic_read(&counter_send) == 0 && !local_my_GVT_phase_B_executed) {
			// Same case for the previous phase_send here, regarding line 29 of the algorithm
			local_my_GVT_phase_B_executed = true;
			return -1.0;
		}

		if(local_my_GVT_phase_B_executed) {
			local_my_GVT_phase_B_executed = false;

			for(i = 0; i < n_prc_per_thread; i++) {
				if(LPS_bound[i]->bound != NULL) {
					local_min[tid] = min(local_min[tid], LPS_bound[i]->bound->timestamp);
				} else {
					local_min[tid] = 0.0;
					break;
				}
			}

			my_phase = phase_aware;
			atomic_dec(&counter_B);
			return  -1.0;
		}


		if(my_phase == phase_aware) {
			new_gvt = INFTY;

			for(i = 0; i < n_cores; i++) {
				new_gvt = min(local_min[i], new_gvt);
			}

			atomic_dec(&counter_aware);

			my_phase = phase_end;

			if(atomic_read(&counter_aware) == 0) {
				// The last one passing here, resets GVT flag
				iCAS(&GVT_flag, 1, 0);
			}

			// Dump statistics
			statistics_post_other_data(STAT_GVT, new_gvt);

			// Execute fossil collection and termination detection
			// Each thread stores the last computed value in last_gvt,
			// while the return value is the gvt only for the master
			// thread. To check for termination based on simulation time,
			// this variable must be explicitly inspected using
			// get_last_gvt()
			last_gvt = adopt_new_gvt(new_gvt);;
			return last_gvt;
		}


	} else {

		// GVT flag is not set. We check whether we can reset the
		// internal thread's state, waiting for the beginning of a
		// new phase.
		if(my_phase == phase_end) {

			// Back to phase A for next GVT round
			my_phase = phase_A;
			local_min[tid] = INFTY;
			atomic_dec(&counter_end);
		}
	}

	return -1.0;
}

