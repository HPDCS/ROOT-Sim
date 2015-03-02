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

#include <core/core.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>


static simtime_t * volatile min_in_transit_lvt;


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
	}
}


void preempt_fini(void) {

	if(rootsim_config.disable_preemption)
		return;

	rsfree(min_in_transit_lvt);
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


/**
 * This function is activated when control is transferred back from
 * kernel space, when an APIC timer interrupt is received. When this is
 * the case, this function quickly checks whether some other LP has
 * gained an increased priority over the currently executing one, and
 * in the case changes the control flow so as to activate it.
 */
void preempt(void) {
	
	if(rootsim_config.disable_preemption)
		return;

	// if min_in_transit_lvt < current_lvt
}


#endif /* HAVE_PREEMPTION */
