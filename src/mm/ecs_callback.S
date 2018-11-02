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
* @file preempt_callback.S
* @brief This callback is used to transfer back control from
*        kernel space to platform space when an APIC interrupt is
*	 received
* @author Alessandro Pellegrini
* @date December, 2015
*/

.file "ecs_callback.S"

#ifdef OS_LINUX

#include <arch/asm_defines.h>

.extern ECS
.extern lps_blocks

.text
.globl rootsim_cross_state_dependency_handler
.type rootsim_cross_state_dependency_handler, @function

# This function is called by the kernel upon a page fault.
# This is the organization of the stack after the various push:
#
#    _______________________
#    |       FLAGS         |
#    |---------------------|
#    |        RAX          |
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

	# Access to context field of LPS[current_lp]
	movl %fs:current_lp@tpoff, %edi
	movq lps_blocks(%rip), %rsi
	addq $offsetof_LP_State_context, %rsi
	leaq (%rsi, %rdi, 8), %rdi
	movq (%rdi), %rdi
	
	call _set_jmp
	testq %rax, %rax
	jnz .dont

	// Fake frame to support printf in ECS
	push %rbp
	mov %rsp, %rbp

	call ECS

	pop %rbp
	
     .dont:

	# Restore FLAGS
	popq %rax
	addb $0x7f, %al		# Overflow if OF was set
	sahf

	popq %rax
	ret

.size   rootsim_cross_state_dependency_handler, .-rootsim_cross_state_dependency_handler

#endif 
