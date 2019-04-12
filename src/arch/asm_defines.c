/**
 * @file arch/asm_defines.c
 *
 * @brief Assembly build support module
 *
 * This file is not actually compiled into ROOT-Sim. This is used by the
 * system to generate the asm_defines.h file in which several sizes
 * offsets of structs are dynamically generated. In this way, the assembly
 * which we generate does not have to carry hard-coded constants and
 * nasty bugs are less likely to appear.
 *
 * This file is not compiled in the runtime environment, but it is
 * processed in order to generate the @c asm_defines.h one which is
 * included by all assembly files.
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
 */


#ifdef ASM_DEFINES


#include <scheduler/process.h>
#include <arch/x86/jmp.h>

/**
 * This macro emits some non-standard assembly code, which cannot be actually
 * assembled, but allows to retrieve the lines we are interested in in all
 * the garbage which is generated when compiling this file with -S. This is done
 * by prepending to the line the string "->" as a marker for the lines of interest.
 * We then emit a constant, which corresponds to the value of interest to be
 * retrieved using OFFSETOF() of SIZEOF().
 */
#define DEFINE(sym, val) __asm__ __volatile__("\n-> " #sym " %0 \n" : : "i" (val))

/**
 * This macro relies no the DEFINE() macro to generate a line in which the offset
 * of a member of a structure from the beginning of that structure is dynamically
 * computed at compile time.
 */
#define OFFSETOF(s, m) DEFINE(offsetof_##s##_##m, offsetof(s, m));

/**
 * This macro relies no the DEFINE() macro to generate a line in which the size
 * of a structure is dynamically computed at compile time.
 */
#define SIZEOF(s) DEFINE(sizeof_##s, sizeof(s));

/**
 * This is a function which is not actually compiled in the core, and thus
 * never ever executed. We declare in this function a set of statements which
 * are later assembled and post-processed using awk in the makefile to expose
 * to the assembly modules some constants which can only be computed by the
 * C compiler.
 * By doing this, if we change C struct definitions, we do not break assembly
 * code.
 *
 * @warning Do not call this function. This is not compiled in the library
 */
void foo(void)
{
	// We need the offset of the LP State to make a context switch in ECS
	SIZEOF(struct lp_struct);
	OFFSETOF(struct lp_struct, context);

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

#endif /* ASM_DEFINES */
