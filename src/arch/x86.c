/**
 * @file arch/x86.c
 *
 * @brief x86 synchronization primitives
 *
 * This module implements atomic and non-blocking operations used within
 * ROOT-Sim to coordinate threads and processes (on shared memory)
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

// Do not compile anything here if we're not on an x86 machine!

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <arch/atomic.h>
#include <mm/dymelor.h>

/**
* This function implements a compare-and-swap atomic operation on x86-64 for integers
*
* @param ptr the address where to perform the CAS operation on
* @param oldVal the old value we expect to find before swapping
* @param newVal the new value to place in ptr if ptr contains oldVal
*
* @return true if the CAS succeeded, false otherwise
*/
inline bool iCAS(volatile uint32_t *ptr, uint32_t oldVal, uint32_t newVal)
{
	unsigned long res = 0;

	__asm__ __volatile__("lock cmpxchgl %1, %2;"	// ZF = 1 if succeeded
			     "lahf;"			// to get the correct result even if oldVal == 0
			     "bt $14, %%ax;"		// is ZF set? (ZF is the 6'th bit in %ah, so it's the 14'th in ax)
			     "adc %0, %0"		// get the result
			     :"=r"(res)
			     :"r"(newVal), "m"(*ptr), "a"(oldVal), "0"(res)
			     :"memory");

	return (bool)res;
}

/**
* This function implements the atomic_test_and_set on an integer value, for x86-64 archs
*
* @param b the counter there to perform the operation
*
* @return true if the int value has been set, false otherwise
*/
inline int atomic_test_and_set(int *b)
{
	int result = 0;

	__asm__ __volatile__("lock bts $0, %1;\n\t"
			     "adc %0, %0"
			     :"=r"(result)
			     :"m"(*b), "0"(result)
			     :"memory");

	return !result;
}

/**
* This function implements (on x86-64 architectures) the atomic inc operation.
* It increments the atomic counter 'v' by 1 unit
*
* @param v the atomic counter which is the destination of the operation
*/
inline void atomic_inc(atomic_t * v)
{
	__asm__ __volatile__("lock incl %0"
			     :"=m"(v->count)
			     :"m"(v->count));
}

/**
* This function implements (on x86-64 architectures) the atomic dec operation.
* It decrements the atomic counter 'v' by 1 unit
*
* @param v the atomic counter which is the destination of the operation
*/
inline void atomic_dec(atomic_t * v)
{
	__asm__ __volatile__("lock decl %0"
			     :"=m"(v->count)
			     :"m"(v->count));
}

/**
* This function implements (on x86-64 architectures) the atomic dec operation.
* It decrements the atomic counter 'v' by 1 unit
*
* @param v the atomic counter which is the destination of the operation
*
* @return true if the counter became zero
*/
inline int atomic_inc_and_test(atomic_t * v)
{
	unsigned char c = 0;

	__asm__ __volatile__("lock incl %0\n\t"
			     "sete %1"
			     :"=m"(v->count), "=qm"(c)
			     :"m"(v->count)
			     :"memory");
	return c != 0;
}

/**
* This function implements (on x86-64 architectures) a spinlock operation.
*
* @param s the spinlock on which to spin
*/
inline void spin_lock(spinlock_t * s)
{
	__asm__ __volatile__("1:\n\t" "movl $1,%%eax\n\t"
			     "lock xchgl %%eax, %0\n\t"
			     "testl %%eax, %%eax\n\t"
			     "jnz 1b"
			     :	/* no output */
			     :"m"(s->lock)
			     :"eax", "memory");
}

/**
* This function implements (on x86-64 architectures) a trylock operation.
*
* @param s the spinlock to try to acquire
*/
inline bool spin_trylock(spinlock_t * s)
{
	return atomic_test_and_set((int *)&s->lock);
}

/**
* This function implements (on x86-64 architectures) a spin unlock operation.
*
* @param s the spinlock to unlock
*/
inline void spin_unlock(spinlock_t * s)
{
	__asm__ __volatile__("mov $0, %%eax\n\t"
			     "lock xchgl %%eax, %0"
			     : /* no output */
			     :"m"(s->lock)
			     :"eax", "memory");
}
