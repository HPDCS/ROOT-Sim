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
* @file bh.h
* @brief
* @author Francesco Quaglia
*/

#pragma once
#ifndef _BH_H
#define _BH_H

#include <core/core.h>

struct _bhmap {
	char		*live_bh;		// address of the live bottom half for the sobj
	char		*expired_bh;		// address of the expired bottom half
	unsigned int	live_msgs;		// number of messages currently present inthe the live bottom half
	unsigned int	live_offset;		// offset of the oldest undelivered msg from the expired pool
	size_t		live_boundary;		// memory occupancy (in bytes) of live messages
	unsigned int	expired_msgs;		// number of messages currently present in the live bottom half
	unsigned int	expired_offset;		// offset of the oldest undelivered msg from the expired pool
	unsigned int	expired_boundary;	// memory occupancy (in bytes) of live messages
	char		*actual_bh_addresses[2];// these are the stable pointers used for bottom half buffers' migration across numa nodes
	size_t		current_pages[2];	// The amount of pages allocated in the corresponding entry of the actual_bh_addessess vector
};

#define MAX_MSG_SIZE sizeof(msg_t)
#define INITIAL_BH_PAGES	10

extern bool BH_init(void);
extern void BH_fini(void);
extern int insert_BH(int sobj, void* msg, int size);
extern void *get_BH(unsigned int sobj);

#endif /* _BH_H */
