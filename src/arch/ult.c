/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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


#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>


static __thread LP_context_t		context_caller;
static __thread volatile sig_atomic_t	context_called;

static __thread LP_context_t		*context_creat;
static __thread void			(*context_creat_func)(void *);
static __thread void			*context_creat_arg;
//~ static sigset_t			context_creat_sigs;


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
void *get_ult_stack(unsigned int lid, size_t size) {
//	int err;
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

/*	err = posix_memalign(&stack, getpagesize(), size);
	if(err == EINVAL) {
		rootsim_error(true, "Error allocating LP stack: invalid alignment.\n");
	}
	if(err == ENOMEM) {
		rootsim_error(true, "Error allocating LP stack: not enough memory.\n");
	}
*/
	// The first call to the LP malloc subsystem gives page-aligned memory
	stack = lp_malloc_unscheduled(lid, size);
	if(stack == NULL) {
		rootsim_error(true, "Error allocating LP stack: not enough memory.\n");
	}

	bzero(stack, size);

	return stack;
}



/**
* This function is called within the already-created user-level thread. The goal of this function is to
* setup a new (clean) frame (we're coming back from a signal handler!) and to prepare the user-level thread
* to jump into its entry point. This entry point is supposed to be implemented as it never returns!
* For further details, refer to the paper:
*
* Ralf S. Engelschall
* "Portable Multithreading: the Signal Stack Trick for User-Space Thread Creation"
* Proceedings of the 2000 USENIX Annual Technical Conference
* June 2000
*
* @author Ralf Engelschall
*/
static void context_create_boot(void) {

	void (*context_start_func)(void *);
	void *context_start_arg;

	//~ sigprocmask(SIG_SETMASK, &context_creat_sigs, NULL);

	context_start_func = context_creat_func;
	context_start_arg = context_creat_arg;

	// Go back where the thread was created, being ready to restart from here when the user thread is scheduled!
	context_switch(context_creat, &context_caller);

	// Magically start the thread
	context_start_func(context_start_arg);

	// you should never reach this!
	assert(0);
}




/**
* This function is executed within a manually-induced signal handler, which allows to create a new execution
* context and set the LP stack. We save the context and then return, to leave the signal scope.
* After the signal handler returns, the context_creat context is restored, so that the final bootstrap function
* is actually executed.
* For further details, refer to the paper:
*
* Ralf S. Engelschall
* "Portable Multithreading: the Signal Stack Trick for User-Space Thread Creation"
* Proceedings of the 2000 USENIX Annual Technical Conference
* June 2000
*
* @author Ralf Engelschall
*/
static void context_create_trampoline(int sig) {

	(void)sig;

	if(context_save(context_creat) == 0) {
		//~ context_called = true;
		return;
	}

	context_create_boot();
}





/**
* This function is executed within a manually-induced signal handler, which allows to create a new execution
* context and set the LP stack. We save the context and then return, to leave the signal scope.
* After the signal handler returns, the context_creat context is restored, so that the final bootstrap function
* is actually executed.
*
* @param context the variable where to store the execution context for the created user-level thread
* @param entry_point the function which must be executed when the newly created thread is first activated
* @param args pointer to arguments to be passed to the thread entry point
* @param stack a pointer to a memory area to be used as LP stack
* @param stack_size size of the memory area to be used as stack
*/
void context_create(LP_context_t *context, void (*entry_point)(void *), void *args, void *stack, size_t stack_size) {

	struct sigaction sa;
	struct sigaction osa;
	struct sigaltstack ss;
	struct sigaltstack oss;
	sigset_t osigs;
	sigset_t sigs;

	//~ sigemptyset(&sigs);
	//~ sigaddset(&sigs, SIGUSR1);
	//~ sigprocmask(SIG_BLOCK, &sigs, &osigs);

	bzero((void *)&sa, sizeof(struct sigaction));
	sa.sa_handler = context_create_trampoline;
	sa.sa_flags = SA_ONSTACK;
	sigfillset(&sa.sa_mask);
	sigdelset(&sa.sa_mask, SIGUSR1);
	sigaction(SIGUSR1, &sa, NULL);

	ss.ss_sp = stack;
	ss.ss_size = stack_size;
	ss.ss_flags = 0;
	sigaltstack(&ss, &oss);

	context_creat = context;
	context_creat_func = entry_point;
	context_creat_arg = args;
	//~ context_creat_sigs = osigs;
	context_called = false;
	raise(SIGUSR1);
	//~ sigfillset(&sigs);
	//~ sigdelset(&sigs, SIGUSR1);
	//~ while(!context_called) {
		//~ sigsuspend(&sigs);
	//~ }

	//~ sigaltstack(NULL, &ss);
	//~ ss.ss_flags = SS_DISABLE;
	//~ sigaltstack(&ss, NULL);
	//~ if(!(oss.ss_flags & SS_DISABLE)) {
		sigaltstack(&oss, NULL);
	//~ }
	//~ sigaction(SIGUSR1, &osa, NULL);
	//~ sigprocmask(SIG_SETMASK, &osigs, NULL);

	context_switch(&context_caller, context);
}


#elif defined(OS_WINDOWS) || defined(OS_CYGWIN)





#if !defined(ARCH_X86) && !defined(ARCH_X86_64)
#error Are you really running Cygwin on non-x86 architecture?! How can I handle this?
#endif




void context_create(LP_context_t *context, void (*entry_point)(void *), void *args, void *stack, size_t stack_size) {

	(void)stack;

	static bool once = false;

	if(once == false) {
		once = true;
		// Convert the current thread to a fiber. This allows to schedule other fibers.
		kernel_context.jb = ConvertThreadToFiber(NULL);
	}

	// Create a new fiber for the LP
	context->jb = CreateFiber(stack_size, (LPFIBER_START_ROUTINE)entry_point, args);
}

#endif /* OS */


#endif /* ENABLE_ULT */


