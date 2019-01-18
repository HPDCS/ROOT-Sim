/**
 * @file arch/thread.h
 *
 * @brief Generic thread management facilities.
 *
 * This module provides generic facilities for thread management.
 * In particular, helper functions to startup worker threads are exposed,
 * and a function to synchronize multiple threads on a software barrier.
 *
 * The software barrier also offers a leader election facility, so that
 * once all threads are synchronized on the barrier, the function returns
 * true to only one of them.
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
 * @date Jan 25, 2012
 */

#pragma once

#include <stdbool.h>
#include <arch/atomic.h>

#if defined(OS_LINUX)

#include <sched.h>
#include <unistd.h>
#include <pthread.h>

/// Macro to get the core count on the hosting machine
#define get_cores() (sysconf( _SC_NPROCESSORS_ONLN ))

/// How do we identify a thread?
typedef pthread_t tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	pthread_create(&os_tid, NULL, entry, arg)

/**
 * This inline function sets the affinity of the thread which calls it.
 *
 * @param core The core id on which the thread wants to be stuck on.
 */
static inline void set_affinity(int core)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);
	// 0 is the current thread
	sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

#elif defined(OS_WINDOWS)

#include <windows.h>

/// Macro to get the core count on the hosting machine
#define get_cores() ({\
			SYSTEM_INFO _sysinfo;\
			GetSystemInfo( &_sysinfo );\
			_sysinfo.dwNumberOfProcessors;\
			})

/// How do we identify a thread?
typedef HANDLE tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	CreateThread(NULL, 0, entry, arg, 0, &os_tid)

/// Macro to set the affinity of the thread which calls it
#define set_affinity(core) SetThreadAffinityMask(GetCurrentThread(), 1<<core)

#else /* OS_LINUX || OS_WINDOWS */
#error Unsupported operating system
#endif


/// This structure is used to call the thread creation helper function
struct _helper_thread {
	void *(*start_routine)(void *);	///< A pointer to the entry point of the next-to-be thread
	void *arg;			///< Arguments to be passed to @ref start_routine
};

/// Thread barrier definition
typedef struct {
	int num_threads;	///< Number of threads which will synchronize on the barrier
	atomic_t c1;		///< First synchronization counter
	atomic_t c2;		///< Second synchronization counter
	atomic_t barr;		/**< "Barrier in a barrier": this is used to wait for the leader
				 *   to correctly reset the barrier before re-entering
				 */
} barrier_t;

/**
 * The global tid is obtained by concatenating of the `kid` and the `local_tid`
 * and is stored into an unsigned int. Since we are using half of the unsigned int
 * for each part we have that the total number of kernels and
 * the number of threads per kernel must be less then (2^HALF_UINT_BITS - 1)
 */
#define HALF_UINT_BITS (sizeof(unsigned int) * 8 / 2)


/**
 * This macro tells what is the maximum number of simulation kernel instances
 * which are supported.
 * This has to do with the maximum representable number give the fact that
 * half of the available bits in a tid are used to keep track of the kid
 * on which that kernel resides.
 *
 * @todo This information is not actually needed, so we can drop this limitation
 */
#define MAX_KERNELS ((1 << HALF_UINT_BITS) - 1)


/**
 * This macro tells how many threads we can have on a single simulation
 * kernel. The logis is the same as that of @ref MAX_KERNELS, i.e. sharing
 * the available bit in an unsigned long to keep both representations.
 */
#define MAX_THREADS_PER_KERNEL ((1 << HALF_UINT_BITS) - 1)


/**
 * @brief Convert a local tid in a global tid
 *
 * This macro takes a local tid and inserts in the upper part of the bits
 * the id of the kernel on which that thread is running. This makes the
 * global tid.
 *
 * @param kid The kid on which a thread is running
 * @param local_tid The locally-assigned tid of the thread
 *
 * @return The global kid
 */
#define to_global_tid(kid, local_tid) ( (kid << HALF_UINT_BITS) | local_tid )

/// This macro expands to true if the current KLT is the master thread for the local kernel
#define master_thread() (local_tid == 0)

/**
 * @brief Reset operation on a thread barrier
 *
 * This macro can be used to initialize (or reset) a thread barrier.
 *
 * @warning Using this macro on a thread barrier on which some thread_barrier
 *          is already synchronizing leads to undefined behaviour.
 *
 * @param b The thread barrier to reset (the name, not a pointer to)
 */
#define thread_barrier_reset(b)		do { \
						(atomic_set((&b->c1), (b)->num_threads)); \
						(atomic_set((&b->c2), (b)->num_threads)); \
						(atomic_set((&b->barr), -1)); \
					} while (0)


extern __thread unsigned int tid;
extern __thread unsigned int local_tid;

extern void barrier_init(barrier_t * b, int t);
extern bool thread_barrier(barrier_t * b);
extern void create_threads(unsigned short int n, void *(*start_routine)(void *), void *arg);

