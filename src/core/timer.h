/**
* @file core/timer.h
*
* @brief Timers
*
* This header defines the timers which the simulator uses to monitor its
* internal behaviour
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
* @author Andrea Santoro
* @author Alessandro Pellegrini
*/

#pragma once

#include <time.h>
#include <sys/time.h>

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

#define TIMER_BUFFER_LEN 64
/// string must be a char array of at least TIMER_BUFFER_LEN bytes to keep the whole string
#define timer_tostring(timer_name, string) do {\
					time_t __nowtime;\
					struct tm *__nowtm;\
					__nowtime = (timer_name).tv_sec;\
					__nowtm = localtime(&__nowtime);\
					strftime((string), sizeof (string), "%Y-%m-%d %H:%M:%S", __nowtm);\
				} while(0)
