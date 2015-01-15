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
* @file gvt-fujimoto.c
* @brief This module implements the Fujimoto's and Hybinette's algorithm for GVT reduction
* 	 on shared memory architectures, as for the publication in:
* 	 Richard M. Fujimoto and Maria Hybinette. 1997. 
*        "Computing global virtual time in shared-memory multiprocessors".
*        ACM Trans. Model. Comput. Simul. 7, 4 (October 1997), 425-446.
*        DOI=10.1145/268403.268404 http://doi.acm.org/10.1145/268403.268404
*        This module is here just for performance comparison, is not usually compiled
*        into the simulation library, and will be eventually removed from the repository.
*
* @author Alessandro Pellegrini
*/


#include <gvt/gvt.h>
#include <arch/atomic.h>
#include <arch/thread.h>
#include <core/timer.h>
#include <core/core.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>

#ifdef SPINLOCK_GIVES_COUNT
atomic_t total_cs_retries;
#endif


// Timer to know when we have to start GVT computation.
// Each thread could start the GVT reduction phase, so this
// is a per-thread variable.
timer gvt_timer;


// The spinlock for the critical sections
static spinlock_t gvt_spinlock;



static atomic_t GVTFlag;
static simtime_t *PEMin;

static bool *have_computed_local_min;
static bool *processed_new_gvt;
static simtime_t *SendMin;

static __thread int LocalGVTFlag;


/**
* Initializer of the GVT subsystem
*
* @author Alessandro Pellegrini
*
*/
void gvt_init_fujimoto(void) {
	timer_start(gvt_timer);
	spinlock_init(&gvt_spinlock);
	PEMin = malloc(sizeof(simtime_t) * n_cores);
	have_computed_local_min = malloc(sizeof(bool) * n_cores);
	processed_new_gvt = malloc(sizeof(bool) * n_cores);
	SendMin = malloc(sizeof(simtime_t) * n_cores);
	atomic_set(&GVTFlag, 0);
	
	#ifdef SPINLOCK_GIVES_COUNT
	atomic_set(&total_cs_retries, 0);
	#endif
}




/**
* Finalizer of the GVT subsystem
*
* @author Alessandro Pellegrini
*/
void gvt_fini_fujimoto(void){
}


simtime_t GVT = INFTY;


/**
* Fujimoto's algorithm for GVT reduction on shared memory machines
*
* @author Richar M. Fujimoto
* @author Alessandro Pellegrini
* 
* @return The newly computed GVT value, or -1.0. Only a Master Thread should return a value
* 	  different from -1.0, to avoid generating too much information. If every thread
* 	  will return a value different from -1.0, nothing will be broken, but all the values
* 	  will be shown associated with the same kernel id (no way to distinguish between
* 	  different threads here).
*/
simtime_t gvt_operations_fujimoto(void) {
	register unsigned int i;
	int delta_gvt_timer;
	simtime_t new_gvt;
	
	#ifdef SPINLOCK_GIVES_COUNT
	unsigned int cs_retries;
	#endif
	
	delta_gvt_timer = timer_value(gvt_timer);
	if (abs(delta_gvt_timer) > (int)rootsim_config.gvt_time_period) {

		timer_restart(gvt_timer);

		#ifdef SPINLOCK_GIVES_COUNT
		cs_retries = spin_lock(&gvt_spinlock);
		#else
		spin_lock(&gvt_spinlock);
		#endif
		if(atomic_read(&GVTFlag) == 0) {
			
			// This is not listed in the paper, but it's clear that goes here...
			for(i = 0; i < n_cores; i++) {
				have_computed_local_min[i] = false;
				SendMin[i] = INFTY;
				PEMin[i] = INFTY;
				processed_new_gvt[i] = false;
			}
			GVT = INFTY;
			
			atomic_set(&GVTFlag, n_cores);
		}
		spin_unlock(&gvt_spinlock);
		
		#ifdef SPINLOCK_GIVES_COUNT
		atomic_add(&total_cs_retries, cs_retries);
		#endif
	}
	

	if(atomic_read(&GVTFlag) > 0 && LocalGVTFlag == 0) {
		// This is equivalent to storing GVTFlag at the beginning of the main loop,
		// it just wastes one cycle
		LocalGVTFlag = atomic_read(&GVTFlag);
		return -1.0;
	}
	
	
	if(LocalGVTFlag > 0 && !have_computed_local_min[tid]) {

		have_computed_local_min[tid] = true;
		
		#ifdef SPINLOCK_GIVES_COUNT
		cs_retries = spin_lock(&gvt_spinlock);
		#else
		spin_lock(&gvt_spinlock);
		#endif
		
		for(i = 0; i < n_prc_per_thread; i++) {
			if(LPS_bound[i]->bound != NULL) {
				PEMin[tid] = min(PEMin[tid], LPS_bound[i]->bound->timestamp);
			} else {
				PEMin[tid] = 0.0;
				break;
			}
		}
		
		PEMin[tid] = min(SendMin[tid], PEMin[tid]);
		
		atomic_dec(&GVTFlag);
		
		if(atomic_read(&GVTFlag) == 0) {
			new_gvt = INFTY;
			for(i = 0; i < n_cores; i++) {
				new_gvt = min(new_gvt, PEMin[i]);
			}
			spin_unlock(&gvt_spinlock);
			GVT = new_gvt;
			processed_new_gvt[tid] = true;
			return adopt_new_gvt(new_gvt);
		}
		
		spin_unlock(&gvt_spinlock);
		
		#ifdef SPINLOCK_GIVES_COUNT
		atomic_add(&total_cs_retries, cs_retries);

		if(master_thread()) {
			printf("total_cs_retries: %d\n", atomic_read(&total_cs_retries));
		}
		#endif

		LocalGVTFlag = 0;

	} else if(GVT < INFTY && !processed_new_gvt[tid]) {
		processed_new_gvt[tid] = true;
		return adopt_new_gvt(GVT);
	}
	
	return -1.0;
}
