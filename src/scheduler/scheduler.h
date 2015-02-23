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
* @file scheduler.h
* @brief Scheduling Subsystem main header file. Scheduler-specific data structures
*        and defines should be placed into specific headers, not here.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/

#pragma once
#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <setjmp.h>

#include <ROOT-Sim.h>
#include <core/core.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <scheduler/stf.h>
#include <arch/ult.h>


/// This macro defines after how many idle cycles the simulation is stopped
#define MAX_CONSECUTIVE_IDLE_CYCLES	1000


/// Smallest Timestamp Scheduler's Code
#define SMALLEST_TIMESTAMP_FIRST	0

/* Functions invoked by other modules */
extern void scheduler_init(void);
extern void scheduler_fini(void);
extern void schedule(void);
extern void initialize_LP(unsigned int lp);
extern void initialize_worker_thread(void);
extern void activate_LP(unsigned int lp, simtime_t lvt, void *evt, void *state);



extern bool receive_control_msg(msg_t *);
extern bool process_control_msg(msg_t *);
extern bool reprocess_control_msg(msg_t *);
extern void rollback_control_message(unsigned int, simtime_t);
extern bool anti_control_message(msg_t * msg);

#ifdef  HAVE_LINUX_KERNEL_MAP_MODULE
extern void unblock_synchronized_objects(unsigned int);
#endif


extern __thread unsigned int current_lp;
extern __thread simtime_t current_lvt;
extern __thread msg_t *current_evt;
extern __thread void *current_state;
extern __thread unsigned int n_prc_per_thread;

#endif
