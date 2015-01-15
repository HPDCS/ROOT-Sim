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
* @file gvt-old.h
* @brief This module implements the old GVT operations. Currently it's not compiled
*        compiled into the final executable, it's kept only for historical reasons.
*        This file will be removed in future revisions of the simulator.
* @author Alessandro Pellegrini
*/


#pragma once
#ifndef GVT_H
#define GVT_H


#include <ROOT-Sim.h>
#include <core/core.h>
#include <core/timer.h>


// TODO: usato da window.c, vedere per cosa
#define GVT_ACK_TIME_PERIOD		1000 // in milliseconds


/// Number of GVT messages that the simulation platform can buffer
#define MAX_GVT_BUFFERED_MSG 	100000


/// Structure to communicate information related to the GVT computation
struct _gvt_info_type {
	int kernel_sender;
	int messages_sent[N_KER_MAX];
	int messages_rcvd[N_KER_MAX];
	simtime_t min_ts;
};

typedef struct _gvt_info_type gvt_info_type;


/// Structure to communicate information related to the buffered messages
typedef struct _gvt_buffered_msg{
	msg_t buffered_msgs[MAX_GVT_BUFFERED_MSG];
	int number_buffered_msgs;
	int first_buffered_msg;
} gvt_buffered_msg;


extern void gvt_init(void);
extern void gvt_fini(void);
extern simtime_t gvt_operations(void);
extern void update_in_transit(unsigned int);
extern void update_bound(simtime_t);

// TODO: vedere se questi si possono togliere qui, sono usati da window.c
extern int *gvt_messages_sent;
extern int *gvt_messages_rcvd;


#endif
