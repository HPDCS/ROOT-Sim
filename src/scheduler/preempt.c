/**
 * @file scheduler/preempt.c
 *
 * @brief LP preemption management
 *
 * LP preemption management
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
 *
 * @date March, 2015
 */

#ifdef HAVE_PREEMPTION

#include <timestretch.h>

#include <arch/atomic.h>
#include <core/core.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>

extern void preempt_callback(void);

static simtime_t *volatile min_in_transit_lvt;

atomic_t preempt_count;
atomic_t overtick_platform;
atomic_t would_preempt;

__thread volatile bool platform_mode = true;

void preempt_init(void)
{
	register unsigned int i;
	int ret;

	if (rootsim_config.disable_preemption)
		return;

	min_in_transit_lvt = rsalloc(sizeof(simtime_t) * n_cores);
	for (i = 0; i < n_cores; i++) {
		min_in_transit_lvt[i] = INFTY;
	}

	// Try to get control over libtimestretch device file
	ret = ts_open();
	if (ret == TS_OPEN_ERROR) {
		rootsim_error(false, "libtimestretch unavailable: is the module mounted? Will run without preemption...\n");
		rootsim_config.disable_preemption = true;
	}

	atomic_set(&preempt_count, 0);

//      enable_preemption();
}

void preempt_fini(void)
{

	if (rootsim_config.disable_preemption)
		return;

	rsfree(min_in_transit_lvt);

	printf("Total preemptions: %d\n", atomic_read(&preempt_count));

//      disable_preemption();

	//ts_close();
}

void reset_min_in_transit(unsigned int thread)
{
	simtime_t local_min;

	if (rootsim_config.disable_preemption)
		return;

	local_min = min_in_transit_lvt[thread];

	// If the CAS (unlikely) fails, then some other thread has updated
	// the min in transit. We do not retry the operation, as this
	// case must be handled.
	CAS((uint64_t *) & min_in_transit_lvt[thread], UNION_CAST(local_min, uint64_t), UNION_CAST(INFTY, uint64_t));
}

void update_min_in_transit(unsigned int thread, simtime_t lvt)
{
	simtime_t local_min;

	if (rootsim_config.disable_preemption)
		return;

	do {
		local_min = min_in_transit_lvt[thread];

		if (lvt >= local_min) {
			break;
		}
	// simtime_t is 64-bits wide, so we use 64-bits CAS
	} while (!CAS((uint64_t *) & min_in_transit_lvt[thread], UNION_CAST(local_min, uint64_t), UNION_CAST(lvt, uint64_t)));
}

__thread bool rolling_back = false;

/**
 * This function is activated when control is transferred back from
 * kernel space, when an APIC timer interrupt is received. When this is
 * the case, this function quickly checks whether some other LP has
 * gained an increased priority over the currently executing one, and
 * in the case changes the control flow so as to activate it.
 */
__attribute__((used))
void preempt(void)
{
//      return;

	if (rootsim_config.disable_preemption)
		return;

	// if min_in_transit_lvt < current_lvt
	// and in platform mode

/*	if(current != NULL) {
		if( (count++) % 100 == 0)
			printf("Overtick interrupt from application mode\n");
	}
*/

//      atomic_inc(&preempt_count);

	if (platform_mode || rolling_back) {
//              atomic_inc(&overtick_platform);

	} else if (min_in_transit_lvt[local_tid] < current_lvt) {
//              atomic_inc(&would_preempt);
		current->state = LP_STATE_SUSPENDED;	// Questo triggera la logica di ripartenza dell'LP di ECS, ma forse va cambiato nome...
		switch_to_platform_mode();
		context_switch(&current->context, &kernel_context);
	}

/*	if(!platform_mode && min_in_transit_lvt[local_tid] < current_lvt) {

		atomic_inc(&preempt_count);

		current->state = LP_STATE_READY_FOR_SYNCH; // Questo triggera la logica di ripartenza dell'LP di ECS, ma forse va cambiato nome...

		// Assembly module has placed a lot of stuff on the stack: put everything back
		// TODO: this is quite nasty, find a cleaner way to do this...
		__asm__ __volatile__("pop %r15;"
			             "pop %r14;"
			             "pop %r13;"
			             "pop %r12;"
			             "pop %r11;"
			             "pop %r10;"
			             "pop %r9;"
			             "pop %r8;");

		__asm__ __volatile__("pop %rsi;"
			             "pop %rdi;"
			             "pop %rdx;"
			             "pop %rcx;"
			             "pop %rbx;"
			             "pop %rax;"
			             "popfq;"
			             "pop %rbp;");

	}
*/

}

void enable_preemption(void)
{
	if (register_ts_thread() != TS_REGISTER_OK) {
		rootsim_error(true, "Error activating high-frequency interrupts. Aborting...\n");
	}

	if (register_callback(preempt_callback) != TS_REGISTER_CALLBACK_OK) {
		rootsim_error(true, "Error registering callback from kernel module. Aborting...\n");
	}

	if (ts_start(0) != TS_START_OK) {
		rootsim_error(true, "General error\n");
	}

}

void disable_preemption(void)
{
	if (deregister_ts_thread() != TS_DEREGISTER_OK) {
		rootsim_error(true, "Error de-activating high-frequency interrupts. Aborting...\n",);
	}
}

#endif /* HAVE_PREEMPTION */
