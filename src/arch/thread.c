/**
*                       Copyright (C) 2008-2017 HPDCS Group
*			 http://www.dis.uniroma1.it/~hpdcs
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
* @file barrier.c
* @brief This module implements a thread barrier
* @author Alessandro Pellegrini
* @date Jan 25, 2012
*/


#include <stdbool.h>
#include <arch/thread.h>
#include <core/core.h>
#include <mm/dymelor.h>

static tid_t os_tid;

__thread unsigned int tid;
__thread unsigned int local_tid;

static unsigned int thread_counter = 0;


/**
* This helper function is the actual entry point for every thread created using the provided internal
* services. The goal of this function is to silently set the new thread's tid so that any place in the
* simulator will find that already set.
* Additionally, when the created thread returns, it frees the memory used to maintain the real entry point
* and the pointer to its arguments.
*
* @author Alessandro Pellegrini
*
* @param arg A pointer to an internally defined structure keeping the real thread's entry point and its arguments
*
* @return This function always returns NULL
*
*/
static void *__helper_create_thread(void *arg) {

	struct _helper_thread *real_arg = (struct _helper_thread *)arg;

	// Get a unique local thread id...
	unsigned int old_counter;
	unsigned int _local_tid;

	while(true) {
		old_counter = thread_counter;
		_local_tid = old_counter + 1;
		if(iCAS(&thread_counter, old_counter, _local_tid)) {
			break;
		}
	}
	local_tid = _local_tid;
	// ...and make it globally unique
	tid = to_global_tid(kid, _local_tid);


	// Set the affinity on a CPU core, for increased performance
	if(rootsim_config.core_binding)
		set_affinity(local_tid);

	// Now get into the real thread's entry point
	real_arg->start_routine(real_arg->arg);

	// We don't really need any return value
	return NULL;
}





/**
* This function creates n threads, all having the same entry point and the same arguments.
* It creates a new thread starting from the __helper_create_thread function which silently
* sets the new thread's tid.
* Note that the arguments passed to __helper_create_thread are malloc'd here, and free'd there.
* This means that if start_routine does not return, there will be a memory leak.
* Additionally, note that arguments pointer by arg are not actually copied here, so all the
* created threads will share them in memory. Changing passed arguments from one of the newly
* created threads will result in all the threads seeing the change.
*
* @author Alessandro Pellegrini
*
* @param start_routine The new threads' entry point
* @param arg A pointer to an array of arguments to be passed to the new threads' entry point
*
*/
void create_threads(unsigned short int n, void *(*start_routine)(void*), void *arg) {

	int i;

	// We create our thread within our helper function, which accepts just
	// one parameter. We thus have to create one single parameter containing
	// the original pointer and the function to be used as real entry point.
	// This malloc'd array is free'd by the helper function.
	struct _helper_thread *new_arg = rsalloc(sizeof(struct _helper_thread));
	new_arg->start_routine = start_routine;
	new_arg->arg = arg;

	// n threads are created simply looping...
	for(i = 0; i < n; i++) {
		new_thread(__helper_create_thread, (void *)new_arg);
	}
}





/**
* This function initializes a thread barrier. If more than the hereby specified
* number of threads try to synchronize on the barrier, the behaviour is undefined.
*
* @author Alessandro Pellegrini
*
* @param b the thread barrier to initialize
* @param t the number of threads which will synchronize on the barrier
*/
void barrier_init(barrier_t *b, int t) {
	b->reserved = 0;
	b->num_threads = t;
	thread_barrier_reset(b);
}


bool reserve_barrier(barrier_t *b) {
	return atomic_test_and_set((int *)&b->reserved);
}


/**
* This function synchronizes all the threads. After a thread leaves this function,
* it is guaranteed that no other thread has (at least) not entered the function,
* therefore allowing to create a separation between the execution in portions of the code.
* If more threads than specified in the initialization of the barrier try to synchronize
* on it, the behaviour is undefined.
* The function additionally returns 'true' only to one of the calling threads, allowing
* the execution of portions of code in isolated mode after the barrier itself. This is
* like a leader election for free.
*
* @author Alessandro Pellegrini
*
* @param b the thread barrier to synchronize on
*
* @return true to only one of the threads which synchronized on the barrier
*/
bool thread_barrier(barrier_t *b) {

	// Wait for the leader to finish resetting the barrier
	while(atomic_read(&b->barr) != -1);

	// Wait for all threads to synchronize
	atomic_dec(&b->c1);
	while(atomic_read(&b->c1));

	// Leader election
	if(atomic_inc_and_test(&b->barr)) {

		// I'm sync'ed!
		atomic_dec(&b->c2);

		// Wait all the other threads to leave the first part of the barrier
		while(atomic_read(&b->c2));

		// Reset the barrier to its initial values
		thread_barrier_reset(b);

		return true;
	}

	// I'm sync'ed!
	atomic_dec(&b->c2);

	return false;
}



