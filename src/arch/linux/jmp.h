/**
*			Copyright (C) 2008-2017 HPDCS Group
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
* @file jmp.h
* @brief setjmp / longjmp variant which saves all registers
* @author Alessandro Pellegrini
* @date December, 2015
*/

#pragma once

#ifdef OS_LINUX

/// This structure is used to maintain execution context for LPs' userspace threads
typedef struct __exec_context_t {
	// This is the space for general purpose registers
	unsigned long long rax;
	unsigned long long rdx;
	unsigned long long rcx;
	unsigned long long rbx;
	unsigned long long rsp;
	unsigned long long rbp;
	unsigned long long rsi;
	unsigned long long rdi;
	unsigned long long r8;
	unsigned long long r9;
	unsigned long long r10;
	unsigned long long r11;
	unsigned long long r12;
	unsigned long long r13;
	unsigned long long r14;
	unsigned long long r15;
	unsigned long long rip;
	unsigned long long flags;

	// Space for other registers
	unsigned char others[512] __attribute__((aligned(16))); // fxsave wants 16-byte aligned memory
} exec_context_t;

long long _set_jmp(exec_context_t *env);
__attribute__ ((__noreturn__)) void _long_jmp(exec_context_t *env, long long val);

#define set_jmp(env) 		({\
					int _set_ret;\
					__asm__ __volatile__ ("pushq %rdi"); \
					_set_ret = _set_jmp(env); \
					__asm__ __volatile__ ("add $8, %rsp"); \
					_set_ret;\
				})

#define long_jmp(env, val)	_long_jmp(env, val)

#endif /* OS_LINUX */

