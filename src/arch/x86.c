/**
*                       Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file x86.c
* @brief This module implements atomic and non-blocking operations used within ROOT-Sim
*        to coordinate threads and processes (on shared memory)
* @author Alessandro Pellegrini
* @date Jan 25, 2012
*/


// Do not compile anything here if we're not on an x86 machine!

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <arch/atomic.h>
#include <mm/dymelor.h>

/**
* This function implements a compare-and-swap atomic operation on x86 for long long values
*
* @author Alessandro Pellegrini
*
* @param ptr the address where to perform the CAS operation on
* @param oldVal the old value we expect to find before swapping
* @param newVal the new value to place in ptr if ptr contains oldVal
*
* @return true if the CAS succeeded, false otherwise
*/
inline bool CAS(volatile uint64_t *ptr, uint64_t oldVal, uint64_t newVal) {
	unsigned long res = 0;

	__asm__ __volatile__(
		LOCK "cmpxchgq %1, %2;"//ZF = 1 if succeeded
		"lahf;"  // to get the correct result even if oldVal == 0
		"bt $14, %%ax;" // is ZF set? (ZF is the 6'th bit in %ah, so it's the 14'th in ax)
		"adc %0, %0" // get the result
		: "=r"(res)
		: "r"(newVal), "m"(*ptr), "a"(oldVal), "0"(res)
		: "memory"
	);

	return (bool)res;
}


/**
* This function implements a compare-and-swap atomic operation on x86-64 for integers
*
* @author Alessandro Pellegrini
*
* @param ptr the address where to perform the CAS operation on
* @param oldVal the old value we expect to find before swapping
* @param newVal the new value to place in ptr if ptr contains oldVal
*
* @return true if the CAS succeeded, false otherwise
*/
inline bool iCAS(volatile uint32_t *ptr, uint32_t oldVal, uint32_t newVal) {
	unsigned long res = 0;

	__asm__ __volatile__(
		LOCK "cmpxchgl %1, %2;" //ZF = 1 if succeeded
		"lahf;"  // to get the correct result even if oldVal == 0
		"bt $14, %%ax;" // is ZF set? (ZF is the 6'th bit in %ah, so it's the 14'th in ax)
		"adc %0, %0" // get the result
		: "=r"(res)
		: "r"(newVal), "m"(*ptr), "a"(oldVal), "0"(res)
		: "memory"
	);

	return (bool)res;
}


/**
* This function implements the atomic_test_and_set on an integer value, for x86-64 archs
*
* @author Alessandro Pellegrini
*
* @param b the counter there to perform the operation
*
* @return true if the int value has been set, false otherwise
*/
inline int atomic_test_and_set(int *b) {
	int result = 0;

	__asm__  __volatile__ (
		LOCK "bts $0, %1;\n\t"
		"adc %0, %0"
		: "=r" (result)
		: "m" (*b), "0" (result)
		: "memory"
	);

	return !result;
}



/**
* This function implements the atomic_test_and_reset on an integer value, for x86-64 archs
*
* @author Alessandro Pellegrini
*
* @param b the counter there to perform the operation
*
* @return true if the int value has been reset, false otherwise
*/
inline int atomic_test_and_reset(int *b) {
	int result = 0;

	__asm__  __volatile__ (
		LOCK "btr $0, %1;\n\t"
		"adc %0, %0"
		: "=r" (result)
		: "m" (*b), "0" (result)
		: "memory"
	);

	return result;
}



/**
* This function implements (on x86-64 architectures) the atomic add operation.
* It adds 'i' units to the atomic counter 'v'
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
* @param i how much must be added
*/
inline void atomic_add(atomic_t *v, int i) {
	__asm__ __volatile__(
		LOCK "addl %1,%0"
		: "=m" (v->count)
		: "ir" (i), "m" (v->count)
	);
}


/**
* This function implements (on x86-64 architectures) the atomic sub operation.
* It subtracts 'i' units from the atomic counter 'v'
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
* @param i how much must be subtracted
*/
inline void atomic_sub(atomic_t *v, int i) {
	__asm__ __volatile__(
		LOCK "subl %1,%0"
		: "=m" (v->count)
		: "ir" (i), "m" (v->count)
	);
}


/**
* This function implements (on x86-64 architectures) the atomic inc operation.
* It increments the atomic counter 'v' by 1 unit
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
*/
inline void atomic_inc(atomic_t *v) {
	__asm__ __volatile__(
		LOCK "incl %0"
		: "=m" (v->count)
		: "m" (v->count)
	);
}



/**
* This function implements (on x86-64 architectures) the atomic dec operation.
* It decrements the atomic counter 'v' by 1 unit
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
*/
inline void atomic_dec(atomic_t *v) {
	__asm__ __volatile__(
		LOCK "decl %0"
		: "=m" (v->count)
		: "m" (v->count)
	);
}



/**
* This function implements (on x86-64 architectures) the atomic dec operation.
* It decrements the atomic counter 'v' by 1 unit
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
*
* @return true if the counter became zero
*/
inline int atomic_inc_and_test(atomic_t *v) {
	unsigned char c = 0;

	__asm__ __volatile__(
		LOCK "incl %0\n\t"
		"sete %1"
		: "=m" (v->count), "=qm" (c)
		: "m" (v->count)
		: "memory"
	);
	return c != 0;
}



/**
* This function implements (on x86-64 architectures) a spinlock operation.
*
* @author Alessandro Pellegrini
*
* @param s the spinlock on which to spin
*/


#ifdef SPINLOCK_GIVES_COUNT

unsigned int spin_lock(spinlock_t *s) {

	int count = -1;

	__asm__ __volatile__(
		"1:\n\t"
		"movl $1,%%eax\n\t"
		LOCK "xchgl %%eax, %2\n\t"
		"addl $1, %0\n\t"
		"testl %%eax, %%eax\n\t"
		"jnz 1b"
		: "=c" (count)
		: "c" (count), "m" (s->lock)
		: "eax", "memory"
	);

#if (!defined(NDEBUG)) && defined(HAVE_HELGRIND_H)
	ANNOTATE_RWLOCK_ACQUIRED(&((s)->lock), 1);
#endif

        return (unsigned int)count;
}


#else

inline void spin_lock(spinlock_t *s) {
	__asm__ __volatile__(
		"1:\n\t"
		"movl $1,%%eax\n\t"
		LOCK "xchgl %%eax, %0\n\t"
		"testl %%eax, %%eax\n\t"
		"jnz 1b"
		: /* no output */
		: "m" (s->lock)
		: "eax", "memory"
	);

#if (!defined(NDEBUG)) && defined(HAVE_HELGRIND_H)
	ANNOTATE_RWLOCK_ACQUIRED(&((s)->lock), 1);
#endif

}
#endif


/**
* This function implements (on x86-64 architectures) a trylock operation.
*
* @author Alessandro Pellegrini
*
* @param s the spinlock to try to acquire
*/
inline bool spin_trylock(spinlock_t *s) {
/*	unsigned int out = 0;
	unsigned int in = 1;

	__asm__ __volatile__(
		"movl $1,%%eax\n\t"
		"xchgl %%eax, %0\n\t"
		"testl %%eax, %%eax\n\t"
		: "=a" ((unsigned int)out)
		: "m" (s->lock)
		: "eax", "memory"
	);

	__asm__ __volatile__(
		LOCK "xchgl %0, %1"
		:"=r" ((unsigned int)out)
		:"m" (s->lock), "0" (in)
		:"memory");

	return (bool)out;
*/
	return atomic_test_and_set((int *)&s->lock);
}


/**
* This function implements (on x86-64 architectures) a spin unlock operation.
*
* @author Alessandro Pellegrini
*
* @param s the spinlock to unlock
*/
inline void spin_unlock(spinlock_t *s) {


#if (!defined(NDEBUG)) && defined(HAVE_HELGRIND_H)
	ANNOTATE_RWLOCK_RELEASED(&((s)->lock), 1);
#endif

	__asm__ __volatile__(
		"mov $0, %%eax\n\t"
		LOCK "xchgl %%eax, %0"
		: /* no output */
		: "m" (s->lock)
		: "eax", "memory"
	);
}

