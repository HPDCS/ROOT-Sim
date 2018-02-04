/**
*                       Copyright (C) 2008-2018 HPDCS Group
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
#include <core/init.h>
#include <mm/dymelor.h>
#include <scheduler/scheduler.h>

static tid_t os_tid;

__thread unsigned int tid;

static unsigned int thread_counter = 0;

/// Thread Control Blocks
Thread_State **Threads;

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

	// Uniquely pick a TCB across the ones which have been previously created
	unsigned int old_counter;
	unsigned int _local_tid;

	while(true) {
		old_counter = thread_counter;
		_local_tid = old_counter + 1;
		if(iCAS(&thread_counter, old_counter, _local_tid)) {
			break;
		}
	}

	// I'm the _local_tid-th thread
	tid = Threads[_local_tid]->tid;

	// Set the affinity on a CPU core, for increased performance
	if(rootsim_config.core_binding)
		set_affinity(tid);

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



void threads_init(void) {
	unsigned int i;
	unsigned int curr_ct = 0;

	// Check if we have enough threads to run an asymmetric simulation
	if(rootsim_config.num_controllers > n_cores / 2) {
		fprintf(stderr, "Running with %d threads, asked for %d controllers: there won't be enough PTs!\n", n_cores, rootsim_config.num_controllers);
		exit(EXIT_FAILURE);
	}

	// Initialize Thread Control Blocks
	Threads = rsalloc(sizeof(Thread_State *) * n_cores);
	bzero(Threads, sizeof(Thread_State *) * n_cores);

	for(i = 0; i < n_cores; i++) {
		Threads[i] = rsalloc(sizeof(Thread_State));
		bzero(Threads[i], sizeof(Thread_State));

		// TODO: we should find a new way to orchestrate symmetric, controlling, and processing threads altogether!
		if(rootsim_config.num_controllers == 0)
			Threads[i]->incarnation = THREAD_SYMMETRIC;
		else if(i < rootsim_config.num_controllers)
			Threads[i]->incarnation = THREAD_CONTROLLER;
		else
			Threads[i]->incarnation = THREAD_PROCESSING;

		// Initialize thread ports
		Threads[i]->input_port[PORT_PRIO_HI] = init_channel();
		Threads[i]->input_port[PORT_PRIO_LO] = init_channel();
		Threads[i]->output_port = init_channel();
		Threads[i]->port_batch_size = PORT_START_BATCH_SIZE;

		// Initialize curr_scheduled_events
		Threads[i]->curr_scheduled_events = rsalloc(sizeof(int)*n_prc);

		// Initialize the pointer of possible PTs for this thread
		Threads[i]->num_PTs = 0;
		if(n_cores - rootsim_config.num_controllers > 0) {
			Threads[i]->PTs = rsalloc(sizeof(Thread_State *) * (n_cores - rootsim_config.num_controllers));
		} else {
			Threads[i]->PTs = NULL;
		}

		// This TCB is associated with a certain tid and global_tid.
		// The actual thread which will take this tid depends on the race
		// threads make upon their creation.
		Threads[i]->tid = i;
		Threads[i]->global_tid = to_global_tid(kid, i);

	}

	// In this second run, we mutually assing PTs to CTs and vice versa (we loop over PTs)
	if(rootsim_config.num_controllers > 0) {
		for(i = rootsim_config.num_controllers; i < n_cores; i++) {
			Threads[i]->CT = Threads[curr_ct];
			Threads[curr_ct]->PTs[Threads[curr_ct]->num_PTs] = Threads[i];

			printf("PT %d sees as its CT: %d\n", Threads[i]->tid, Threads[i]->CT->tid);
			printf("CT %d has got a new PT: %d\n", Threads[curr_ct]->tid, Threads[curr_ct]->PTs[Threads[curr_ct]->num_PTs]->tid); 

			Threads[curr_ct]->num_PTs++;
			curr_ct = (curr_ct + 1) % rootsim_config.num_controllers;
		}
	}
}

