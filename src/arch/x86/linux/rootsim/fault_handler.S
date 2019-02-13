.extern rootsim_page_fault

/**
 * This function is hooked directly in the IDT entry associated with a
 * page fault. Therefore, once getting here, the firmware gives us
 * the following return frame for iretq:
 *
 * |--------------------|
 * | [ %ss ]            |
 * | [ %sp ]            |
 * | %rflags            |
 * | %cs                |
 * | %rip               |
 * | error code         | <-- %rsp
 * |--------------------|
 *
 * We must check whether the fault is received when running in kernel
 * more or not. This can be done by checking bit 3 in the saved CS value
 * on the stack.
 * do_page_fault() has the following parameters which we have to properly
 * prepare:
 *  - struct pt_regs *regs
 *  - unsigned long err
 *
 * This function runs with interrupts off, but once we give control to
 * other kernel functions, nothing can be assumed about this.
 */
.globl fault_handler
.align 32
fault_handler:
	cld			// kernel code wants string instructions to move forward
	testq $3, 16(%rsp)	// check if we have to change GS segment due to running in kernel mode
	jz 1f
	swapgs
    1:
	pushq %rdi		// prepare missing pt_regs. Don't save callee-save registers
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rax
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11

	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsp, %rdi		// pt_regs pointer TODO: add an offset to account for full struct pt_regs
	movq 120(%rsp), %rsi	// get error code (orig_ax in pt_regs)
	movq $-1, 120(%rsp)	// no syscall to restart
	
	call rootsim_page_fault

	cli			// Restore regs and return to kernel (if needed) atomically

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rax
	popq %rcx
	popq %rdx
	popq %rsi
	popq %rdi
	
	testq $3, 16(%rsp)
	jz 2f
	swapgs
    2:
	iretq			// Get back to userspace

.type fault_handler, %function


