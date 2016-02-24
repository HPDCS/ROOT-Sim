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
* @date December, 2015
*/

.file "ecs_callback.S"

#if defined(ARCH_X86_64) && defined(OS_LINUX)

.extern ECS
.extern LPS

.text
.globl rootsim_cross_state_dependency_handler
.type rootsim_cross_state_dependency_handler, @function

# This function is called by the kernel upon a page fault.
# This is the organization of the stack after the various push:
#
#    _______________________
#    |        RDI          |
#    |---------------------|
#    |        RSI          |
#    |---------------------|
#    |       FLAGS         |
#    |---------------------|
#    |        RAX          |
#    |---------------------|
#    |  WORKER THREAD ID   |  -> Placed by kernel module
#    |---------------------|
#    |      TARGET LP      |  -> Placed by kernel module
#    |---------------------|
#    |   RETURN ADDRESS    |  -> Placed by kernel module
#    |---------------------|
#
# This function accepts no arguments

rootsim_cross_state_dependency_handler:
	pushq %rax

	# Save FLAGS
	lahf
	seto %al
	push %rax

	# Force checking 
	movq $1, %rax

	pushq %rsi
	pushq %rdi

	# Access to context field of LPS[current_lp]
	movl %fs:current_lp@tpoff, %edi
	movq LPS(%rip), %rsi
	leaq (%rsi, %rdi, 8), %rdi
	movq (%rdi), %rdi
	
	call _set_jmp
	testq %rax, %rax
	jnz .dont

	// Fake frame to support printf in ECS
	push %rbp
	mov %rsp, %rbp

//	movq 32(%rsp), %rdi
//	movq 40(%rsp), %rsi
	movq 40(%rsp), %rdi
	movq 48(%rsp), %rsi
	call ECS

	add $8, %rsp
	pop %rbp
	

     .dont:

	popq %rdi
	popq %rsi

	# Restore FLAGS
	popq %rax
	addb $0x7f, %al		# Overflow if OF was set
	sahf

	popq %rax

	
	addq $16, %rsp
	ret

.size   rootsim_cross_state_dependency_handler, .-rootsim_cross_state_dependency_handler

#endif /* defined(ARCH_X86_64) && defined(OS_LINUX) */
