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
#include <core/core.h>
#include <arch/atomic.h>
#include <arch/os.h>
#include <datatypes/msgchannel.h>




/* Global tid is obtained by concatenating `kid` and `tid`.
 * It is stored into an unsigned int. Since we are using half of the unsigned int
 * for each part we have that the total number of kernels and
 * the number of threads per kernel must be less then (2^HALF_UINT_BITS - 1)
 */
#define HALF_UINT_BITS (sizeof(unsigned int)*8/2)

#define MAX_KERNELS ((1 << HALF_UINT_BITS) - 1)
#define MAX_THREADS_PER_KERNEL ((1 << HALF_UINT_BITS) - 1)

#define to_global_tid(kid, local_tid) ( (kid << HALF_UINT_BITS) | local_tid )
#define to_local_tid(global_tid) ( global_tid & ((1 << HALF_UINT_BITS)-1) )

/// This macro expands to true if the current KLT is the master thread for the local kernel
#define master_thread() (tid == 0)

/// This macro tells on what core the current thread is running
#define running_core() (tid)

// This macro defins the default value of the batch size of each input port
#define PORT_START_BATCH_SIZE	5 

enum thread_incarnation {
	THREAD_SYMMETRIC,
	THREAD_CONTROLLER,
	THREAD_PROCESSING
};

/// Thread State Control Block
typedef struct _Thread_State {
	/// This member tells what incar
	enum thread_incarnation	incarnation;

	/// tid, used to identify a thread within a local machine
	unsigned int		tid;

	/// Global tid, used to identify a thread within the whole system
	unsigned int		global_tid;

	/** Thread Input Port: if a thread is a Processing Thread, these are used to send messages to process.
	 * There are two input ports, which are associated with high and low priority messages to exchange. */
	msg_channel		*input_port[2];

	/// Thread Output Port: if a thread is a Processing Thread, this is used to send back generated events or control messages to the controller
	msg_channel		*output_port;

	/// Number of PTs assigned to this controller. 0 if the thread isn't a controller.
	unsigned int		num_PTs;

	/// Processing Threads assigned to this controller.
	struct _Thread_State	**PTs;

	/// If the thread is a PT, this points to the corresponding CT
	struct _Thread_State	*CT;

	/// If PT, it defines the current batch size for the input port
	unsigned int port_batch_size; 

	/* Pointer to an array of chars used by controllers as a counter of the number
	of events scheduled for each LP during the execution of asym_schedule*/
	int *curr_scheduled_events;	

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
void threads_init(void);

extern msg_t *pt_get_lo_prio_msg(void);
extern msg_t *pt_get_hi_prio_msg(void);

// Macros to differentiate across different input ports
#define PORT_PRIO_HI	0
#define PORT_PRIO_LO	1

// Macros to retrieve messages from PT port
#define pt_get_lo_prio_msg() get_msg(Threads[tid]->input_port[PORT_PRIO_LO])
#define pt_get_hi_prio_msg() get_msg(Threads[tid]->input_port[PORT_PRIO_HI])
#define pt_get_out_msg(th_id) get_msg(Threads[(th_id)]->output_port)

// Macros to send messages to PT port
#define pt_put_lo_prio_msg(th_id, event) insert_msg(Threads[(th_id)]->input_port[PORT_PRIO_LO], (event))
#define pt_put_hi_prio_msg(th_id, event) insert_msg(Threads[(th_id)]->input_port[PORT_PRIO_HI], (event))
#define pt_put_out_msg(event) insert_msg(Threads[tid]->output_port, (event))

/// Barrier for all worker threads
extern barrier_t all_thread_barrier;
extern barrier_t controller_barrier;

extern Thread_State **Threads;


#endif /* __ROOTSIM_THREAD_H */


