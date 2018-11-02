/**
*                       Copyright (C) 2008-2018 HPDCS Group
*			 http://www.dis.uniroma1.it/~hpdcs
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
* @file thread.h
* @brief This module implements Kernel Level Thread supports
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


// Macros to get information about the hosting machine

#define get_cores() (sysconf( _SC_NPROCESSORS_ONLN ))



// How do we identify a thread?
typedef pthread_t tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	pthread_create(&os_tid, NULL, entry, arg)

static inline void set_affinity(int core) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);
	// 0 is the current thread
	sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

#elif defined(OS_WINDOWS)

#include <windows.h>

#define get_cores() ({\
			SYSTEM_INFO _sysinfo;\
			GetSystemInfo( &_sysinfo );\
			_sysinfo.dwNumberOfProcessors;\
			})

// How do we identify a thread?
typedef HANDLE tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	CreateThread(NULL, 0, entry, arg, 0, &os_tid)

#define set_affinity(core) SetThreadAffinityMask(GetCurrentThread(), 1<<core)

#else /* OS_LINUX || OS_WINDOWS */
#error Unsupported operating system
#endif



/* The global tid is obtained by concatenating of the `kid` and the `local_tid`
 * and is stored into an unsigned int. Since we are using half of the unsigned int
 * for each part we have that the total number of kernels and
 * the number of threads per kernel must be less then (2^HALF_UINT_BITS - 1)
 */
#define HALF_UINT_BITS (sizeof(unsigned int)*8/2)

#define MAX_KERNELS ((1 << HALF_UINT_BITS) - 1)
#define MAX_THREADS_PER_KERNEL ((1 << HALF_UINT_BITS) - 1)

#define to_global_tid(kid, local_tid) ( (kid << HALF_UINT_BITS) | local_tid )
#define to_local_tid(global_tid) ( global_tid & ((1 << HALF_UINT_BITS)-1) )


/// This macro expands to true if the current KLT is the master thread for the local kernel
#define master_thread() (local_tid == 0)


/// This macro tells on what core the current thread is running
#define running_core() (local_tid)


/// This structure is used to call the thread creation helper function
struct _helper_thread {
	void *(*start_routine)(void*);
	void *arg;
};


/// Macro to create one single thread
#define create_thread(entry, arg) (create_threads(1, entry, arg))

void create_threads(unsigned short int n, void *(*start_routine)(void*), void *arg);

extern __thread unsigned int tid;
extern __thread unsigned int local_tid;

/// Thread barrier definition
typedef struct {
	int num_threads;
	atomic_t pass;
	atomic_t barr;
} barrier_t;

/// Reset operation on a thread barrier
#define thread_barrier_reset(b)	atomic_set(&(b)->barr, 0)

/**
* Initialize a thread barrier. If more than the hereby specified
* number of threads try to synchronize on the barrier, the behaviour is undefined.
**/
#define barrier_init(B, T) do {\
				(B)->num_threads = T;\
				thread_barrier_reset(B);\
			   } while(0)


extern bool thread_barrier(barrier_t *b);

