/**
*                       Copyright (C) 2008-2017 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
*
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
* @file wnd.c
* @brief Message Delivery Support Module
* @author Tommaso Tocci
*/

#pragma once

#ifndef _WND_H_
#define _WND_H_

#ifdef HAVE_MPI


#include <mpi.h>

#include <datatypes/list.h>
#include <mm/dymelor.h>
#include <ROOT-Sim.h>
#include <arch/atomic.h>


typedef struct _outgoing_msg {
	MPI_Request req;
	msg_t *msg;
} outgoing_msg;


typedef struct _outgoing_queue {
	spinlock_t lock;
	list(outgoing_msg) queue;
} outgoing_queue;


void outgoing_window_init(void);
void outgoing_window_finalize(void);
outgoing_msg* allocate_outgoing_msg(void);
bool is_msg_delivered(outgoing_msg* msg);
void store_outgoing_msg(outgoing_msg* out_msg, unsigned int dest_kid);
int prune_outgoing_queue(outgoing_queue* oq);
int prune_outgoing_queues(void);
simtime_t min_timestamp_outgoing_msgs(void);
size_t outgoing_queues_size(void);


#endif /* HAVE_MPI */
#endif /* _WND_H_ */
