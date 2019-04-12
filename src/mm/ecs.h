/**
* @file mm/ecs.h
*
* @brief Event & Cross State Synchornization
*
* Event & Cross State Synchronization. This module implements the userspace handler
* of the artificially induced memory faults to implement transparent distributed memory.
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
* @author Matteo Principe
*/

#pragma once

typedef struct _ecs_page_node {

} ecs_page_node_t;

typedef struct _ecs_page_request {
	void *base_address;
	bool write_mode;
	unsigned int count;
	unsigned char buffer[];
} ecs_page_request_t;

extern void lp_alloc_deschedule(void);
extern void lp_alloc_schedule(void);
extern void lp_alloc_thread_init(void);
extern void setup_ecs_on_segment(msg_t *);
extern void ecs_send_pages(msg_t *);
extern void ecs_install_pages(msg_t *);
void unblock_synchronized_objects(LID_t lid);

#ifdef HAVE_ECS
extern void remote_memory_init(void);
#else
#define remote_memory_init()
#endif
