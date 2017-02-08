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
	unsigned long long r8;
	unsigned long long r9;
	unsigned long long r10;
	unsigned long long r11;
	unsigned long long r12;
	unsigned long long r13;
	unsigned long long r14;
	unsigned long long r15;
	unsigned long long rip;
	unsigned long long flags;
	
	// Space for other registers
	unsigned char others[512] __attribute__((aligned(16))); // fxsave wants 16-byte aligned memory
} exec_context_t;

long long _set_jmp(exec_context_t *env);
__attribute__ ((__noreturn__)) void _long_jmp(exec_context_t *env, long long val);

#define set_jmp(env) 		({\
					int _set_ret;\
					__asm__ __volatile__ ("pushq %rdi"); \
					_set_ret = _set_jmp(env); \
					__asm__ __volatile__ ("add $8, %rsp"); \
					_set_ret;\
				})

#define long_jmp(env, val)	_long_jmp(env, val)

#endif /* _JMP_H */
