/**
* @file queues/queues.h
*
* @brief Message queueing subsystem
*
* This module implements the event/message queues subsystem.
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
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
* @date March 16, 2011
*/

#pragma once

#include <core/core.h>
#include <scheduler/process.h>

#define last_event_timestamp(lp) lvt(lp);

extern inline simtime_t get_min_in_transit(void);
extern simtime_t next_event_timestamp(struct lp_struct *);
extern msg_t *advance_to_next_event(struct lp_struct *);
extern void insert_bottom_half(msg_t * msg);
extern void process_bottom_halves(void);
extern unsigned long long generate_mark(struct lp_struct *);
