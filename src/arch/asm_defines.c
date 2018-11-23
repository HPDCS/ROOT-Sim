/**
*			Copyright (C) 2008-2018 HPDCS Group
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
* @file asm_defines.c
* @brief This file is not actually compiled into ROOT-Sim. This is used by the
*        build system to generate the asm_defines.h file in which several sizes
*        and offsets of structs are dynamically generated. In this way, the assembly
*        code which we generate does not have to carry hard-coded constants and
*        subtle nasty bugs are less likely to appear.
* @author Alessandro Pellegrini
*/


#define DEFINE(sym, val) __asm__ __volatile__("\n-> " #sym " %0 \n" : : "i" (val))
#define OFFSETOF(s, m) DEFINE(offsetof_##s##_##m, offsetof(s, m));
#define SIZEOF(s) DEFINE(sizeof_##s, sizeof(s));

#include <scheduler/process.h>
#include <arch/x86/jmp.h>

void foo(void) {
	// We need the offset of the LP State to make a context switch in ECS
	SIZEOF(LP_State);
	OFFSETOF(LP_State, context);

	// Size and offsets of a context are used when creating/saving/restoring contexts in jmp.S
	SIZEOF(exec_context_t);
	OFFSETOF(exec_context_t, rax);
	OFFSETOF(exec_context_t, rdx);
	OFFSETOF(exec_context_t, rcx);
	OFFSETOF(exec_context_t, rbx);
	OFFSETOF(exec_context_t, rsp);
	OFFSETOF(exec_context_t, rbp);
	OFFSETOF(exec_context_t, rsi);
	OFFSETOF(exec_context_t, rdi);
	OFFSETOF(exec_context_t, r8);
	OFFSETOF(exec_context_t, r9);
	OFFSETOF(exec_context_t, r10);
	OFFSETOF(exec_context_t, r11);
	OFFSETOF(exec_context_t, r12);
	OFFSETOF(exec_context_t, r13);
	OFFSETOF(exec_context_t, r14);
	OFFSETOF(exec_context_t, r15);
	OFFSETOF(exec_context_t, rip);
	OFFSETOF(exec_context_t, flags);
	OFFSETOF(exec_context_t, fpu);
}
