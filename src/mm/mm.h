/**
* @file mm/mm.h
*
* @brief Memory Manager main header
*
* Memory Manager main header
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
*/

#pragma once

#include <sys/mman.h>
#include <pthread.h>
#include <core/core.h>
#include <arch/atomic.h>
#include <mm/state.h>

struct malloc_state;
struct lp_struct;

extern bool allocator_init(void);
extern void allocator_fini(void);
extern void segment_init(void);
extern struct segment *get_segment(GID_t i);
extern void *get_base_pointer(GID_t gid);

extern void initialize_memory_map(struct lp_struct *lp);
extern void finalize_memory_map(struct lp_struct *lp);

extern struct buddy *buddy_new(struct lp_struct *,
			       unsigned long num_of_fragments);
void buddy_destroy(struct buddy *);

extern struct slab_chain *slab_init(const size_t itemsize);
extern void *slab_alloc(struct lp_struct *);
extern void slab_free(struct lp_struct *, const void *const addr);

/* Simulation Platform Memory APIs */
extern inline void *rsalloc(size_t);
extern inline void *rszalloc(size_t size);
extern inline void rsfree(void *);
extern inline void *rsrealloc(void *, size_t);
extern inline void *rscalloc(size_t, size_t);

// Checkpointing API
extern void *log_full(struct lp_struct *);
extern void *log_state(struct lp_struct *);
extern void log_restore(struct lp_struct *, state_t *);
extern void log_delete(void *);

// DyMeLoR Other API
extern void set_force_full(struct lp_struct *lp);
extern void clean_buffers_on_gvt(struct lp_struct *, simtime_t);
extern size_t get_state_size(int);
extern size_t get_log_size(struct malloc_state *);
extern size_t get_inc_log_size(void *);
extern int get_granularity(void);
extern size_t dirty_size(unsigned int, void *, double *);
extern struct malloc_state *malloc_state_init(void);
extern void malloc_state_wipe(struct malloc_state **);
extern void *do_malloc(struct lp_struct *, size_t);
extern void do_free(struct lp_struct *, void *ptr);
extern void *allocate_lp_memory(struct lp_struct *, size_t);
extern void free_lp_memory(struct lp_struct *, void *);
extern void ecs_init(void);
extern bool is_incremental(void *ckpt);
