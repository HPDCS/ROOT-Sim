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
	char *live_bh;			// address of the live bottom half for the sobj
	char *expired_bh;		// address of the expired bottom half
	int   live_msgs;		// number of messages currently present inthe the live bottom half
	int   live_offset;		// offset of the oldest undelivered msg from the expired pool
	int   live_boundary;		// memory occupancy (in bytes) of live messages
	int   expired_msgs;		// number of messages currently present in the live bottom half
	int   expired_offset;		// offset of the oldest undelivered msg from the expired pool
	int   expired_boundary;		// memory occupancy (in bytes) of live messages
	char *actual_bh_addresses[2];	// these are the stable pointers seen for ottom half buffers' migration across numa nodes
};

#define MAX_MSG_SIZE sizeof(msg_t)
#define BH_PAGES	2000
#define BH_SIZE      	(BH_PAGES * PAGE_SIZE) //this is in bytes

extern bool BH_init(void);
extern void BH_fini(void);
extern int insert_BH(int sobj, void* msg, int size);
extern void *get_BH(unsigned int sobj);

#endif /* _BH_H */
