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
* @file atomic.h
* @brief This module implements atomic and non-blocking operations used within ROOT-Sim
*        to coordinate threads and processes (on shared memory)
* @author Alessandro Pellegrini
* @date Jan 25, 2012
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#if (!defined(NDEBUG)) && defined(HAVE_HELGRIND_H)
#include <valgrind/helgrind.h>
#endif

//#define SPINLOCK_GIVES_COUNT

/// Atomic counter definition
typedef struct {
	volatile int count;
} atomic_t;

/// Spinlock definition
typedef struct {
	volatile unsigned int lock;
} spinlock_t;

inline bool CAS(volatile uint64_t * ptr, uint64_t oldVal, uint64_t newVal);
inline bool iCAS(volatile uint32_t * ptr, uint32_t oldVal, uint32_t newVal);
inline int atomic_test_and_set(int *);
inline int atomic_test_and_reset(int *);
inline void atomic_add(atomic_t *, int);
inline void atomic_sub(atomic_t *, int);
inline void atomic_inc(atomic_t *);
inline void atomic_dec(atomic_t *);
inline int atomic_inc_and_test(atomic_t * v);
inline bool spin_trylock(spinlock_t * s);
inline void spin_unlock(spinlock_t * s);

#ifdef SPINLOCK_GIVES_COUNT
inline unsigned int spin_lock(spinlock_t * s);
#else
inline void spin_lock(spinlock_t * s);
#endif

#define LOCK "lock; "

/// Read operation on an atomic counter
#define atomic_read(v)		((v)->count)

/// Set operation on an atomic counter
#define atomic_set(v,i)		(((v)->count) = (i))

/// Spinlock initialization
#define plain_spinlock_init(s)	((s)->lock = 0)

/* #if (!defined(NDEBUG)) && defined(HAVE_HELGRIND_H) */
/* #define spinlock_init(s)	(plain_spinlock_init(s); ANNOTATE_RWLOCK_CREATE(&((s)->lock))) */
/* #else */
#define spinlock_init(s)	plain_spinlock_init(s)
/* #endif */
