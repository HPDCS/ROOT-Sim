.extern rootsim_page_fault

.globl fault_handler
fault_handler:
	cld
	testq $3, 8(%rsp)
	jz 1f
	swapgs
    1:
	pushq $0 	# error code
	pushq %rdi
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
	call rootsim_page_fault
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
	addq $8, %rsp 	# error code
	testq $3, 8(%rsp)
	jz 2f
	swapgs
    2:
	iretq



