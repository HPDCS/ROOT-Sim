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
#include <datatypes/list.h>
#include <scheduler/scheduler.h>
#include <arch/ult.h>
#include <arch/atomic.h>
#include <lib/numerical.h>
#include <mm/modules/ktblmgr/ktblmgr.h>
#include <communication/communication.h>


#define LP_STACK_SIZE	4194304	// 4 MB


#define LP_STATE_READY			0x00001
#define LP_STATE_RUNNING		0x00002
#define LP_STATE_ROLLBACK		0x00004
#define LP_STATE_SILENT_EXEC		0x00008
#define LP_STATE_READY_FOR_SYNCH	0x00010
#define LP_STATE_WAIT_FOR_SYNCH		0x01001
#define LP_STATE_WAIT_FOR_UNBLOCK	0x01002


#define BLOCKED_STATE			0x01000
#define is_blocked_state(state)	(bool)(state & BLOCKED_STATE)


typedef struct _LP_state {
	
	/// Local ID of the thread (used to translate from bound LPs to local id)
	unsigned int 	lid;
	
	/// Logical Process lock, used to serialize accesses to concurrent data structures
	spinlock_t	lock;

	/// Seed to generate pseudo-random values
	seed_type	seed;
	
	/// LP execution state
	LP_context_t	context;
	
	/// LP execution state when blocked during the execution of an event 
	LP_context_t	default_context;

	/// ID of the worker thread towards which the LP is bound
	unsigned int	worker_thread;
	
	/// Process' stack
	void 		*stack;

	/// Current execution state of the LP
	short unsigned int state;

	/// This variable mainains the current checkpointing interval for the LP
	unsigned int	ckpt_period;
	
	/// Counts how many events executed from the last checkpoint
	unsigned int	from_last_ckpt;
	
	/// If this variable is set, the next invocation to LogState() takes a new state log, independently of the checkpointing interval
	bool		state_log_forced;

	/// Input messages queue
	list(msg_t)	queue_in;
	
	/// Pointer to the last correctly elaborated message/event
	msg_t		*bound;
	
	/// Pointer to the bound that must be restored due to a rollback operation. If this pointer is not NULL, then the LP's state *must* be LP_STATE_ROLLBACK, to notify the kernel that a rollback operation must be executed upon next scheduling operation.
//	msg_t		*rollback_bound;
	
	/// Output messages queue
	list(msg_hdr_t)	queue_out;
	
	/// Saved states queue
	list(state_t)	queue_states;
	
	/// Pointer to the last-taken snapshot
	state_t		*state_bound;
	
	/// Bottom halves queue
	list(msg_t)	bottom_halves;

	/// Processed rendezvous queue
	list(msg_t)	rendezvous_queue;


/* ROBA DA RISISTEMARE */


	/// Counter of the total events, committed and undone for each local LP
	unsigned long 	total_events;
	
	// Struggle messages counter
	unsigned long 	count_stragglers;
	
	/// Antimessages counter
	unsigned long 	total_antimessages;

	/// Event total execution time
	double		event_total_time;

	long int	count_rollbacks;
	
	long int	saved_states_counter;
	
	
	


	



/* ROBA DA REIMPLEMENTARE */




	/// The first snapshot after the first transitorial phase
//	state_t		*first_snapshot;
	
	/// The last collected snapshot
//	state_t 	*last_snapshot;
	




/* ROBA DA VEDERE SE SERVE ANCORA */



	
	
	/// Minimum local virtual time of the LP
//	simtime_t	min_lvt;

	/// Unique identifier within the LP
	unsigned long long	mark;
	
//	double		current_time;
	
	
	 

	
	/// Counts how many events executed from the last check of the dirty state dimension
//	int		from_last_check;
	
	/// Counts how many events executed from the last adaptive operation
//	int		from_last_adapt_point;
	
	/// Total time fo state saving
//	double		ckpt_total_time;
	
//	double		old_committed_time;
	
//	double		total_time;
	
//	double		old_total_time;
	
//	long int	contatore_stati_salvati;

	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	unsigned int ECS_synch_table[MAX_CROSS_STATE_DEPENDENCIES];
	unsigned int ECS_index;
	#endif

	/// Buffer used by KLTs for buffering outgoing messages during the execution of an event
	outgoing_t outgoing_buffer;


	unsigned long long	wait_on_rendezvous;
	unsigned int		wait_on_object;
	

	
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

