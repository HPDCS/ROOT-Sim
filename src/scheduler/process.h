/**
 * @file scheduler/process.h
 *
 * @brief LP control blocks
 *
 * This header defines a LP control block, keeping information about both
 * simulation state and execution state as a user thread.
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
 * @author Roberto Vitali
 *
 * @date November 5, 2013
 */

#pragma once

#include <stdbool.h>

#include <mm/state.h>
#include <mm/mm.h>
#include <mm/ecs.h>
#include <datatypes/list.h>
#include <datatypes/msgchannel.h>
#include <arch/ult.h>
#include <lib/numerical.h>
#include <lib/abm_layer.h>
#include <lib/topology.h>
#include <communication/communication.h>
#include <arch/x86/linux/cross_state_manager/cross_state_manager.h>

#define LP_STACK_SIZE	4194304	// 4 MB

#define LP_STATE_READY			0x00001
#define LP_STATE_RUNNING		0x00002
#define LP_STATE_RUNNING_ECS		0x00004
#define LP_STATE_ROLLBACK		0x00008
#define LP_STATE_SILENT_EXEC		0x00010
#define LP_STATE_SUSPENDED		0x01010
#define LP_STATE_READY_FOR_SYNCH	0x00011	// This should be a blocked state! Check schedule() and stf()
#define LP_STATE_WAIT_FOR_SYNCH		0x01001
#define LP_STATE_WAIT_FOR_UNBLOCK	0x01002
#define LP_STATE_WAIT_FOR_DATA		0x01004

#define BLOCKED_STATE			0x01000
#define is_blocked_state(state)	(bool)(state & BLOCKED_STATE)

struct lp_struct {
	/// LP execution state.
	LP_context_t context;

	/// LP execution state when blocked during the execution of an event
	LP_context_t default_context;

	/// Process' stack
	void *stack;

	/// Memory map of the LP
	struct memory_map *mm;

	/// Local ID of the LP
	LID_t lid;

	/// Global ID of the LP
	GID_t gid;

	/// ID of the worker thread towards which the LP is bound
	unsigned int worker_thread;

	/// Current execution state of the LP
	short unsigned int state;

	/// This variable mainains the current checkpointing interval for the LP
	unsigned int ckpt_period;

	/// Counts how many events executed from the last checkpoint (to support PSS)
	unsigned int from_last_ckpt;

	/// If this variable is set, the next invocation to LogState() takes a new state log, independently of the checkpointing interval
	bool state_log_forced;

	/// The current state base pointer (updated by SetState())
	void *current_base_pointer;

	/// Input messages queue
	 list(msg_t) queue_in;

	/// Pointer to the last correctly processed event
	msg_t *bound;

	/// Output messages queue
	 list(msg_hdr_t) queue_out;

	/// Saved states queue
	 list(state_t) queue_states;

	/// Bottom halves
	msg_channel *bottom_halves;

	/// Processed rendezvous queue
	 list(msg_t) rendezvous_queue;

	/// Unique identifier within the LP
	unsigned long long mark;

	/// Buffer used by KLTs for buffering outgoing messages during the execution of an event
	outgoing_t outgoing_buffer;

	/**
	 * Implementation of OnGVT used for this LP. This can be changed
	 * at runtime by the autonomic subsystem, when dealing with ISS and SSS
	 */
	bool (*OnGVT)(unsigned int me, void *snapshot);

	/**
	 * Implementation of ProcessEvent used for this LP. This can be changed
	 * at runtime by the autonomic subsystem, when dealing with ISS and SSS
	 */
	void (*ProcessEvent)(unsigned int me, simtime_t now, int event_type,
			     void *event_content, unsigned int size,
			     void *state);

#ifdef HAVE_CROSS_STATE
	GID_t ECS_synch_table[MAX_CROSS_STATE_DEPENDENCIES];
	unsigned int ECS_index;
#endif

	unsigned long long wait_on_rendezvous;
	unsigned int wait_on_object;

	/* Per-Library variables */
	numerical_state_t numerical;

	/// pointer to the topology struct
	topology_t *topology;

	/// pointer to the region struct
	region_abm_t *region;
	
};

// LPs process control blocks and binding control blocks
extern struct lp_struct **lps_blocks;
extern __thread struct lp_struct **lps_bound_blocks;

/** This macro retrieves the LVT for the current LP. There is a small interval window
 *  where the value returned is the one of the next event to be processed. In particular,
 *  this happens in the scheduling function, when the bound is advanced to the next event to
 *  be processed, just before its actual execution.
 */
#define lvt(lp) (lp->bound != NULL ? lp->bound->timestamp : 0.0)

// TODO: see issue #121 to see how to make this ugly hack disappear
extern __thread unsigned int __lp_counter;
extern __thread unsigned int __lp_bound_counter;

#define foreach_lp(lp)		__lp_counter = 0;\
				for(struct lp_struct *(lp) = lps_blocks[__lp_counter]; __lp_counter < n_prc && ((lp) = lps_blocks[__lp_counter]); ++__lp_counter)

#define foreach_bound_lp(lp)	__lp_bound_counter = 0;\
				for(struct lp_struct *(lp) = lps_bound_blocks[__lp_bound_counter]; __lp_bound_counter < n_prc_per_thread && ((lp) = lps_bound_blocks[__lp_bound_counter]); ++__lp_bound_counter)

#define LPS_bound_set(entry, lp)	lps_bound_blocks[(entry)] = (lp);

extern void initialize_binding_blocks(void);
extern void initialize_lps(void);
extern struct lp_struct *find_lp_by_gid(GID_t);
