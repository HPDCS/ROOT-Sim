/**
* @file arch/x86/jmp.S
* 
* @brief x86 ULT support header
*
* This header defines all the facilities to implement User-Level Threads
* on x86.
*
* The core of the implementation is found in jmp.S which is written in
* assembly. jmp.S is undocumented here, but looking at its source will
* give an explanation of all the functions and how they behave.
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

.file "jmp.S"

#ifdef OS_LINUX
#if defined(__x86_64__)

#include <arch/asm_defines.h>

.text

# _set_jmp: create a snapshot of the current running ULT, for later restore.
#
# Stack organization of this function after the first three push instruction:
#
#    _______________
#    |    FLAGS    |
#    |-------------|
#    |     R11     |
#    |-------------| <-- Saved RSP points here
#    |     RAX     |
#    |-------------|
#    |  Ret. Addr. |
#    |-------------|
#    |   Old RDI   | <-- This is pushed by wrapping macro
#    |-------------|
#
# Arguments to this function:
#  - RDI: pointer to the CPU context where to store the current (before call) context
#
# The following defines keep the offsets described in the picture above
# starting from the stack pointer. This is because this function does
# not actually creates a valid stack frame, to save some registers

#define old_flags	0
#define old_r11		8
#define old_rax		16
#define ret_addr	24
#define old_rdi		32

.align 8
.globl _set_jmp
.type _set_jmp, @function
_set_jmp:
	pushq %rax 		# save rax, it will point to the context
	pushq %r11		# save r11, it will be used as the source

	# Save only status flags
	lahf
	seto %al
	pushq %rax

	# Save the context
	movq %rdi, %rax 	# rax points to the context
	movq old_rax(%rsp), %r11 	# r11 keeps the 'old' rax
	# Save GP registers at appropriate offsets
	movq %r11, offsetof_exec_context_t_rax(%rax)
	movq %rdx, offsetof_exec_context_t_rdx(%rax)
	movq %rcx, offsetof_exec_context_t_rcx(%rax)
	movq %rbx, offsetof_exec_context_t_rbx(%rax)
	movq %rsp, offsetof_exec_context_t_rsp(%rax)
	# saved rsp must point one quadword above the old return address
	addq $16,  offsetof_exec_context_t_rsp(%rax)
	movq %rbp, offsetof_exec_context_t_rbp(%rax)
	movq %rsi, offsetof_exec_context_t_rsi(%rax)
	# old 'rdi' was pushed by the surrounding macro
	movq old_rdi(%rsp), %r11
	movq %r11, offsetof_exec_context_t_rdi(%rax)
	movq %r8, offsetof_exec_context_t_r8(%rax)
	movq %r9, offsetof_exec_context_t_r9(%rax)
	movq %r10, offsetof_exec_context_t_r10(%rax)
	# make r11 keep the 'old' r11
	movq old_r11(%rsp), %r11
	movq %r11, offsetof_exec_context_t_r11(%rax)
	movq %r12, offsetof_exec_context_t_r12(%rax)
	movq %r13, offsetof_exec_context_t_r13(%rax)
	movq %r14, offsetof_exec_context_t_r14(%rax)
	movq %r15, offsetof_exec_context_t_r15(%rax)
	# save flags, pushed at the beginning of this function
	movq old_flags(%rsp), %rdx
	movq %rdx, offsetof_exec_context_t_flags(%rax)

	# Save the original return address
	movq ret_addr(%rsp), %r11
	movq %r11, offsetof_exec_context_t_rip(%rax)

	# Now save other registers. fxsave wants memory aligned to 16 byte.
	# The context structure is aligned to 16 bytes. We have 18 8-byte
	# registers, so the next address is exactly the 'others' buffer.
	fxsave offsetof_exec_context_t_fpu(%rax)

	addq $24, %rsp
	xorq %rax, %rax		# return 0 because the context is being created
	ret

.size   _set_jmp, .-_set_jmp


#undef old_flags
#undef old_r11
#undef old_rax
#undef ret_addr
#undef old_rdi



# _long_jmp: restore a previously saved ULT context.
#
# This long_jmp version does not restore RAX. In fact, the common usage is to
# call longjmp(context, val); and RAX is set to val. This function respects this,
# so that if RAX should be restored as well, it is possible to call
# long_jmp(context, context->rax);
#
# Arguments to this function:
#  - RDI: pointer to the CPU context to restore
#  - RSI: return value of this function call
#
# The return address at the end of the function does not return control to the
# caller, rather to the instruction immediately after the set_jmp call.
#
# The organization of this long_jmp implementation works as well if the stack of
# the caller and the stack of the destination of the long jump are different.


.align 8
.globl _long_jmp
.type _long_jmp, @function
_long_jmp:
	movq %rdi, %rax		# rax now points to the context

	movq offsetof_exec_context_t_rip(%rax), %r10	# This is the old return address
	movq offsetof_exec_context_t_rsp(%rax), %r11	# r11 is the old rsp
	movq %r10, 8(%r11)	# restore the old return address

	movq %rsi, (%r11)	# Put on the old stack the desired return value

	# Restore GP registers from the saved context
	movq offsetof_exec_context_t_rdx(%rax), %rdx
	movq offsetof_exec_context_t_rcx(%rax), %rcx
	movq offsetof_exec_context_t_rbx(%rax), %rbx
	movq offsetof_exec_context_t_rsp(%rax), %rsp
	movq offsetof_exec_context_t_rbp(%rax), %rbp
	movq offsetof_exec_context_t_rsi(%rax), %rsi
	movq offsetof_exec_context_t_r8(%rax), %r8
	movq offsetof_exec_context_t_r9(%rax), %r9
	movq offsetof_exec_context_t_r10(%rax), %r10
	movq offsetof_exec_context_t_r11(%rax), %r11
	movq offsetof_exec_context_t_r12(%rax), %r12
	movq offsetof_exec_context_t_r13(%rax), %r13
	movq offsetof_exec_context_t_r14(%rax), %r14

	# Restore FLAGS
	movq offsetof_exec_context_t_flags(%rax), %rax	# this is flags
	addb $0x7f, %al		# Overflows if OF was set
	sahf

	# Restore remaining rdi and r15
	movq %rdi, %rax		# rax now points again to context
	movq offsetof_exec_context_t_rdi(%rax), %rdi
	movq offsetof_exec_context_t_r15(%rax), %r15

	# Restore other registers
	fxrstor offsetof_exec_context_t_fpu(%rax)

	# (possibly) change stack
	movq offsetof_exec_context_t_rsp(%rax), %rsp
	# Set the desired return value. This was RSI passed to the function,
	# which was moved on top of the new stack (now restored) at the
	# beginning of this function
	popq %rax
	ret			# do the long jump

.size   _long_jmp, .-_long_jmp





# _context_create: create a context for a new ULT. The new ULT is assumed
# to never return from the first function it starts in (no valid return
# address is stored in the initial context).
#
# Parameters to this function are as follows:
# - RDI: context to be setup (2nd argument)
# - RSI: function to be activated in the new ULT (3rd argument)
# - RDX: arguments to be passed to the above function (4th argument)
# - RCX: a pointer to the newly allocated stack (5th argument)
# - R8: the size of the newly allocated stack (6th argument)
#
# We do not clobber any callee-save register here.

.align 8
.globl context_create
.type context_create, @function
context_create:

	# Initialize the FPU context to the values kept in this function
	# (we do not care about the values, but FPU configuration is saved)
	fxsave offsetof_exec_context_t_fpu(%rdi)

	# Compute and store the stack pointer
	addq %r8, %rcx
	# subq $8 would theoretically be enough to have a valid stack
	# but _set_jump and _long_jump expect a caller to already
	# have populated it: we need to reserve 8 bytes for the fake
	# return address and 8 more to keep 16 bytes alignment.
	subq $24, %rcx
	movq %rcx, offsetof_exec_context_t_rsp(%rdi)

	# Set the function to be activated and its parameter
	movq %rsi, offsetof_exec_context_t_rip(%rdi)
	movq %rdx, offsetof_exec_context_t_rdi(%rdi)

	ret

.size   context_create, .-context_create

#endif /* defined(__x86_64__) */
#endif /* OS_LINUX */

