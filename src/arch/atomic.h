/**
 * @file arch/atomic.h
 *
 * @brief Atomic operations
 *
 * This module implements atomic and non-blocking operations used
 * within ROOT-Sim to coordinate threads and processes (on shared memory).
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
 *
 * @date Jan 25, 2012
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Atomic counter definition. This is a structure keeping a volatile int
 * inside. The structure and the volatile part is required to avoid
 * that the compiler wrongly optimizes some accesses to the count member.
 */
typedef struct {
	volatile int count; ///< Atomic counter. Use the provided API to ensure atomicity.
} atomic_t;

/**
 * Spinlock definition. This is a structure keeping a volatile unsigned int
 * inside. The structure and the volatile part is required to avoid
 * that the compiler wrongly optimizes some accesses to the count member.
 */
typedef struct {
	volatile unsigned int lock; ///< The lock guard.
} spinlock_t;


inline bool iCAS(volatile uint32_t * ptr, uint32_t oldVal, uint32_t newVal);
inline int atomic_test_and_set(int *);
inline void atomic_inc(atomic_t *);
inline void atomic_dec(atomic_t *);
inline int atomic_inc_and_test(atomic_t * v);
inline bool spin_trylock(spinlock_t * s);
inline void spin_unlock(spinlock_t * s);
inline void spin_lock(spinlock_t * s);

/// Read operation on an atomic counter
#define atomic_read(v)		((v)->count)

/// Set operation on an atomic counter
#define atomic_set(v,i)		(((v)->count) = (i))

/// Spinlock initialization
#define spinlock_init(s)	((s)->lock = 0)

