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

//ADDED BY MAT 0x00000200000000000
#define LP_PREALLOCATION_INITIAL_ADDRESS	(void *)0x0000008000000000
#define MASK 0x00000001		// Mask used to check, set and unset bits

#define MIN_CHUNK_SIZE 128	// Size (in bytes) of the smallest chunk provideable by DyMeLoR
#define MAX_CHUNK_SIZE 4194304	// Size (in bytes) of the biggest one. Notice that if this number
				// is too large, performance (and memory usage) might be affected.
				// If it is too small, large amount of memory requests by the
				// application level software (i.e, larger than this number)
				// will fail, as DyMeLoR will not be able to handle them!

#define NUM_AREAS (log2(MAX_CHUNK_SIZE) - log2(MIN_CHUNK_SIZE) + 1)	// Number of initial malloc_areas available (will be increased at runtime if needed)
#define MAX_NUM_AREAS (NUM_AREAS * 32)	// Maximum number of allocatable malloc_areas. If MAX_NUM_AREAS
				// malloc_areas are filled at runtime, subsequent malloc() requests
				// by the application level software will fail.
#define MAX_LIMIT_NUM_AREAS MAX_NUM_AREAS
#define MIN_NUM_CHUNKS 512	// Minimum number of chunks per malloc_area
#define MAX_NUM_CHUNKS 4096	// Maximum number of chunks per malloc_area

#define MAX_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is underused TODO: retest
#define MIN_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is overused TODO: retest

#ifndef INCREMENTAL_GRANULARITY
#define INCREMENTAL_GRANULARITY 50	// Number of incremental logs before a full log is forced
#endif

// These macros are used to tune the statistical malloc_area diff
#define LITTLE_SIZE		32
#define CHECK_SIZE		0.25	// Must be <= 0.25!

// This macro is used to retrieve a cache line in O(1)
#define GET_CACHE_LINE_NUMBER(P) ((unsigned long)((P >> 4) & (CACHE_SIZE - 1)))

// Macros uset to check, set and unset special purpose bits
#define SET_LOG_MODE_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size |=  (MASK << 0))
#define RESET_LOG_MODE_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &= ~(MASK << 0))
#define CHECK_LOG_MODE_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &   (MASK << 0))

#define SET_AREA_LOCK_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size |=  (MASK << 1))
#define RESET_AREA_LOCK_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &= ~(MASK << 1))
#define CHECK_AREA_LOCK_BIT(m_area)	(((malloc_area*)(m_area))->chunk_size &   (MASK << 1))

#define UNTAGGED_CHUNK_SIZE(m_area)	(((malloc_area*)(m_area))->chunk_size & ~((MASK << 0) | (MASK << 1)))

#define POWEROF2(x) (1UL << (1 + (63 - __builtin_clzl((x) - 1))))
#define IS_POWEROF2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

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
	struct _malloc_area *self_pointer;	// This pointer is used in a free operation. Each chunk points here. If malloc_area is moved, only this is updated.
	rootsim_bitmap *use_bitmap;
	rootsim_bitmap *dirty_bitmap;
	void *area;
	int prev;
	int next;
};

typedef struct _malloc_area malloc_area;

/// Definition of the memory map
struct _malloc_state {
	bool is_incremental;	///< Tells if it is an incremental log or a full one (when used for logging)
	size_t total_log_size;
	size_t total_inc_size;
	size_t bitmap_size;
	size_t dirty_bitmap_size;
	int num_areas;
	int max_num_areas;
	int busy_areas;
	int dirty_areas;
	simtime_t timestamp;
	struct _malloc_area *areas;
};

typedef struct _malloc_state malloc_state;

#define is_incremental(ckpt) (((malloc_state *)ckpt)->is_incremental == true)

#define get_top_pointer(ptr) ((unsigned long long *)((char *)ptr - sizeof(unsigned long long)))
#define get_area_top_pointer(ptr) ( (malloc_area **)(*get_top_pointer(ptr)) )
#define get_area(ptr) ( *(get_area_top_pointer(ptr)) )

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
extern void malloc_state_wipe(malloc_state **);
extern void *do_malloc(struct lp_struct *, size_t);
extern void do_free(struct lp_struct *, void *ptr);
extern void *allocate_lp_memory(struct lp_struct *, size_t);
extern void free_lp_memory(struct lp_struct *, void *);

// Checkpointing API
extern void *log_full(struct lp_struct *);
extern void *log_state(struct lp_struct *);
extern void log_restore(struct lp_struct *, state_t *);
extern void log_delete(void *);

// Userspace API
extern void *__wrap_malloc(size_t);
extern void __wrap_free(void *);
extern void *__wrap_realloc(void *, size_t);
extern void *__wrap_calloc(size_t, size_t);
extern void clean_buffers_on_gvt(struct lp_struct *, simtime_t);

/* Simulation Platform Memory APIs */
extern inline void *rsalloc(size_t);
extern inline void rsfree(void *);
extern inline void *rsrealloc(void *, size_t);
extern inline void *rscalloc(size_t, size_t);

extern void ecs_init(void);

// This is used to help ensure that the platform is not using malloc.
#pragma GCC poison malloc free realloc calloc
