/**
* @file communication/wnd.h
*
* @brief Message delivery support
*
* Message delivery support
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
* @author Tommaso Tocci
*/

#pragma once

#ifdef HAVE_MPI

#include <mpi.h>

#include <datatypes/list.h>
#include <mm/mm.h>
#include <ROOT-Sim.h>
#include <arch/atomic.h>

/// The structure representing a node in the @ref outgoing_queue list
typedef struct _outgoing_msg {
	MPI_Request req;		///< The MPI Request used to keep track of the delivery operation
	struct _outgoing_msg *next;	///< next pointer for the list
	struct _outgoing_msg *prev;	///< prev pointer for the list
	msg_t *msg;			///< A pointer to the @ref msg_t which MPI is delivering
} outgoing_msg;


/// An outgoing queue, to keep track of pending MPI-based message delivery
typedef struct _outgoing_queue {
	spinlock_t lock;		///< A lock used to protect access to the actual queue
	list(outgoing_msg) queue;	///< The actual list of pending message delivery operations
} outgoing_queue;

extern void outgoing_window_init(void);
extern void outgoing_window_finalize(void);
extern void store_outgoing_msg(outgoing_msg * out_msg, unsigned int dest_kid);
extern int prune_outgoing_queues(void);
extern outgoing_msg *allocate_outgoing_msg(void);

#endif	/* HAVE_MPI */
