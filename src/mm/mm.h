/**
*			Copyright (C) 2008-2017 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file allocator.h
* @brief
* @author Francesco Quaglia
*/

#pragma once

#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <core/core.h>
#include <arch/atomic.h>

typedef struct _map_move {
	spinlock_t	spinlock;
	unsigned 	target_node;
	int      	need_move;
	int    		in_progress;
} map_move;


struct _buddy {
	size_t size;
	size_t longest[1];
};

#ifndef PAGE_SIZE
 #if defined(ARCH_X86_64) || defined(ARCH_X86)
   #define PAGE_SIZE (4<<10)
 #else
   #error Unable to determine page size
 #endif
#endif
#define PER_LP_PREALLOCATED_MEMORY (262144L * PAGE_SIZE) // This should be power of 2 multiplied by a page size. This is 1GB per LP.
#define BUDDY_GRANULARITY PAGE_SIZE	// This is the smallest chunk released by the buddy in bytes. PER_LP_PREALLOCATED_MEMORY/BUDDY_GRANULARITY must be integer and a power of 2
#define MAX_MMAP	(131072L * PAGE_SIZE) // This is the maximum amount of memory that a single mmap() call is able to serve. TODO: this should be checked within configure.ac
#define NUM_MMAP	PER_LP_PREALLOCATED_MEMORY / MAX_MMAP


#ifdef HAVE_NUMA
extern void **mem_areas;
#endif

// TODO: no need to keep a structure anymore...
// This is for the segment allocator
typedef struct _lp_mem_region{
	char* base_pointer;
}lp_mem_region;

#define SUCCESS_AECS                  0
#define FAILURE_AECS                 -1
#define INVALID_SOBJS_COUNT_AECS     -99
#define INIT_ERROR_AECS              -98
#define INVALID_SOBJ_ID_AECS         -97
#define MDT_RELEASE_FAILURE_AECS     -96

extern bool allocator_init(void);
extern void allocator_fini(void);
extern void *get_segment(unsigned int i);
void *get_base_pointer(unsigned int gid);
