/**
 * @file arch/x86/ecs_callback.S
 * 
 * @brief Userspace callback for preëmptive Time Warp
 *
 * This callback is used to transfer back control from kernel space to
 * platform space when an artificial page fault due to ECS is faced.
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
 *
 * @date December, 2015
 */

.file "ecs_callback.S"

#ifdef OS_LINUX

#include <arch/asm_defines.h>

.extern ECS
.extern current

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

	# Access to current->context
	movq $current, %rsi
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
