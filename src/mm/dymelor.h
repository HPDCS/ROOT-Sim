/**
* @file mm/dymelor.h
*
* @brief Dynamic Memory Logger and Restorer (DyMeLoR)
*
* LP's memory manager.
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
* @author Roberto Toccaceli
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @author Francesco Quaglia
*
* @date April 02, 2008
*/

#pragma once

#include <math.h>
#include <string.h>

#include <core/core.h>
#include <datatypes/bitmap.h>
#include <mm/state.h>
#include <core/timer.h>

/**************************************
 * DyMeLoR definitions and structures *
 **************************************/

#define MIN_CHUNK_SIZE 128U	// Size (in bytes) of the smallest chunk provideable by DyMeLoR
#define MAX_CHUNK_SIZE 4194304U	// Size (in bytes) of the biggest one. Notice that if this number
				// is too large, performance (and memory usage) might be affected.
				// If it is too small, large amount of memory requests by the
				// application level software (i.e, larger than this number)
				// will fail, as DyMeLoR will not be able to handle them!

#define NUM_AREAS (B_CTZ(MAX_CHUNK_SIZE) - B_CTZ(MIN_CHUNK_SIZE) + 1)	// Number of initial malloc_areas available (will be increased at runtime if needed)
#define MAX_NUM_AREAS (NUM_AREAS * 32)	// Maximum number of allocatable malloc_areas. If MAX_NUM_AREAS
				// malloc_areas are filled at runtime, subsequent malloc() requests
				// by the application level software will fail.
#define MAX_LIMIT_NUM_AREAS MAX_NUM_AREAS
#define MIN_NUM_CHUNKS 512	// Minimum number of chunks per malloc_area

#define MAX_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is underused TODO: retest
#define MIN_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is overused TODO: retest

#ifndef INCREMENTAL_GRANULARITY
#define INCREMENTAL_GRANULARITY 50	// Number of incremental logs before a full log is forced
#endif

// Macros used to check, set and unset special purpose bits
#define SET_LOG_MODE_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size |=  (1UL << 0))
#define RESET_LOG_MODE_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &= ~(1UL << 0))
#define CHECK_LOG_MODE_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &   (1UL << 0))

#define SET_AREA_LOCK_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size |=  (1UL << 1))
#define RESET_AREA_LOCK_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &= ~(1UL << 1))
#define CHECK_AREA_LOCK_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &   (1UL << 1))

#define UNTAGGED_CHUNK_SIZE(m_area)	(((malloc_area*)(m_area))->chunk_size & ~((1UL << 0) | (1UL << 1)))

#define POWEROF2(x) (1UL << (1 + (63 - __builtin_clzl((x) - 1))))
#define IS_POWEROF2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

#define PER_LP_PREALLOCATED_MEMORY (262144L * PAGE_SIZE)	// This should be power of 2 multiplied by a page size. This is 1GB per LP.

/// This structure let DyMeLoR handle one malloc area (for serving given-size memory requests)
struct _malloc_area {
#ifndef NDEBUG
	atomic_t presence;
#endif
	size_t chunk_size;
	int alloc_chunks;
	int dirty_chunks;
	int next_chunk;
	int num_chunks;
	int idx;
	int state_changed;
	simtime_t last_access;
	struct _malloc_area **self_pointer;	// This pointer is used in a free operation. Each chunk points here. If malloc_area is moved, only this is updated.
	rootsim_bitmap *use_bitmap;
	rootsim_bitmap *dirty_bitmap;
#ifdef HAVE_APPROXIMATED_ROLLBACK
	rootsim_bitmap *coredata_bitmap;
#endif
	void *area;
	int prev;
	int next;
};

typedef struct _malloc_area malloc_area;

/// Definition of the memory map
struct _malloc_state {
	bool is_incremental;			///< Tells if it is an incremental log or a full one (when used for logging)
#ifdef HAVE_APPROXIMATED_ROLLBACK
	bool is_approximated;			///< Tells if it is an approximate log or a precise one (when used for logging)
	bool want_approximated;			///< Tells if we want the next logs to be approximated or not
	size_t approximated_log_size; 	///< The difference in size between a full log and an approximated one
#endif
	size_t total_log_size;
	size_t total_inc_size;
	int num_areas;
	int max_num_areas;
	simtime_t timestamp;
	struct _malloc_area *areas;
};

typedef struct _malloc_state malloc_state;

#define is_incremental(ckpt) (((malloc_state *)ckpt)->is_incremental == true)

#define get_top_malloc_area(ptr) **((malloc_area ***)ptr - 1)

#define PER_LP_PREALLOCATED_MEMORY (262144L * PAGE_SIZE)	// This should be power of 2 multiplied by a page size. This is 1GB per LP.
#define BUDDY_GRANULARITY PAGE_SIZE	// This is the smallest chunk released by the buddy in bytes. PER_LP_PREALLOCATED_MEMORY/BUDDY_GRANULARITY must be integer and a power of 2


struct segment {
	unsigned char *base;
	unsigned char *brk;
};

extern size_t __page_size;
#define PAGE_SIZE ({ \
			if(unlikely(__page_size == 0))\
				__page_size = getpagesize();\
			__page_size;\
		  })

struct slab_header {
#ifndef NDEBUG
	atomic_t presence;
#endif
	struct slab_header *prev, *next;
	uint64_t slots;
	uintptr_t refcount;
	struct slab_header *page;
	uint8_t data[] __attribute__((aligned(sizeof(void *))));
};

struct slab_chain {
	spinlock_t lock;
	size_t itemsize, itemcount;
	size_t slabsize, pages_per_alloc;
	uint64_t initial_slotmask, empty_slotmask;
	uintptr_t alignment_mask;
	struct slab_header *partial, *empty, *full;
};


/***************
 * EXPOSED API *
 ***************/

// DyMeLoR API
extern void set_force_full(unsigned int, int);
extern void dirty_mem(void *, int);
extern size_t get_state_size(int);
extern size_t get_log_size(malloc_state *);
extern size_t get_inc_log_size(void *);
extern int get_granularity(void);
extern size_t dirty_size(unsigned int, void *, double *);
extern malloc_state *malloc_state_init(void);
extern void *do_malloc(struct lp_struct *, size_t);
extern void do_free(struct lp_struct *, void *ptr);
extern void *allocate_lp_memory(struct lp_struct *, size_t);
extern void free_lp_memory(struct lp_struct *, void *);
extern malloc_area* malloc_area_get (void *address, int *chunk_ret);

// Userspace API
extern void *__wrap_malloc(size_t);
extern void __wrap_free(void *);
extern void *__wrap_realloc(void *, size_t);
extern void *__wrap_calloc(size_t, size_t);


/***************************
 * BUDDY SYSTEM
 ***************************/

// buddy block size expressed in 2^n, e.g.: BUDDY_BLOCK_SIZE_EXP = 4, block_size = 16 TODO make this the pagesize
#define BUDDY_BLOCK_SIZE_EXP 12

struct buddy {
	spinlock_t lock;
	size_t size;
	size_t longest[] __attribute__((aligned(sizeof(size_t)))); // an array based binary tree
};

extern struct buddy *buddy_new(size_t requested_size);
extern void buddy_destroy(struct buddy *self);
extern void *allocate_buddy_memory(struct buddy *self, void *base_mem, size_t requested_size);
extern void free_buddy_memory(struct buddy *self, void *base_mem, void *ptr);


// This is used to help ensure that the platform is not using malloc.
#pragma GCC poison malloc free realloc calloc
