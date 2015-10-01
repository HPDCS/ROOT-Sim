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


#define PAGE_SIZE (4*1<<10)
#define TOTAL_MEMORY 262144L * PAGE_SIZE // This should be power of 2 multiplied by a page size. This is 1GB per LP.
#define BUDDY_GRANULARITY PAGE_SIZE	// This is the smallest chunk released by the buddy in bytes. TOTAL_MEMORY/BUDDY_GRANULARITY must be integer and a power of 2


extern bool allocator_init(void);
extern void allocator_fini(void);
extern char *allocate_pages(int num_pages);
extern void free_pages(void *ptr, size_t length);


#ifdef HAVE_NUMA
extern void **mem_areas;
#endif


#endif /* _ALLOCATOR_H */


