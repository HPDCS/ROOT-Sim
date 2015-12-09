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
* @file jmp.S
* @brief setjmp / longjmp variant which saves all registers
* @author Alessandro Pellegrini
* @date December, 2015
*/

.file "jmp.S"
.extern set_jmp_other
.extern long_jmp_other


#if defined(ARCH_X86_64)

.text

.globl set_jmp
.type set_jmp, @function
set_jmp:
	pushfq			# save flags
	pushq %rax 		# save rax, it will point to the context
	pushq %rdx		# save rbd, it will be used as the source
	movq 32(%rsp), %rax 	# rax points to the context
	movq 16(%rsp), %rdx 	# rdx keeps the 'old' rax
	movq %rdx, (%rax)	# rax is the first field of the context
	movq 8(%rsp), %rdx 	# rdx keeps the 'old' rdx
	movq %rdx, 8(%rax)	# rdx is the second field of the context
	movq %rcx, 16(%rax)
	movq %rbx, 24(%rax)
	movq %rsp, 32(%rax)
	addq $24, 32(%rax)	# saved rsp must point to return address
	movq %rbp, 40(%rax)
	movq %rsi, 48(%rax)
	movq %rdi, 56(%rax)
	movq %r8, 64(%rax)
	movq %r9, 72(%rax)
	movq %r10, 80(%rax)
	movq %r11, 88(%rax)
	movq %r12, 96(%rax)
	movq %r13, 104(%rax)
	movq %r14, 112(%rax)
	movq %r15, 120(%rax)
	movq (%rsp), %rdx
	movq %rdx, 136(%rax)	# (%rsp) is flags

	movq %rax, %rdi 	# this is the context
	call set_jmp_other
	
	addq $24, %rsp
	xorq %rax, %rax		# return 0 because the context is being created
	
	# Must save the RIP value at this point!
	call .save_rip
    .do_save:
	movq (%rsp), %rdx
	movq %rdx, 128(%rax)
	addq $8, %rsp
	jmp .saved
    .save_rip:
	jmp .do_save	# This instruction must be skipped by long_jmp
    .saved:
	ret
	
.size   set_jmp, .-set_jmp


# This is the displacement to be applied by long_jmp to the saved RIP value
.equ return_displacement, .saved-.save_rip

# TODO: RAX is a problem here, as it has a twofold nature!

.globl long_jmp
.type long_jmp, @function	
long_jmp:
	movq 24(%rsp), %rax	# rax points to the context
	
	movq 8(%rax), %rdx	# rdx is the second field of the context
	movq 16(%rax), %rcx 
	movq 24(%rax), %rbx 
	movq 32(%rax), %rsp 
	movq 40(%rax), %rbp
	movq 48(%rax), %rsi
	movq 56(%rax), %rdi
	movq 64(%rax), %r8
	movq 72(%rax), %r9
	movq 80(%rax), %r10
	movq 88(%rax), %r11
	movq 96(%rax), %r12
	movq 104(%rax), %r13
	movq 112(%rax), %r14
	movq 120(%rax), %r15
	pushq 136(%rax)		# this is flags
	popfq
	
	movq %rax, %rdi 	# this is the context
	call long_jmp_other
	
	movq 16(%rsp), %rax	# set up desired return value
	movq 32(%rax), %rsp	# (possibly) change stack
	
	# By construction, the stack pointer points to the return address
	# of the corresponding set_jmp.
	# Stored RIP in the context buffer points to .save_rip label
	# in the same function call. We should set the return value of
	# this function (caveat: no return point exists at this point
	# as we have restored rsp!) to the next instruction in the same
	# function, applying a positive displacement to RIP
	
	push 128(%rax)		# This is the original RIP value
	addq $return_displacement, (%rsp) # Displace RIP
	
	ret			# do the long jump
	
.size   long_jmp, .-long_jmp

#endif
