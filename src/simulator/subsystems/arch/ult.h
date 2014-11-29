/**
*                       Copyright (C) 2008-2014 HPDCS Group
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
* @file ult.h
* @brief The User-Level Thread module allows the creation/scheduling of a user-level thread
* 	in an architecture-dependent way.
* @author Alessandro Pellegrini
*/


#pragma once
#ifndef __ULT_H
#define __ULT_H


#if defined(OS_LINUX)

#include <setjmp.h>

/// This structure is used to maintain execution context for LPs' userspace threads
struct __execution_context_t {
	jmp_buf jb;
};

typedef struct __execution_context_t LP_context_t;
typedef struct __execution_context_t kernel_context_t;



/// Save machine context for userspace context switch. This is used only in initialization.
#define context_save(context) \
		setjmp((context)->jb)


/// Restore machine context for userspace context switch. This is used only in inizialitaion.
#define context_restore(context) \
		longjmp((context)->jb, 1)


/// Swicth machine context for userspace context switch. This is used to schedule a LP or return control to simulation kernel
#define context_switch(context_old, context_new) \
	if(setjmp((context_old)->jb) == 0) \
		longjmp((context_new)->jb, 1)


// Allocate ULT stack (in LP memory)
extern void *get_ult_stack(unsigned int lid, size_t size);

#elif defined(OS_CYGWIN) || defined(OS_WINDOWS) /* OS_LINUX || OS_CYGWIN */

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
#define context_switch(context_old, context_new) \
				do {\
					(void)context_old.jb;\
					SwitchToFiber(context_new.jb);\
				} while (0)


// On Windows/Cygwin we use fibers, so there is no need to allocate LP's stacks
#define get_ult_stack() ()


#endif


// These are the APIs that, independently of the underlying arch, must be exposed by this module
extern void context_create(LP_context_t *context, void (*entry_point)(void *), void *args, void *stack, size_t stack_size);

// This is the current KLT main execution context
extern __thread kernel_context_t kernel_context;

#endif /* #define __ULT_H */
