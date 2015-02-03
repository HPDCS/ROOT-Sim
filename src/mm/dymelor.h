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
* @file dymelor.h
* @brief This is the Memory Management Subsystem main header file
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date April 02, 2008
*
*/


#pragma once
#ifndef _DYMELOR_H
#define _DYMELOR_H

#include <math.h>
#include <string.h>


#include <core/core.h>
#include <mm/state.h>
#include <core/timer.h>

// TODO: serve per la definizione di una macro che poi verrà passata in configure.ac
#include <mm/modules/ktblmgr/ktblmgr.h>




/**************************************
 * DyMeLoR definitions and structures *
 **************************************/



#define MASK 0x00000001		// Mask used to check, set and unset bits


#define NUM_CHUNKS_PER_BLOCK 32
#define BLOCK_SIZE sizeof(unsigned int)


#define CACHE_SIZE 32768	// Must be a power of 2!
#define MIN_CHUNK_SIZE 32	// Size (in bytes) of the smallest chunk provideable by DyMeLoR
#define MAX_CHUNK_SIZE 1048576	// Size (in bytes) of the biggest one. Notice that if this number
				// is too large, performance (and memory usage) might be affected.
				// If it is too small, large amount of memory requests by the
				// application level software (i.e, larger than this number)
				// will fail, as DyMeLoR will not be able to handle them!

#define NUM_AREAS (log2(MAX_CHUNK_SIZE) - log2(MIN_CHUNK_SIZE) + 1)			// Number of initial malloc_areas available (will be increased at runtime if needed)
#define MAX_NUM_AREAS (NUM_AREAS * 2) 	// Maximum number of allocatable malloc_areas. If MAX_NUM_AREAS
				// malloc_areas are filled at runtime, subsequent malloc() requests
				// by the application level software will fail.
#define MAX_LIMIT_NUM_AREAS 100
#define MIN_NUM_CHUNKS 64	// Minimum number of chunks per malloc_area
#define MAX_NUM_CHUNKS 1024	// Maximum number of chunks per malloc_area

#define MAX_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is underused TODO: retest
#define MIN_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is overused TODO: retest


#ifndef INCREMENTAL_GRANULARITY
 #define INCREMENTAL_GRANULARITY 50 // Number of incremental logs before a full log is forced
#endif

// These macros are used to tune the statistical malloc_area diff
#define LITTLE_SIZE		32
#define CHECK_SIZE		0.25  // Must be <= 0.25!


// This macro is used to retrieve a cache line in O(1)
#define GET_CACHE_LINE_NUMBER(P) ((unsigned long)((P >> 4) & (CACHE_SIZE - 1)))

// Macros to check, set and unset bits in the malloc_area masks
#define CHECK_USE_BIT(A,I) ( CHECK_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->use_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],	\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define SET_USE_BIT(A,I) ( SET_BIT_AT(										\
			((unsigned int*)(((malloc_area*)A)->use_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],	\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define RESET_USE_BIT(A,I) ( RESET_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->use_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],	\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )

#define CHECK_DIRTY_BIT(A,I) ( CHECK_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->dirty_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define SET_DIRTY_BIT(A,I) ( SET_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->dirty_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define RESET_DIRTY_BIT(A,I) ( RESET_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->dirty_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )


// Macros uset to check, set and unset special purpose bits
#define SET_LOG_MODE_BIT(A)     ( SET_BIT_AT(((malloc_area*)A)->chunk_size, 0) )
#define RESET_LOG_MODE_BIT(A) ( RESET_BIT_AT(((malloc_area*)A)->chunk_size, 0) )
#define CHECK_LOG_MODE_BIT(A) ( CHECK_BIT_AT(((malloc_area*)A)->chunk_size, 0) )

#define SET_AREA_LOCK_BIT(A)     ( SET_BIT_AT(((malloc_area*)A)->chunk_size, 1) )
#define RESET_AREA_LOCK_BIT(A) ( RESET_BIT_AT(((malloc_area*)A)->chunk_size, 1) )
#define CHECK_AREA_LOCK_BIT(A) ( CHECK_BIT_AT(((malloc_area*)A)->chunk_size, 1) )

#define SET_BIT_AT(B,K) ( B |= (MASK << K) )
#define RESET_BIT_AT(B,K) ( B &= ~(MASK << K) )
#define CHECK_BIT_AT(B,K) ( B & (MASK << K) )

// Macros to force DyMeLoR to adopt special behaviours (i.e, different from the normal policies) when logging the state
#define NO_FORCE_FULL	0
#define FORCE_FULL_NEXT	1
#define FORCE_FULL	2

//#define DIRTY_APPROXIMATION_STATS


/** This defines a cache line used to perform inverse query lookup, in order to determine which area
*   belongs to (used for free operations)
*/
struct _cache_line {
	void *chunk_init_address;
	void *chunk_final_address;
	unsigned int malloc_area_idx;
	unsigned int lid;
	int valid_era;
};

typedef struct _cache_line cache_line;



/// This structure let DyMeLoR handle one malloc area (for serving given-size memory requests)
struct _malloc_area {
	size_t chunk_size;
	int alloc_chunks;
	int dirty_chunks;
	int next_chunk;
	int num_chunks;
	int idx;
	int state_changed;
	simtime_t last_access;
	unsigned int *use_bitmap;
	unsigned int *dirty_bitmap;
	void *area;
	int prev;
	int next;
};

typedef struct _malloc_area malloc_area;


/// Definition of the memory map
struct _malloc_state {
	int is_incremental;		/// Tells if it is an incremental log or a full one (when used for logging)
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






/*****************************************************
 * LP memory preallocator definitions and structures *
 *****************************************************/


/** This macro describes how much memory is pre-allocated for each LP.
  * This sets each LP's maximum available memory (minus metadata).
  * Changing this is non-trivial, as Linux has a limit on the size of an mmap call,
  * so this must be reflected in the number of mmap calls in lp-alloc.c!
  */
#define PER_LP_PREALLOCATED_MEMORY	512*512*4096 // Allow 1 GB of virtual space per LP
//#define PER_LP_PREALLOCATED_MEMORY	1024*1024*128


/// This macro tells the LP memory preallocator where to start preallocating. This must be a PDP entry-aligned value!
#define LP_PREALLOCATION_INITIAL_ADDRESS	(void *)0x0000008000000000


/// This structure describes per-LP memory
struct _lp_memory {
	void *start;
	void *brk;
};








/***************
 * EXPOSED API *
 ***************/


// Variables
// TODO: quali sono realmente da esporre all'esterno?!

extern int incremental_granularity;
extern int force_full[MAX_LPs];
extern malloc_state *m_state[MAX_LPs];
extern double checkpoint_cost_per_byte;
extern double recovery_cost_per_byte;
extern unsigned long total_checkpoints;
extern unsigned long total_recoveries;
extern double checkpoint_bytes_total;



// DyMeLoR API
extern void dymelor_init(void);
extern void dymelor_fini(void);
extern void set_force_full(unsigned int, int);
extern void dirty_mem(void *, int);
extern size_t get_state_size(int);
extern size_t get_log_size(void *);
extern size_t get_inc_log_size(void *);
extern void *__wrap_malloc(size_t);
extern void __wrap_free(void *);
extern void *__wrap_realloc(void *, size_t);
extern void *__wrap_calloc(size_t, size_t);
extern void reset_state(void);
extern bool is_incremental(void *);
extern int get_granularity(void);
extern size_t dirty_size(unsigned int, void *, double *);
extern void clean_buffers_on_gvt(unsigned int, simtime_t);

// Checkpointing API
extern void *log_full(int);
extern void *log_state(int);
extern void log_restore(int, state_t *);
extern void log_delete(void *);



// LP memory preallocation API
extern void *lp_malloc_unscheduled(unsigned int, size_t);
#define lp_malloc(size) lp_malloc_unscheduled(current_lp, size);
extern void lp_free(void *);
extern void *lp_realloc(void *, size_t);
extern void lp_alloc_init(void);
extern void lp_alloc_fini(void);


#ifdef HAVE_LINUX_KERNEL_MAP_MODULE

extern void lp_alloc_thread_init(void);
extern void lp_alloc_schedule(void);
extern void lp_alloc_deschedule(void);
extern void lp_alloc_thread_fini(void);
#define clear_inter_lp_dependencies(lid) (LPS[lid]->ECS_index = 0)

#else

#define lp_alloc_thread_init()	{}
#define lp_alloc_schedule()		{}
#define lp_alloc_deschedule()	{}
#define lp_alloc_thread_fini()	{}


#endif /* HAVE_LINUX_KERNEL_MAP_MODULE */



#endif
