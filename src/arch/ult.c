/**
*                       Copyright (C) 2008-2018 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file ult.c
* @brief The User-Level Thread module allows the creation/scheduling of a user-level thread
* 	in an architecture-dependent way.
* @author Alessandro Pellegrini
*/


#ifdef ENABLE_ULT



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arch/ult.h>
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
void *get_ult_stack(size_t size) {
	void *stack;
	size_t reminder;

	// Sanity check
	if (size <= 0) {
		size = LP_STACK_SIZE;
	}

	// Align the size to the page boundary (by increasing the stack size)
	reminder = size % getpagesize();
	if (reminder != 0) {
		size += getpagesize() - reminder;
	}

	// The first call to the LP malloc subsystem gives page-aligned memory
	stack = rsalloc(size);
	if(stack == NULL) {
		rootsim_error(true, "Error allocating LP stack: not enough memory.\n");
	}

	bzero(stack, size);

	return stack;
}


/**
 * This function is called from the assembly _setup_jmp in order to create
 * a fresh stack frame on top of the one which has just been cloned.
 * Additionally, it prepares the ULT to be ready to start executing from the
 * function which has been passed as a parameter when the ULT was created.
 * This entry point function is supposed to be implemented as it
 * never returns!
 */
void context_create_boot(exec_context_t *caller, exec_context_t *creat, void (*fn)(void *), void *args) {

	// Go back where the thread was created, being ready to restart from here when the user thread is scheduled!
	context_switch(creat, caller);

	// Start the ULT with the specified parameters
	fn(args);
	
	abort();
}

#elif defined(OS_WINDOWS) || defined(OS_CYGWIN)

void context_create(LP_context_t *context, void (*entry_point)(void *), void *args, void *stack, size_t stack_size) {

	(void)stack;

	// BUG? was this intended to be used when the contexts were setup by a single thread?
	static bool once = false;

	if(once == false) {
		once = true;
		// Convert the current thread to a fiber. This allows to schedule other fibers.
		kernel_context.jb = ConvertThreadToFiber(NULL);
	}

	// Create a new fiber
	context->jb = CreateFiber(stack_size, (LPFIBER_START_ROUTINE)entry_point, args);
}

#endif /* OS */

#endif /* ENABLE_ULT */

