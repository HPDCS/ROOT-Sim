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
* @file queues.h
* @brief Event and State queueing Subsystem main header
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/16/2011
*
*/

#pragma once
#ifndef _QUEUES_H
#define _QUEUES_H

#include <core/core.h>

extern inline simtime_t get_min_in_transit(void);
extern simtime_t last_event_timestamp(LID_t);
extern simtime_t next_event_timestamp(LID_t);
extern msg_t *advance_to_next_event(LID_t);
extern void insert_bottom_half(msg_t *msg);
extern void process_bottom_halves(void);
extern unsigned long long generate_mark(LID_t);

#endif /* _QUEUES_H */
