/**
 * @file arch/ult.c
 *
 * @brief User-Level Threads
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <arch/ult.h>
#include <core/core.h>
#include <scheduler/process.h>
#include <mm/dymelor.h>

/// This is the execution context of the simulation kernel
__thread kernel_context_t kernel_context;

#if defined(OS_LINUX)

/**
* When this function is called, a zeroed page-aligned stack for the ULT is created and returned.
* The size of the stack can be specified by using the parameter. It's suggested to give a stack
* size which is a multiple of the page size. Nevertheless, no check is done by this function.
*
* @author Alessandro Pellegrini
*
* @param size The size of the requested stack
* @return A pointer to the allocated and zeroed page-aligned stack
*/
void *get_ult_stack(size_t size)
{
	void *stack;
	size_t reminder;
	size_t page_size;

	// Sanity check
	if (unlikely(size <= 0)) {
		size = LP_STACK_SIZE;
	}
	// Align the size to the page boundary (by increasing the stack size)
	page_size = getpagesize();
	reminder = size % page_size;
	if (unlikely(reminder != 0)) {
		size += page_size - reminder;
	}
	// The first call to the LP malloc subsystem gives page-aligned memory
	stack = rsalloc(size);
	if (stack == NULL) {
		rootsim_error(true,
			      "Error allocating LP stack: not enough memory.\n");
	}

	bzero(stack, size);

	return stack;
}

#elif defined(OS_WINDOWS) || defined(OS_CYGWIN)

void context_create(LP_context_t * context, void (*entry_point)(void *), void *args, void *stack, size_t stack_size)
{

	 (void)stack;

	// BUG? was this intended to be used when the contexts were setup by a single thread?
	static bool once = false;

	if (unlikely(once == false)) {
		once = true;
		// Convert the current thread to a fiber. This allows to schedule other fibers.
		kernel_context.jb = ConvertThreadToFiber(NULL);
	}
	// Create a new fiber
	context->jb = CreateFiber(stack_size, (LPFIBER_START_ROUTINE) entry_point, args);
}

#endif /* OS */
