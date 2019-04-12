/**
 * @file arch/thread.c
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

#include <stdbool.h>
#include <arch/thread.h>
#include <core/init.h>
#include <mm/mm.h>

/**
 * An OS-level thread id. We never do any join on worker threads, so
 * there is no need to keep track of system ids. Internally, each thread
 * is given a unique id in [0, n_thr], which is used to know who is who.
 * This variable is used only when spawning threads, because the system
 * wants us to know who is what thread from its point of view, although
 * we don't care at all.
 */
static tid_t os_tid;


/**
 * The id of the thread.
 *
 * @todo This is global across all kernel instances in a distributed
 *       run. We don't actually care at all about this, so this can be
 *       safely made per-machine. Once this is done, local_tid should
 *       go away.
 */
__thread unsigned int tid;


/**
 * The "local" id of the thread. This is a per-instance value which
 * uniquely identifies a thread on a compute node.
 *
 * @todo This is global across all kernel instances in a distributed
 *       run. We don't actually care at all about this, so this can be
 *       safely made per-machine. Once this is done, local_tid should
 *       go away.
 */
__thread unsigned int local_tid;


/**
 * The thread counter is a global variable which is incremented atomically
 * to assign a unique thread ID. Each spawned thread will compete using a
 * CAS to update this counter. Once a thread succeeds, it can use the
 * value it was keeping as its private local id.
 * This variable is used only as a synchronization point upon initialization,
 * later no one cares about its value.
 */
static unsigned int thread_counter = 0;


/**
* This helper function is the actual entry point for every thread created
* using the provided internal services. The goal of this function is to
* start a race on the thread_counter shared variable using a CAS. This
* race across all the threads is used to determine unique identifiers
* across all the worker threads. This is done in this helper function to
* silently set the new thread's tid before any simulation-specific function
* is ever called, so that any place in the simulator will find that
* already set.
* Additionally, when the created thread returns, it frees the memory used
* to maintain the real entry point and the pointer to its arguments.
*
* @param arg A pointer to an internally defined structure keeping the
*            real thread's entry point and its arguments
*
* @return This function always returns NULL
*/
static void *__helper_create_thread(void *arg)
{

	struct _helper_thread *real_arg = (struct _helper_thread *)arg;

	// Get a unique local thread id...
	unsigned int old_counter;
	unsigned int _local_tid;

	while (true) {
		old_counter = thread_counter;
		_local_tid = old_counter + 1;
		if (iCAS(&thread_counter, old_counter, _local_tid)) {
			break;
		}
	}
	local_tid = _local_tid;

	// ...and make it globally unique
	tid = to_global_tid(kid, _local_tid);

	// Set the affinity on a CPU core, for increased performance
	if (likely(rootsim_config.core_binding))
		set_affinity(local_tid);

	// Now get into the real thread's entry point
	real_arg->start_routine(real_arg->arg);

	// Free arg and return (we don't really need any return value)
	rsfree(arg);
	return NULL;
}

/**
* This function creates n threads, all having the same entry point and
* the same arguments.
* It creates a new thread starting from the __helper_create_thread
* function which silently sets the new thread's tid.
* Note that the arguments passed to __helper_create_thread are malloc'd
* here, and free'd there. This means that if start_routine does not
* return, there is a memory leak.
* Additionally, note that we don't make a copy of the arguments pointed
* by arg, so all the created threads will share them in memory. Changing
* passed arguments from one of the newly created threads will result in
* all the threads seeing the change.
*
* @param n The number of threads which should be created
* @param start_routine The new threads' entry point
* @param arg A pointer to an array of arguments to be passed to the new
*            threads' entry point
*
*/
void create_threads(unsigned short int n, void *(*start_routine)(void *), void *arg)
{
	int i;

	// We create our thread within our helper function, which accepts just
	// one parameter. We thus have to create one single parameter containing
	// the original pointer and the function to be used as real entry point.
	// This malloc'd array is free'd by the helper function.
	struct _helper_thread *new_arg = rsalloc(sizeof(struct _helper_thread));
	new_arg->start_routine = start_routine;
	new_arg->arg = arg;

	// n threads are created simply looping...
	for (i = 0; i < n; i++) {
		new_thread(__helper_create_thread, (void *)new_arg);
	}
}

/**
* This function initializes a thread barrier. If more than the hereby specified
* number of threads try to synchronize on the barrier, the behaviour is undefined.
*
* @param b the thread barrier to initialize
* @param t the number of threads which will synchronize on the barrier
*/
void barrier_init(barrier_t * b, int t)
{
	b->num_threads = t;
	thread_barrier_reset(b);
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
* @param b A pointer to the thread barrier to synchronize on
*
* @return false to all threads, except for one which is elected as the leader
*/
bool thread_barrier(barrier_t * b)
{
	// Wait for the leader to finish resetting the barrier
	while (atomic_read(&b->barr) != -1) ;

	// Wait for all threads to synchronize
	atomic_dec(&b->c1);
	while (atomic_read(&b->c1)) ;

	// Leader election
	if (unlikely(atomic_inc_and_test(&b->barr))) {

		// I'm sync'ed!
		atomic_dec(&b->c2);

		// Wait all the other threads to leave the first part of the barrier
		while (atomic_read(&b->c2)) ;

		// Reset the barrier to its initial values
		thread_barrier_reset(b);

		return true;
	}
	// I'm sync'ed!
	atomic_dec(&b->c2);

	return false;
}
