/**
* @file mm/state.h
*
* @brief LP state management
*
* The state module is responsible for managing LPs' simulation states.
* In particular, it allows to take a snapshot, to restore a previous snapshot,
* and to silently re-execute a portion of simulation events to bring
* a LP to a partiuclar LVT value for which no simulation state is available
* in the log chain.
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
*/

#pragma once

#include <ROOT-Sim.h>
#include <core/core.h>
#include <lib/numerical.h>
#include <lib/abm_layer.h>
#include <lib/topology.h>

enum {
	STATE_SAVING_INVALID = 0,	/**< By convention 0 is the invalid field */
	STATE_SAVING_COPY,			/**< Copy State Saving checkpointing interval */
	STATE_SAVING_PERIODIC		/**< Periodic State Saving checkpointing interval */
};

/// Structure for LP's state
typedef struct _state_t {
	// Pointers to chain this structure to the state queue
	struct _state_t *next;
	struct _state_t *prev;

	/// Simulation time associated with the state log
	simtime_t lvt;
	/// A pointer to the actual log
	void *log;
	/// This log has been taken after the execution of this event
	msg_t *last_event;

	/* Per-LP fields which should be transparently rolled back */

	/// Execution state
	short unsigned int state;
	/// This is a pointer used to keep track of changes to simulation states via SetState()
	void *base_pointer;

	/* Library state fields */
	numerical_state_t numerical;
	
	topology_t *topology;

	void *region_data;
} state_t;

struct lp_struct;

extern bool LogState(struct lp_struct *);
extern void RestoreState(struct lp_struct *, state_t * restore_state);
extern void rollback(struct lp_struct *);
extern state_t *find_time_barrier(struct lp_struct *, simtime_t time);
extern void clean_queue_states(struct lp_struct *, simtime_t new_gvt);
extern void rebuild_state(struct lp_struct *, state_t * state_pointer, simtime_t time);
extern void set_checkpoint_period(struct lp_struct *, int period);
extern void force_LP_checkpoint(struct lp_struct *);
extern unsigned int silent_execution(struct lp_struct *, msg_t * evt, msg_t * final_evt);
