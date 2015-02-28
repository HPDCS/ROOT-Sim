/**
*			Copyright (C) 2008-2015 HPDCS Group
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
* @file preempt_callback.S
* @brief This callback is used to transfer back control from
*        kernel space to platform space when an APIC interrupt is
*	 received
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*/

.file "kernel_callback.S"
.extern preempt

.text
.globl preempt_callback
.type preempt_callback, @function

preempt_callback:
	push %rbp
	mov  %rsp, %rbp	
	push %rax
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
	mov  0x10(%rbp), %rdi # TODO: in case of 32-bits execution, these
	mov  0x8(%rbp), %rsi  # two lines can be commented out
	call preempt
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
	leaveq
	add $0x10, %rsp
	retq

.size   preempt_callback, .-preempt_callback
