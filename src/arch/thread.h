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
#ifndef __ROOTSIM_THREAD_H
#define __ROOTSIM_THREAD_H

#include <stdbool.h>
#include <arch/atomic.h>
#include <arch/os.h>
#include <datatypes/msgchannel.h>




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

enum thread_incarnation {
	THREAD_SYMMETRIC,
	THREAD_CONTROLLER,
	THREAD_PROCESSING
};

/// Thread State Control Block
typedef struct _Thread_State {
	/// This member tells what incar
	enum thread_incarnation	incarnation;

	// TODO: remove local_tid everywhere and use only tid. The global tid (if ever required)
	// can be retrieved from here!
	/// Global tid, used to identify a thread within the whole system
	unsigned int		global_tid;

	/** Thread Input Port: if a thread is a Processing Thread, these are used to send messages to process.
	 * There are two input ports, which are associated with high and low priority messages to exchange. */
	msg_channel		*input_port[2];

	/// Thread Output Port: if a thread is a Processing Thread, this is used to send back generated events or control messages to the controller
	msg_channel		*output_port;
} Thread_State;


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
	atomic_t c1;
	atomic_t c2;
	atomic_t barr;
} barrier_t;

/// Reset operation on a thread barrier
#define thread_barrier_reset(b)		do { \
						(atomic_set((&b->c1), (b)->num_threads)); \
						(atomic_set((&b->c2), (b)->num_threads)); \
						(atomic_set((&b->barr), -1)); \
					} while (0)


extern void barrier_init(barrier_t *b, int t);
extern bool thread_barrier(barrier_t *b);
bool reserve_barrier(barrier_t *b);
void release_barrier(barrier_t *b);



/// Barrier for all worker threads
extern barrier_t all_thread_barrier;


#endif /* __ROOTSIM_THREAD_H */


