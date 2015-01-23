/**
*			Copyright (C) 2008-2013 HPDC Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
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

/// This macro expands to true if the current KLT is the master thread for the local kernel
#define master_thread() ((tid & ((1 << sizeof(unsigned int) * 8 / 2)-1)) == 0)


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


#endif /* __ROOTSIM_THREAD_H */


