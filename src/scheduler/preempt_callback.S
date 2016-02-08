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
* @date March, 2015
*/

#include <limits.h>

.file "preempt_callback.S"
.extern preempt

#if defined(HAVE_PREEMPTION) && ( defined(ARCH_X86) || defined(ARCH_X86_64) )

.text
.globl preempt_callback
.type preempt_callback, @function

preempt_callback:
	pushq %rax

	# Save FLAGS
	lahf
	seto %al
	pushq %rax

	# TODO: only in debug mode
	movq $preempt_count, %rax
	lock incl (%rax)


	# Check whether we are running in platform mode or if the LP is rolling back
	movzbl  %fs:platform_mode@tpoff, %eax
        testb   %al, %al
        jne     .dont2
        movzbl  %fs:rolling_back@tpoff, %eax
        testb   %al, %al
        jne     .dont2

	# TODO: only in debug mode
	movq $overtick_user, %rax
	lock incl (%rax)
	
	push %rdx
	push %rcx
	subq $16, %rsp
	movsd %xmm0, (%rsp)


	# Compare min_in_transit_lvt with current_lvt
	movq    tid@gottpoff(%rip), %rdx
        movq    current_lvt@gottpoff(%rip), %rcx
        movq    min_in_transit_lvt(%rip), %rax
        movl    %fs:(%rdx), %edx
        movsd   %fs:(%rcx), %xmm0
        ucomisd (%rax,%rdx,8), %xmm0
        jbe      .dont3

	# TODO: only in debug mode
	movq $would_preempt, %rax
	lock incl (%rax)
	
	pushq %rdi

	# Set the LP to LP_STATE_SUSPENDED and switch to platform mode
	movq    current_lp@gottpoff(%rip), %rax
        movl    %fs:(%rax), %edx
        movq    LPS(%rip), %rax
        movq    (%rax,%rdx,8), %rdi
        movb    $1, %fs:platform_mode@tpoff
//        movl    $4112, %eax		# TODO! This is LP_STATE_SUSPENDED
        movw    $4112, 1340(%rdi)	# TODO! This is the offset of state in the LPS structure


	# Save the state of the current LP. %rdi keeps the pointer to LPS[current_lp]
	movq	$1, %rax
        call    _set_jmp


	# Do the actual context switch
	testl   %eax, %eax
        jnz     .dont4

	movq    kernel_context@gottpoff(%rip), %rax
        movq    %fs:(%rax), %rsi
        addq    %fs:0, %rax
        movq    %rax, %rdi
        call    _long_jmp

     .dont4:
	popq %rdi
     .dont3:
	movsd (%rsp), %xmm0
	addq $16, %rsp
	popq %rcx
	popq %rdx


     .dont2:

	# Restore FLAGS
	popq %rax
	addb $0x7f, %al		# Overflow if OF was set
	sahf

	popq %rax

	retq
.size   preempt_callback, .-preempt_callback

#endif /* defined(HAVE_PREEMPTION) and ( defined(ARCH_X86) or defined(ARCH_X86_64) ) */
