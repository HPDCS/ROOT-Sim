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
* @file jmp.S
* @brief setjmp / longjmp variant which saves all registers
* @author Alessandro Pellegrini
* @date December, 2015
*/

.file "jmp.S"

#ifdef OS_LINUX

.text

# Stack organization of this function after the first three push:
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
	movq 16(%rsp), %r11 	# r11 keeps the 'old' rax
	movq %r11, (%rax)	# rax is the first field of the context
	movq %rdx, 8(%rax)
	movq %rcx, 16(%rax)
	movq %rbx, 24(%rax)
	movq %rsp, 32(%rax)
	addq $16, 32(%rax)	# saved rsp must point one quadword above the old return address
	movq %rbp, 40(%rax)
	movq %rsi, 48(%rax)
	movq 32(%rsp), %r11	# old 'rdi' was pushed by the surrounding macro
	movq %r11, 56(%rax)
	movq %r8, 64(%rax)
	movq %r9, 72(%rax)
	movq %r10, 80(%rax)
	movq 8(%rsp), %r11 	# r11 keeps the 'old' r11
	movq %r11, 88(%rax)	# r11 is the 12-th field of the context
	movq %r12, 96(%rax)
	movq %r13, 104(%rax)
	movq %r14, 112(%rax)
	movq %r15, 120(%rax)
	movq (%rsp), %rdx
	movq %rdx, 136(%rax)	# (%rsp) is flags
	
	movq 24(%rsp), %r11	# Save the original return address
	movq %r11, 128(%rax)

	# Now save other registers. fxsave wants memory aligned to 16 byte.
	# The context structure is aligned to 16 bytes. We have 18 8-byte
	# registers, so the next address is exactly the 'others' buffer.
	fxsave 144(%rax)
	
	addq $24, %rsp
	xorq %rax, %rax		# return 0 because the context is being created
	ret
	
.size   _set_jmp, .-_set_jmp


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
	movq %rdi, %rax		# rax points to the context

	movq 128(%rax), %r10	# This is the old return address
	movq 32(%rax), %r11	# r11 is the old rsp
	movq %r10, 8(%r11)	# restore the old return address

	movq %rsi, (%r11)	# Put on the old stack the desired return value
	
	movq 8(%rax), %rdx	# rdx is the second field of the context
	movq 16(%rax), %rcx 
	movq 24(%rax), %rbx 
	movq 32(%rax), %rsp 
	movq 40(%rax), %rbp
	movq 48(%rax), %rsi
	movq 64(%rax), %r8
	movq 72(%rax), %r9
	movq 80(%rax), %r10	# Finish to restore GP registers
	movq 88(%rax), %r11
	movq 96(%rax), %r12
	movq 104(%rax), %r13
	movq 112(%rax), %r14

	# Restore FLAGS
	movq 136(%rax), %rax	# this is flags
	addb $0x7f, %al		# Overflows if OF was set
	sahf
	
	
	# Restore remaining rdi and r15
	movq %rdi, %rax		# rax now points again to context
	movq 56(%rax), %rdi
	movq 120(%rax), %r15

	# Restore other registers
	fxrstor 144(%rax)
	
	movq 32(%rax), %rsp	# (possibly) change stack
	popq %rax		# Set the desired return value
	ret			# do the long jump
	
.size   _long_jmp, .-_long_jmp



# This function creates a new execution context for a new ULT
# by entirely cloning both the stack frame of the current function at the
# bottom of the alternate stack memory area and the current machine context
# where the BP and SP values come to be updated with the new values.
#
#  _OLD_SP____\ |_______________|
#             / |______RET______|
#               |__DISPLACEMENT_|
#               |_____NEW_SP____|
#               |_____NEW_BP____|
#               |_____OLD_SP____|  |\ CLONED   |_______________| /____NEW_SP_
#               |_____OLD_BP____|  | \         |______RET______| \
#               |___STACK_SIZE__|   \ \        |__DISPLACEMENT_|
#               |_____STACK_____|    \ \ |\    |_____NEW_SP____|
#               |____CREATED____|     \ \| \   |_____NEW_BP____|
#  _OLD_BP____\ |____CREATOR____|      \    \  |_____OLD_SP____|
#             / |_____FLAGS_____|       \   /  |_____OLD_BP____|
#               |______RBP______|        | /   |___STACK_SIZE__|
#               |______RAX______|        |/    |_____STACK_____|
#               |___Ret._Addr.__|              |____CREATED____|
#               |               |              |____CREATOR____| /____NEW_BP_
#                                                                \
#
# When execution flow associated to the new context will take control it
# will start working by accessing the cloned data, while the control flow
# associated to the old context will continue along its original path.
#
# Parameters to this function are as follows:
# exec_context_t *caller, exec_context_t *creat, void (*fn)(void *), void *args, void *stack, size_t stack_size
# namely:
# - RDI: caller context (1st argument)
# - RSI: context to be setup (2nd argument)
# - RDX: function to be activated in the new ULT (3rd argument)
# - RCX: arguments to be passed to the above function (4th argument)
# - R8: a pointer to the newly allocated stack (5th argument)
# - R9: the size of the newly allocated stack (6th argument)

.align 8
.globl _context_create
.type _context_create, @function
_context_create:

	pushq	%rbp

	movq	%rsp, %rbp
	subq	$0x50, %rsp
	movq	%rdi, -0x8(%rbp)	# caller context
	movq	%rsi, -0x10(%rbp)	# context to be created
	movq	%r8,  -0x18(%rbp)	# stack
	movq	%r9,  -0x20(%rbp)	# stack size
	movq	$0x0, -0x28(%rbp)	# old_BP
	movq	$0x0, -0x30(%rbp)	# old_SP
	movq	$0x0, -0x38(%rbp)	# new_BP
	movq	$0x0, -0x40(%rbp)	# new_SP
	movq	$0x0, -0x48(%rbp)	# displacement
	movq	$0x0, -0x50(%rbp)	# ret
	movq	%rdx, -0x58(%rbp)	# function to be activated
	movq	%rcx, -0x60(%rbp)	# arguments to the function

	# Save the current context of the caller
	xorq	%rax, %rax
	movq	-0x8(%rbp), %rdi
	callq	_set_jmp

	# Are we returning for a context creation or restore?
	movq	%rax, -0x50(%rbp)	# ret
	cmpq	$0x0, %rax
	jne	.L1

	# Make a copy of the context in the new context using a movs
	movq	$0x290, %rcx		# context_size
	movq	-0x8(%rbp), %rsi	# creator
	movq	-0x10(%rbp), %rdi	# created
	movq	0x28(%rsi), %rax	# old_BP
	movq	%rax, -0x28(%rbp)	# old_BP
	movq	0x20(%rsi), %rax	# old_SP
	movq	%rax, -0x30(%rbp)	# old_SP

	cld
	rep movsb

	# Make a copy of the stack in the new context
	movq	-0x28(%rbp), %rcx	# old_BP
	movq	-0x30(%rbp), %rsi	# old_SP
	subq	%rsi, %rcx		# displacement
	movq	-0x18(%rbp), %rdi	# stack
	movq	-0x20(%rbp), %rax	# stack_size
	addq	%rax, %rdi		# new_BP
	xorq	%rax, %rax
	movq	$0x3f, %rax
	notq	%rax
	andq	%rax, %rdi		# new_BP = new_BP & ~0x3F
	movq	%rdi, -0x38(%rbp)	# new_BP
	subq	%rcx, %rdi		# new_SP (1st arg)
	movq	%rdi, -0x40(%rbp)	# new_SP

	rep movsb			# cld is not needed here, it
					# was issued before in the function

	# MAGIC_NUMBER is used to prevent the context from which we
	# are creating the new one to call into `context_create_boot`
	movq	$0x1ae7, %rsi		# MAGIC_NUMBER (2nd arg)
	movq	-0x10(%rbp), %rdi	# created (1st arg)
	movq	-0x38(%rbp), %rax	# new_BP
	movq	%rax, 0x28(%rdi)	# created->rbp = new_BP
	movq	-0x40(%rbp), %rax	# new_SP
	movq	%rax, 0x20(%rdi)	# created->rsp = new_SP

	callq	_long_jmp

    .align 8
    .L1:
	# Make a sanity check on the magic number. If we get here in the
	# context from which the new one was created, just leave!
	cmpq	$0x1ae7, -0x50(%rbp)
	jne	.L2

	movq	-0x8(%rbp), %rdi	# creator (1st arg)
	movq	-0x10(%rbp), %rsi	# created (2nd arg)
	movq	-0x58(%rbp), %rdx	# function (3rd arg)
	movq	-0x60(%rbp), %rcx	# arguments (4th arg)

	callq	context_create_boot

    .align 8
    .L2:
	# Get out of here!
	movq	-0x20(%rbp), %rcx
	movq	-0x18(%rbp), %rdx
	movq	-0x10(%rbp), %rsi
	movq	-0x8(%rbp), %rdi
	movq	%rbp, %rsp

	popq	%rbp

	ret

.size   _context_create, .-_context_create

#endif /* OS_LINUX */
