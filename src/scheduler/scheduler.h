/**
 * @file scheduler/scheduler.h
 *
 * @brief The ROOT-Sim scheduler main module header
 *
 * This module implements the schedule() function, which is the main
 * entry point for all the schedulers implemented in ROOT-Sim, and
 * several support functions which allow to initialize worker threads.
 *
 * Also, the LP_main_loop() function, which is the function where all
 * the User-Level Threads associated with Logical Processes live, is
 * defined here.
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
 */

#pragma once
#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <ROOT-Sim.h>
#include <core/core.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <scheduler/stf.h>
#include <arch/ult.h>
#include <scheduler/process.h>

/// This macro defines after how many idle cycles the simulation is stopped
#define MAX_CONSECUTIVE_IDLE_CYCLES	1000

enum {
	SCHEDULER_INVALID = 0,	/**< By convention 0 is the invalid field */
	SCHEDULER_STF			/**< Smallest Timestamp First Scheduler's Code */
};

/* Functions invoked by other modules */
extern void scheduler_init(void);
extern void scheduler_fini(void);
extern void schedule(void);
extern void schedule_on_init(struct lp_struct *next);
extern void initialize_worker_thread(void);
extern void activate_LP(struct lp_struct *, msg_t *);
extern void LP_main_loop(void *args);

extern bool receive_control_msg(msg_t *);
extern bool process_control_msg(msg_t *);
extern bool reprocess_control_msg(msg_t *);
extern void rollback_control_message(struct lp_struct *current, simtime_t);
extern bool anti_control_message(msg_t * msg);

#ifdef HAVE_PREEMPTION
extern void preempt_init(void);
extern void preempt_fini(void);
extern void reset_min_in_transit(unsigned int);
extern void update_min_in_transit(unsigned int, simtime_t);
void enable_preemption(void);
void disable_preemption(void);
#endif

extern __thread struct lp_struct *current;
extern __thread msg_t *current_evt;
extern __thread unsigned int n_prc_per_thread;

#ifdef HAVE_PREEMPTION
extern __thread volatile bool platform_mode;
#define switch_to_platform_mode() do {\
				   if(current->state != LP_STATE_SILENT_EXEC) {\
					platform_mode = true;\
				   }\
				  } while(0)

#define switch_to_application_mode() platform_mode = false
#else
#define switch_to_platform_mode() {}
#define switch_to_application_mode() {}
#endif				/* HAVE_PREEMPTION */

#endif
