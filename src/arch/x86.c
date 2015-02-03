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
* @file atomic.c
* @brief This module implements atomic and non-blocking operations used within ROOT-Sim
*        to coordinate threads and processes (on shared memory)
* @author Alessandro Pellegrini
* @date Jan 25, 2012
*/


// Do not compile anything here if we're not on an x86 machine!
#if defined(ARCH_X86) || defined(ARCH_X86_64)


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <arch/atomic.h>


/**
* This function implements a compare-and-swap atomic operation on x86 for long long values
*
* @author Alessandro Pellegrini
*
* @param ptr the address where to perform the CAS operation on
* @param oldVal the old value we expect to find before swapping
* @param newVal the new value to place in ptr if ptr contains oldVal
*
* @ret true if the CAS succeeded, false otherwise
*/
inline bool CAS_x86(volatile unsigned long long *ptr, unsigned long long oldVal, unsigned long long newVal) {
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
* @ret true if the CAS succeeded, false otherwise
*/
inline bool iCAS_x86(volatile unsigned int *ptr, unsigned int oldVal, unsigned int newVal) {
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
* @ret true if the int value has been set, false otherwise
*/
inline int atomic_test_and_set_x86(int *b) {
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
* @ret true if the int value has been reset, false otherwise
*/
inline int atomic_test_and_reset_x86(int *b) {
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
inline void atomic_add_x86(atomic_t *v, int i) {
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
inline void atomic_sub_x86(atomic_t *v, int i) {
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
inline void atomic_inc_x86(atomic_t *v) {
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
inline void atomic_dec_x86(atomic_t *v) {
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
inline int atomic_inc_and_test_x86(atomic_t *v) {
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

inline unsigned int spin_lock(spinlock_t *s) {

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

        return (unsigned int)count;
}


#else

inline void spin_lock_x86(spinlock_t *s) {

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
}
#endif


/**
* This function implements (on x86-64 architectures) a trylock operation.
*
* @author Alessandro Pellegrini
*
* @param s the spinlock to try to acquire
*/
inline bool spin_trylock_x86(spinlock_t *s) {
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
	return atomic_test_and_set_x86((int *)&s->lock);
}


/**
* This function implements (on x86-64 architectures) a spin unlock operation.
*
* @author Alessandro Pellegrini
*
* @param s the spinlock to unlock
*/
inline void spin_unlock_x86(spinlock_t *s) {

	__asm__ __volatile__(
		"mov $0, %%eax\n\t"
		LOCK "xchgl %%eax, %0"
		: /* no output */
		: "m" (s->lock)
		: "eax", "memory"
	);
}


#endif /* defined(ARCH_X86) || defined(ARCH_X86_64) */
