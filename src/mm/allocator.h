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
* @file allocator.h
* @brief 
* @author Francesco Quaglia
*/


#pragma once
#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <core/core.h>

/*
typedef struct _mem_map {
	char* base;   //base address of the chain of meta-data tables for the memory map of the sobj	
	int   size;   //maximum number of entries in the current meta-data tables of the memory map of the sobj
	int   active;   //number of valid entries in the meta-data tables of the memory map of the sobj
	char* live_bh; //address of the live bottom half for the sobj
	char* expired_bh; //address of the expired bottom half
	int   live_msgs; //number of messages currently present in the the live bottom half
	int   live_offset; // offset of the oldest undelivered msg from the expired pool
	int   live_boundary; //memory occupancy (in bytes) of live messages
	int   expired_msgs; //number of messages currently present in the live bottom half
	int   expired_offset; // offset of the oldest undelivered msg from the expired pool
	int   expired_boundary; //memory occupancy (in bytes) of live messages
	char* actual_bh_addresses[2];// these are the stable pointers seen for bottom half buffers' migration across numa nodes
} mem_map; 
*/

/*
typedef struct _mdt_entry { //mdt stands for 'meta data table'
	char* addr;
	int   numpages;
} mdt_entry;
*/

typedef struct _map_move {
	pthread_spinlock_t spinlock;
	unsigned 	   target_node;
	int      	   need_move;
	int    		   in_progress;
} map_move; 


struct _buddy {
    size_t size;
    size_t longest[1];
};


#define PAGE_SIZE (4*1<<10)
#define TOTAL_MEMORY 262144 * PAGE_SIZE // This should be power of 2 multiplied by a page size. This is 1GB per LP.
#define BUDDY_GRANULARITY 256	// This is the smallest chunk released by the buddy in bytes. TOTAL_MEMORY/BUDDY_GRANULARITY must be integer and a power of 2



/*
#define MDT_PAGES	80

#define MDT_ENTRIES ((MDT_PAGES * PAGE_SIZE) / sizeof(mdt_entry))
#define MAX_SEGMENT_SIZE 16384 // this is expressed in number of pages
 
#define MAX_SOBJS  	MAX_LPs

#define SUCCESS 		0
#define FAILURE			-1
#define INVALID_SOBJS_COUNT 	-99
#define INIT_ERROR		-98
#define INVALID_SOBJ_ID 	-97
#define MDT_RELEASE_FAILURE	-96
*/

extern bool allocator_init(void);
extern void allocator_fini(void);
extern char *allocate_pages(int num_pages);

#endif /* _ALLOCATOR_H */


