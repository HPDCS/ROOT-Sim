// setjmp / longjmp variant which saves all registers

#pragma once
#ifndef _JMP_H
#define _JMP_H

/// This structure is used to maintain execution context for LPs' userspace threads
typedef struct __exec_context_t {
	// This is the space for general purpose registers
	unsigned long long rax;
	unsigned long long rdx;
	unsigned long long rcx;
	unsigned long long rbx;
	unsigned long long rsp;
	unsigned long long rbp;
	unsigned long long rsi;
	unsigned long long rdi;
#ifdef ARCH_X86_64
	unsigned long long r8;
	unsigned long long r9;
	unsigned long long r10;
	unsigned long long r11;
	unsigned long long r12;
	unsigned long long r13;
	unsigned long long r14;
	unsigned long long r15;
#endif
	unsigned long long rip;
	unsigned long long flags;
	
	// Space for other registers
	unsigned char others[512] __attribute__((aligned(16))); // fxsave wants 16-byte aligned memory
} exec_context_t;

extern __attribute__((regparm(0))) int set_jmp(exec_context_t *env);
extern __attribute__((regparm(0))) void long_jmp(exec_context_t *env, int val);

extern void set_jmp_other(exec_context_t *env);
extern void long_jmp_other(exec_context_t *env);

#endif /* _JMP_H */
