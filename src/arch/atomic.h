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
* @file atomic.h
* @brief This module implements atomic and non-blocking operations used within ROOT-Sim
*        to coordinate threads and processes (on shared memory)
* @author Alessandro Pellegrini
* @date Jan 25, 2012
*/

#pragma once
#ifndef __ROOTSIM__ATOMIC_H
#define __ROOTSIM__ATOMIC_H

#include <stdbool.h>



#define SPINLOCK_GIVES_COUNT



#if defined(ARCH_X86) || defined(ARCH_X86_64)

/// Atomic counter definition
typedef struct { volatile int count; } atomic_t;

/// Spinlock definition
typedef struct { volatile unsigned int lock; } spinlock_t;



bool CAS_x86(volatile unsigned long long *ptr, unsigned long long oldVal, unsigned long long newVal);
bool iCAS_x86(volatile unsigned int *ptr, unsigned int oldVal, unsigned int newVal);
int atomic_test_and_set_x86(int *);
int atomic_test_and_reset_x86(int *);
void atomic_add_x86(atomic_t *, int);
void atomic_sub_x86(atomic_t *, int);
void atomic_inc_x86(atomic_t *);
void atomic_dec_x86(atomic_t *);
int atomic_inc_and_test_x86(atomic_t *v);
bool spin_trylock_x86(spinlock_t *s);
void spin_unlock_x86(spinlock_t *s);

#ifdef SPINLOCK_GIVES_COUNT
unsigned int spin_lock_x86(spinlock_t *s);
#else
void spin_lock_x86(spinlock_t *s);
#endif


#define CAS			CAS_x86
#define iCAS			iCAS_x86
#define atomic_test_and_set	atomic_test_and_set_x86
#define atomic_test_and_reset	atomic_test_and_reset_x86
#define atomic_add		atomic_add_x86
#define atomic_sub		atomic_sub_x86
#define atomic_dec		atomic_dec_x86
#define atomic_inc		atomic_inc_x86
#define atomic_inc_and_test	atomic_inc_and_test_x86
#define spin_lock		spin_lock_x86
#define spin_trylock		spin_trylock_x86
#define spin_unlock		spin_unlock_x86

#define LOCK "lock; "

#else
#error Currently supporting only x86/x86_64
#endif


/// Read operation on an atomic counter
#define atomic_read(v)		((v)->count)

/// Set operation on an atomic counter
#define atomic_set(v,i)		(((v)->count) = (i))

/// Spinlock initialization
#define spinlock_init(s)	((s)->lock = 0)




#endif /* __ROOTSIM_ATOMIC_H */


