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
* @file process.h
* @brief This header defines a LP control block, keeping information about both
*        simulation state and execution state as a user thread.
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
* @date November 5, 2013
*/


#pragma once
#ifndef __LP_H_
#define __LP_H_

#include <stdbool.h>

#include <mm/state.h>
#include <mm/dymelor.h>
#include <datatypes/list.h>
#include <scheduler/group.h>
#include <scheduler/scheduler.h>
#include <arch/ult.h>
#include <arch/atomic.h>
#include <lib/numerical.h>
#include <communication/communication.h>
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>

#define LP_STACK_SIZE	4194304	// 4 MB


#define LP_STATE_READY			0x00001
#define LP_STATE_RUNNING		0x00002
#define LP_STATE_ROLLBACK		0x00004
#define LP_STATE_SILENT_EXEC		0x00008
#define LP_STATE_SUSPENDED		0x01010
#define LP_STATE_READY_FOR_SYNCH	0x00011
#define LP_STATE_WAIT_FOR_SYNCH		0x01001
#define LP_STATE_WAIT_FOR_UNBLOCK	0x01002
#define LP_STATE_WAIT_FOR_GROUP		0x01003
#define LP_STATE_WAIT_FOR_LOG		0x01004


#define BLOCKED_STATE			0x01000
#define is_blocked_state(state)	(bool)(state & BLOCKED_STATE)


typedef struct _LP_state {
#ifdef ENABLE_ULT
	/// LP execution state
	LP_context_t	context;

	/// LP execution state when blocked during the execution of an event
	LP_context_t	default_context;

	/// Process' stack
	void 		*stack;
#endif /* ENABLE_ULT */

	/// Local ID of the thread (used to translate from bound LPs to local id)
	unsigned int 	lid;

	/// Logical Process lock, used to serialize accesses to concurrent data structures
	spinlock_t	lock;

	/// Seed to generate pseudo-random values
	seed_type	seed;


	/// ID of the worker thread towards which the LP is bound
	unsigned int	worker_thread;

	/// Current execution state of the LP
	short unsigned int state;

	/// This variable mainains the current checkpointing interval for the LP
	unsigned int	ckpt_period;

	/// Counts how many events executed from the last checkpoint (to support PSS)
	unsigned int	from_last_ckpt;

	/// If this variable is set, the next invocation to LogState() takes a new state log, independently of the checkpointing interval
	bool		state_log_forced;

	/// The current state base pointer (updated by SetState())
	void 		*current_base_pointer;

	/// Input messages queue
	list(msg_t)	queue_in;

	/// Pointer to the last correctly elaborated event
	msg_t		*bound;

	/// Output messages queue
	list(msg_hdr_t)	queue_out;

	/// Saved states queue
	list(state_t)	queue_states;

	/// Bottom halves queue
	list(msg_t)	bottom_halves;

	/// Processed rendezvous queue
	list(msg_t)	rendezvous_queue;

	/// Unique identifier within the LP
	unsigned long long	mark;

	/// Buffer used by KLTs for buffering outgoing messages during the execution of an event
	outgoing_t outgoing_buffer;

	#ifdef HAVE_CROSS_STATE
	unsigned int ECS_synch_table[MAX_CROSS_STATE_DEPENDENCIES];
	unsigned int ECS_index;
	#endif

	unsigned long long	wait_on_rendezvous;
	unsigned int		wait_on_object;

	//TODO MN
	#ifdef HAVE_GLP_SCH_MODULE
	unsigned int current_group;
	ECS_stat ** ECS_stat_table;
	msg_t *target_rollback;
	bool updated_counter;
	char dummy[3];
	#endif

} LP_state;


/** This macro retrieves the LVT for the current LP. There is a small interval window
 *  where the value returned is the one of the next event to be processed. In particular,
 *  this happens during the scheduling, when the bound is advanced to the next event to
 *  be processed, just before its actual execution.
 */
#define lvt(lid) (LPS[lid]->bound != NULL ? LPS[lid]->bound->timestamp : 0.0)



extern LP_state **LPS;

extern __thread LP_state **LPS_bound;

#endif /* __LP_H_ */

