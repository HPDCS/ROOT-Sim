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
* @file preempt.c
* @brief LP preemption management
* @author Alessandro Pellegrini
* @author Francesco Quaglia
* @date March, 2015
*/


#ifdef HAVE_PREEMPTION

#include <timestretch.h>

#include <arch/atomic.h>
#include <core/core.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>

#define ACTUAL_PREEMPTION if(1)

extern void preempt_callback(void); // This is defined in src/scheduler/preempt_callback.S

//static simtime_t * volatile min_in_transit_lvt;
simtime_t * volatile min_in_transit_lvt;

atomic_t preempt_count;
atomic_t overtick_platform;
atomic_t would_preempt;
atomic_t overtick_user;
atomic_t need_resched;

__thread volatile bool platform_mode = true;
__thread volatile bool rolling_back = false;


void preempt_init(void) {
	register unsigned int i;
	int ret;

	if(rootsim_config.disable_preemption)
		return;

	min_in_transit_lvt = rsalloc(sizeof(simtime_t) * n_cores);
	for(i = 0; i < n_cores; i++) {
		min_in_transit_lvt[i] = INFTY;
	}
	
	// Try to get control over libtimestretch device file
	ret = ts_open();
	if(ret == TS_OPEN_ERROR) {
		rootsim_error(false, "libtimestretch unavailable: is the module mounted? Will run without preemption...\n");
		rootsim_config.disable_preemption = true;
	} else {
		printf("TS OPEN OK\n");
	}

	atomic_set(&preempt_count, 0);

}


void preempt_fini(void) {

	if(rootsim_config.disable_preemption)
		return;

	rsfree(min_in_transit_lvt);

	printf("Total preemptions: %d\n", atomic_read(&preempt_count));
}


void reset_min_in_transit(unsigned int thread) {
	simtime_t local_min;

	if(rootsim_config.disable_preemption)
		return;

	local_min  = min_in_transit_lvt[thread];

	// If the CAS (unlikely) fails, then some other thread has updated
	// the min in transit. We do not retry the operation, as this
	// case must be handled.
	CAS((uint64_t *)&min_in_transit_lvt[thread], UNION_CAST(local_min, uint64_t), UNION_CAST(INFTY, uint64_t));
}


void update_min_in_transit(unsigned int thread, simtime_t lvt) {
	simtime_t local_min;

	if(rootsim_config.disable_preemption)
		return;

	do {
		local_min = min_in_transit_lvt[thread];

		if(lvt >= local_min) {
			break;
		}

	// simtime_t is 64-bits wide, so we use 64-bits CAS
	} while(!CAS((uint64_t *)&min_in_transit_lvt[thread], UNION_CAST(local_min, uint64_t), UNION_CAST(lvt, uint64_t)));
}


void resched(void){

ACTUAL_PREEMPTION{
                        LPS[current_lp]->state = LP_STATE_SUSPENDED; // Questo triggera la logica di ripartenza dell'LP di ECS, ma forse va cambiato nome...
                        switch_to_platform_mode();
                        context_switch(&LPS[current_lp]->context, &kernel_context);
                        }


}


/**
 * This function is activated when control is transferred back from
 * kernel space, when an APIC timer interrupt is received. When this is
 * the case, this function quickly checks whether some other LP has
 * gained an increased priority over the currently executing one, and
 * in the case changes the control flow so as to activate it.
 */
void preempt(void) {

//	if(rootsim_config.disable_preemption)
//		return;


	// if min_in_transit_lvt < current_lvt
	// and in platform mode

	atomic_inc(&preempt_count);

	if(rolling_back){
		atomic_inc(&overtick_platform);
		return;
	}

	if(!platform_mode && !rolling_back) {

		atomic_inc(&overtick_user);

		if(min_in_transit_lvt[tid] < current_lvt) {
			atomic_inc(&would_preempt);
		ACTUAL_PREEMPTION{
			LPS[current_lp]->state = LP_STATE_SUSPENDED; // Questo triggera la logica di ripartenza dell'LP di ECS, ma forse va cambiato nome...
			switch_to_platform_mode();
			context_switch(&LPS[current_lp]->context, &kernel_context);
			}
		}
	} else {
		atomic_inc(&overtick_platform);
	}

}


void enable_preemption(void) {
	int ret;

	if((ret = register_ts_thread()) != TS_REGISTER_OK) {
		rootsim_error(true, "%s:%d: Error activating high-frequency interrupts. Aborting...\n", __FILE__, __LINE__);
	}

	if(register_callback(preempt_callback) != TS_REGISTER_CALLBACK_OK) {
		rootsim_error(true, "%s:%d: Error registering callback from kernel module. Aborting...\n", __FILE__, __LINE__);
	}

}


void disable_preemption(void) {

	if(deregister_ts_thread() != TS_DEREGISTER_OK) {
		rootsim_error(true, "%s:%d: Error de-activating high-frequency interrupts. Aborting...\n", __FILE__, __LINE__);
	}
}


#endif /* HAVE_PREEMPTION */
