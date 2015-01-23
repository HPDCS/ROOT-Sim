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
* @file queues.h
* @brief Event and State queueing Subsystem main header
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/16/2011
*
*/

#pragma once
#ifndef _QUEUE_MGNT_H_
#define _QUEUE_MGNT_H_




#define QUEUE_IN	0
#define QUEUE_OUT	1


extern simtime_t last_event_timestamp(unsigned int);
extern simtime_t next_event_timestamp(unsigned int);
extern msg_t *advance_to_next_event(unsigned int);
extern msg_t *get_last_event(unsigned int);
extern void clean_queue_in(unsigned int, simtime_t);
extern void clean_queue_out(unsigned int, simtime_t);
extern simtime_t last_event_to_execute(msg_t *, simtime_t);
extern msg_t *free_queue_out_elem(unsigned int, msg_t *);

extern void insert_bottom_half(msg_t *msg);
extern void process_bottom_halves(void);
extern unsigned long long generate_mark(unsigned int);

#ifdef TRACE_INPUT_QUEUE
#define trace_input_queue(lid) __trace_input_queue(__FILE__, __LINE__, lid)
extern void __trace_input_queue(char *, int, unsigned int);
#endif

#endif

