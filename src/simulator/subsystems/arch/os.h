/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file os.h
* @brief This header implements OS-dependent facilities
* @author Alessandro Pellegrini
* @date 16 Sept 2013
*/



#pragma once
#ifndef __OS_H
#define __OS_H


#if defined(OS_LINUX) || defined(OS_CYGWIN)

#include <unistd.h>
#include <pthread.h>


// Macros to get information about the hosting machine

#define get_cores() (sysconf( _SC_NPROCESSORS_ONLN ))



// How do we identify a thread?
typedef pthread_t tid_t;

/// Spawn a new thread
#define new_thread(entry, arg)	pthread_create(&os_tid, NULL, entry, arg)


#else /* OS_LINUX || OS_CYGWIN */
#error Currently supporting only Linux...
#endif


#endif /* __OS_H */

