/**
*			Copyright (C) 2008-2017 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file os.h
* @brief This header implements OS-dependent facilities
* @author Alessandro Pellegrini
* @date 16 Sept 2013
*/



#pragma once
#ifndef __OS_H
#define __OS_H


#if defined(OS_LINUX)

#include <sched.h>
#include <unistd.h>
#include <pthread.h>


// Macros to get information about the hosting machine

#define get_cores() (sysconf( _SC_NPROCESSORS_ONLN ))



// How do we identify a thread?
typedef pthread_t tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	pthread_create(&os_tid, NULL, entry, arg)

static inline void set_affinity(int core) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);
	// 0 is the current thread
	sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

#elif defined(OS_WINDOWS)

#include <windows.h>

#define get_cores() ({\
			SYSTEM_INFO _sysinfo;\
			GetSystemInfo( &_sysinfo );\
			_sysinfo.dwNumberOfProcessors;\
			})

// How do we identify a thread?
typedef HANDLE tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	CreateThread(NULL, 0, entry, arg, 0, &os_tid)

#define set_affinity(core) SetThreadAffinityMask(GetCurrentThread(), 1<<core)

#else /* OS_LINUX || OS_WINDOWS */
#error Unsupported operating system
#endif


#endif /* __OS_H */

