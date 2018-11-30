/**
*			Copyright (C) 2008-2018 HPDCS Group
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
* @file mm.h
* @brief
* @author Francesco Quaglia
*/

#pragma once

#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <core/core.h>
#include <arch/atomic.h>
#include <mm/dymelor.h>


struct segment {
	unsigned char *base;
	unsigned char *brk;
};

struct buddy {
	size_t size;
	size_t longest[];
};

extern size_t __page_size;
#define PAGE_SIZE ({ \
			if(unlikely(__page_size == 0))\
				__page_size = getpagesize();\
			__page_size;\
		  })


struct memory_map {
	malloc_state	*m_state;
	struct buddy	*buddy;
	struct segment	*segment;
	spinlock_t	mm_lock;
};


#define PER_LP_PREALLOCATED_MEMORY (262144L * PAGE_SIZE) // This should be power of 2 multiplied by a page size. This is 1GB per LP.
#define BUDDY_GRANULARITY PAGE_SIZE	// This is the smallest chunk released by the buddy in bytes. PER_LP_PREALLOCATED_MEMORY/BUDDY_GRANULARITY must be integer and a power of 2


extern bool allocator_init(void);
extern void allocator_fini(void);
extern void segment_init(void);
extern struct segment *get_segment(GID_t i);
extern void *get_base_pointer(GID_t gid);

extern void initialize_memory_map(struct lp_struct *lp);
extern void finalize_memory_map(struct lp_struct *lp);

extern struct buddy *buddy_new(struct lp_struct *, unsigned long num_of_fragments);
void buddy_destroy(struct buddy *);
