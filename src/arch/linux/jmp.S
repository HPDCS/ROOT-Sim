
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

#if defined(ARCH_X86_64)

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

.align 4
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


.align 4
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

#endif
