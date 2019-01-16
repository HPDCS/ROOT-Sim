/**
* @file arch/x86/jmp.h
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

#pragma once

#ifdef OS_LINUX
#if defined(__x86_64__)

/// This structure describes the CPU context of a User-Level Thread
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

	// Space for FPU registers
	unsigned char fpu[512] __attribute__((aligned(16))); // fxsave wants 16-byte aligned memory
} exec_context_t;

/**
 * This macro is a wrapper for _set_jmp(), which is the custom implementation
 * of the standard setjmp used in ROOT-Sim.
 * The goal of the wrapper is the following. _set_jmp() requires one
 * parameter, which is the pointer to an exec_context_t where to store the
 * current context. Since the custom implementation is such that *all*
 * registers should be saved in the context, not only callee-save as in the
 * standard implementation, simply calling _set_jmp() clobbers a register
 * which is later not possible to save. This macro pushes the RDI register,
 * used to keep the first parameter of function calls according to SysV ABI,
 * before calling into _set_jmp(). _set_jmp() expects this to happen, as
 * it will find the original value of RDI to save on the stack frame of
 * the caller. This is why it is not possible to directly call _set_jmp()
 * from the code, but this macro must be explicitly used.
 *
 * The semantics of this macro are the same as the called _set_jmp().
 *
 * @param env A pointer to an exec_context_t structure where to store
 *            the current execution contenxt.
 *
 * @return 0 if directly called. Otherwise, upon a context restore via
 *         long_jmp(), the return value is set to the value of the second
 *         argument passed to long_jmp().
 */
#define set_jmp(env) 	({\
				int _set_ret;\
				__asm__ __volatile__ ("pushq %rdi"); \
				_set_ret = _set_jmp(env); \
				__asm__ __volatile__ ("add $8, %rsp"); \
				_set_ret;\
			})

/**
 * This macro is a wrapper for _long_jmp(). There is no special care to be
 * taken when calling _long_jmp(), so this macro is offered only to
 * make uniform the usage of the api.
 *
 * @param env A pointer to an @ref exec_context_t structure keeping a CPU
 *            context to restore.
 * @param val This is the value returned by set_jmp() when the context
 *            specified in env is restored.
 */
#define long_jmp(env, val)	_long_jmp(env, val)

/**
 * _set_jmp() is a function implemented in assembly in jmp.S which
 * saves the whole CPU state (both general purpose registers and FPU
 * registers and state) into the buffer pointed by @p env.
 *
 * The semantic of this function is the same as the surrounding macro
 * @ref set_jmp, which should be used instead. Indeed, calling this function
 * will definitely produce a wrong CPU state, because at least the RDI
 * register cannot be saved when directly calling it, due to register
 * clobbering for parameter passing.
 *
 * Therefore, this function cannot be directly called (it's poisoned).
 *
 * @param env A pointer to an @ref exec_context_t structure where to store
 * 	      the current CPU state
 *
 * @return 0 if directly called. Otherwise, upon a context restore via
 *         long_jmp(), the return value is set to the value of the second
 *         argument passed to long_jmp().
 *
 * @warning Do not call this function: it's poisoned. Use the @ref set_jmp
 *          macro instead.
 */
long long _set_jmp(exec_context_t *env);



/**
 * _long_jmp() is a function implemented in assembly in jmp.S which
 * restores the whole CPU state (both general purpose registers and FPU
 * registers and state) from the buffer pointed by @p env.
 *
 * This function cannot be directly called (it's poisoned). Use the
 * macro @ref  long_jmp instead.
 *
 * @param env A pointer to an @ref exec_context_t structure where to store
 * 	      the current CPU state
 * @param val This is the value which is returned from _set_jmp() when that
 *            function returns after that the context in @p env has been
 *            restored.
 *
 * @return 0 if directly called. Otherwise, upon a context restore via
 *         long_jmp(), the return value is set to the value of the second
 *         argument passed to long_jmp().
 *
 * @warning Do not call this function: it's poisoned. Use the @ref long_jmp
 *          macro instead.
 */
__attribute__((__noreturn__))
void _long_jmp(exec_context_t *env, long long val);



/**
 * This function creates a new User-Level Thread by instantiating a new
 * valid context in @p env.
 * After that this function returns, the context in @p env can be safely
 * passed to @ref long_jmp. The execution will then start from the routine
 * passed in @p fn.
 *
 * @param creat A pointer to an @ref exec_context_t structure where the
 *              context for the new ULT should be initialized.
 * @param fn A function pointer which will be the entry point of the ULT
 *           once it is first scheduled using @ref long_jmp.
 * @param args Arguments to be passed to @p fn once the context is activated.
 * @param stack A pointer to a memory area to be used as stack. This memory
 *              area must be aligned to 16 bytes on x86.
 * @param stack_size the size of the memory area pointed by @p stack.
 *
 * @return 0 if directly called. Otherwise, upon a context restore via
 *         long_jmp(), the return value is set to the value of the second
 *         argument passed to long_jmp().
 */
extern void context_create(exec_context_t *creat, void (*fn)(void *), void *args, void *stack, size_t stack_size);


// As mentioned, using direct calls to _set_jmp or _long_jmp is not safe.
// Here we poison their explicit usage, forcing the code base to rely
// on wrapper macros.
// on wrapper macros.
#pragma GCC poison _set_jmp _long_jmp


#endif	/* defined(__x86_64__) */
#endif	/* OS_LINUX */
