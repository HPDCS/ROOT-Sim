/**
 * @file arch/ult.h
 *
 * @brief User-Level Threads Headers
 *
 * The User-Level Thread module allows the creation/scheduling of multiple
 * co-routines which can yield the CPU to a different one, or that can
 * be preëmpted by some asynchronous event.
 *
 * Due to the possibility of external preëmption, ULTs store the whole
 * CPU context in an ad-hoc buffer. This is a significant difference with
 * respect standard facilities such as setjmp/longjmp which only store
 * callee-save registers.
 *
 * ULTs are used to implement both runtime-level routines (each worker
 * thread has its own execution context) and model-level routines (each
 * LP has its own execution context).
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
 */

#pragma once

#include <core/core.h>

#if defined(OS_LINUX)

#if !defined(__x86_64__)
#error Unsupported architecture
#endif

#include <arch/jmp.h>

/// Definition of an execution context for an LP. This is just syntactic sugar.
typedef exec_context_t LP_context_t;

/// Definition of an execution context for a worker thread. This is just syntactic sugar.
typedef exec_context_t kernel_context_t;

/**
 * @brief Save machine context for userspace context switch
 *
 * This macro performs a context save in the specified context (a pointer).
 *
 * This is used only in initialization to setup the execution contexts.
 * After that, the platform only relies on context_switch()
 *
 * @param context A pointer to the @ref exec_context_t to save the current
 *                CPU context into.
 */
#define context_save(context) set_jmp(context)

/// Restore machine context for userspace context switch. This is used only in inizialitaion.
#define context_restore(context) long_jmp(context, 1)

/// Swicth machine context for userspace context switch. This is used to schedule a LP or return control to simulation kernel
#define context_switch(context_old, context_new)		\
	if(set_jmp(context_old) == 0)				\
		long_jmp(context_new, (context_new)->rax)

/// Swicth machine context for userspace context switch. This is used to schedule a LP or return control to simulation kernel
#define context_switch_create(context_old, context_new)	\
	if(set_jmp(context_old) == 0)			\
		long_jmp(context_new, 1)

extern void *get_ult_stack(size_t size);

#elif defined(OS_CYGWIN) || defined(OS_WINDOWS)

#include <windows.h>
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0400
#	if defined(__MINGW32__)
#		include <w32api.h>
#		if __W32API_MAJOR_VERSION > 3 || __W32API_MAJOR_VERSION == 3 && __W32API_MINOR_VERSION >= 6
#			define _WIN32_XP
#		endif
#	else
#		define _WIN32_XP
#	endif
#endif

/// This structure is used to maintain execution context for LPs' userspace threads
struct __execution_context_t {
	void *jb;
};

typedef struct __execution_context_t LP_context_t;
typedef struct __execution_context_t kernel_context_t;

/// Swicth machine context for userspace context switch. This is used to schedule a LP or return control to simulation kernel
#define context_switch(context_old, context_new)			\
				do {					\
					(void)context_old.jb;		\
					SwitchToFiber(context_new.jb);	\
				} while (0)

// On Windows/Cygwin we use fibers, so there is no need to allocate LP's stacks
#define get_ult_stack(lid, size) NULL
#define context_save(context) {}
#define context_restore(context) {}

// This is a function which creates a new fiber when running on Windows
extern void context_create(LP_context_t * context, void (*entry_point)(void *), void *args, void *stack, size_t stack_size);

#endif /* OS */

// This is the current KLT main execution context
extern __thread kernel_context_t kernel_context;
