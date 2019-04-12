/**
 * @file arch/x86/preempt_callback.S
 * 
 * @brief Userspace callback for preëmptive Time Warp
 *
 * This callback is used to transfer back control from kernel space to
 * platform space when an APIC interrupt is received.
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
 * @author Alessandro Pellegrini
 *
 * @date March, 2015
 */

.file "preempt_callback.S"
.extern preempt
.extern return_from_kernel

#ifdef HAVE_PREEMPTION

#include <arch/asm_defines.h>

.text
.globl preempt_callback
.type preempt_callback, @function

preempt_callback:
	push %rbp
	push %rax

	lahf
	seto %al
	pushq %rax

	push %rbx
	push %rcx
	push %rdx
	push %rdi
	push %rsi

	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15

	subq $16, %rsp
	movsd %xmm0, (%rsp)
	subq $16, %rsp
	movsd %xmm1, (%rsp)

	call preempt

	movsd (%rsp), %xmm1
	addq $16, %rsp
	movsd (%rsp), %xmm0
	addq $16, %rsp

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8

	pop %rsi
	pop %rdi
	pop %rdx
	pop %rcx
	pop %rbx

	pop %rax
	addb $0x7f, %al
	sahf

	pop %rax
	pop %rbp
	retq

.size   preempt_callback, .-preempt_callback

#endif /* HAVE_PREEMPT */
