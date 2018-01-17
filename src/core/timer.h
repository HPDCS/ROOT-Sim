/**
*			Copyright (C) 2008-2018 HPDCS Group
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
* @file timer.h
* @brief This header defines the timers which the simulator uses to monitor its internal behaviour
* @author Andrea Santoro
* @author Alessandro Pellegrini
*/

#pragma once
#ifndef _TIMER_H
#define _TIMER_H

#include <time.h>
#include <sys/time.h>

/* gettimeofday() timers  - TODO: SHOULD BE DEPRECATED */
typedef struct timeval timer;


#define timer_start(timer_name) gettimeofday(&timer_name, NULL)

#define timer_restart(timer_name) timer_start(timer_name)

#define timer_value_seconds(timer_name) ((double)timer_value_milli(timer_name) / 1000.0)

#define timer_value_milli(timer_name) ({\
					struct timeval __rs_tmp_timer;\
					int __rs_timedif;\
					gettimeofday(&__rs_tmp_timer, NULL);\
					__rs_timedif = __rs_tmp_timer.tv_sec * 1000 + __rs_tmp_timer.tv_usec / 1000;\
					__rs_timedif -= timer_name.tv_sec * 1000 + timer_name.tv_usec / 1000;\
					__rs_timedif;\
				})

#define timer_value_micro(timer_name) ({\
					struct timeval __rs_tmp_timer;\
					int __rs_timedif;\
				        gettimeofday(&__rs_tmp_timer, NULL);\
					__rs_timedif = __rs_tmp_timer.tv_sec * 1000000 + __rs_tmp_timer.tv_usec;\
					__rs_timedif -= timer_name.tv_sec * 1000000 + timer_name.tv_usec;\
					__rs_timedif;\
				})

/// string must be a char array of at least 64 bytes to keep the whole string
#define timer_tostring(timer_name, string) do {\
					time_t __nowtime;\
					struct tm *__nowtm;\
					__nowtime = timer_name.tv_sec;\
					__nowtm = localtime(&__nowtime);\
					strftime(string, sizeof string, "%Y-%m-%d %H:%M:%S", __nowtm);\
				} while(0)





/// This overflows if the machine is not restarted in about 50-100 years (on 64 bits archs)
#ifdef HAVE_RDTSC
#define CLOCK_READ() ({ \
			unsigned int lo; \
			unsigned int hi; \
			__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
			((unsigned long long)hi) << 32 | lo; \
			})
#else
#define CLOCK_READ() (unsigned long long)clock()
#endif

#endif /* _TIMER_H */

