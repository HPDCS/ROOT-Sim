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
* @file gvt.h
* @brief This header defines all the Global Virtual Time symbols needed by the platform.
* 	 The current implementation is GVT for non-observable systems based on bottom halves
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date June 14, 2014
*/


#pragma once
#ifndef GVT_H
#define GVT_H

#include <ROOT-Sim.h>
#include <mm/state.h>

#define CANCELBACK_COOLDOWN         2
#define CANCELBACK_MEM_THRESHOLD    0.25
#define RAND() ((double)rand() / (double)RAND_MAX)


/** API from gvt.c */
extern void gvt_init(void);
extern void gvt_fini(void);
extern simtime_t gvt_operations(void);
inline extern simtime_t get_last_gvt(void);

/** API from fossil.c */
extern void adopt_new_gvt(simtime_t, simtime_t);

/** API from cancelback.c */
extern bool attempt_cancelback(simtime_t);
extern bool is_memory_limit_exceeded();
extern inline void synch_for_cancelback();
extern void send_cancelback_messages(unsigned int);
extern bool is_cancelback_started();
extern int num_unprocessed_msgs(unsigned int);
extern inline size_t get_cancelback_threshold();

#endif
