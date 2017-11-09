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
* @file communication.h
* @brief This is the main header file, containing all data structures and defines
*        used by the communication subsystem.
* @author Francesco Quaglia
* @author Roberto Vitali
*
* @todo There are still some defines undocumented
*/

#pragma once
#ifndef _COMMUNICATION_H_
#define _COMMUNICATION_H_

#include <core/core.h>


/// Ack window size
#define WND_BUFFER_LENGTH	10000
/// Ack window size
#define WINDOW_DIMENSION	50


#define SLAB_MSG_SIZE		512



/// Number of slots used by MPI for buffering messages
#define SLOTS	100000


/// Simulation Platform Control Messages
enum _control_msgs {
	MIN_VALUE_CONTROL=65537,
	RENDEZVOUS_START,
	RENDEZVOUS_ACK,
	RENDEZVOUS_UNBLOCK,
	RENDEZVOUS_ROLLBACK,
	RENDEZVOUS_GET_PAGE,
	RENDEZVOUS_GET_PAGE_ACK,
	RENDEZVOUS_PAGE_WRITE_BACK,
	MAX_VALUE_CONTROL
};

#define count_as_white(type)	(type < MIN_VALUE_CONTROL || type == RENDEZVOUS_START)

// Message Codes for PVM
#define MSG_INIT_MPI		200
#define MSG_EVENT		2
#define MSG_EVENT_LARGER	3
//#define MSG_ACKNOWLEDGE		3
#define MSG_GVT			10
#define MSG_UNLOCK		11
#define MSG_GO_TO_FINAL_BARRIER	12
#define MSG_GVT_ACK		13



// Message Codes for GVT operations
#define MSG_COMPUTE_GVT		50 /// Master asks for tables
#define MSG_INFO_GVT		51 /// Slaves reply with their information
#define MSG_NEW_GVT		52 /// Master notifies the new GVT
#define MSG_TIME_BARRIER	53 /// Slaves communicate their maximum time barrier
#define MSG_SNAPSHOT		54 /// Retrieve termination result
#define MSG_FINI			55


#define INIT_OUTGOING_MSG	10


/// This structure is used by the communication subsystem to handle outgoing messages
typedef struct _outgoing_t {
	msg_t **outgoing_msgs;
	unsigned int size;
	unsigned int max_size;
	simtime_t *min_in_transit;
} outgoing_t;

extern void ParallelScheduleNewEvent(unsigned int, simtime_t, unsigned int, void *, unsigned int);

/* Functions invoked by other modules */
extern void communication_init(void);
extern void communication_fini(void);
extern int comm_finalize(void);
extern void Send(msg_t *msg);
extern simtime_t receive_time_barrier(simtime_t max);
extern int messages_checking(void);
extern void insert_outgoing_msg(msg_t *msg);
extern void send_outgoing_msgs(unsigned int);
extern void send_antimessages(unsigned int, simtime_t);
extern void communication_fini_thread(void);
extern void communication_init_thread(void);

/* In window.c */
extern void windows_init(void);
extern void register_msg(msg_t *msg);
extern void receive_ack(void);
extern void send_forced_ack(void);
extern simtime_t local_min_timestamp(void);
extern void start_ack_timer(void);


extern msg_t *get_msg_from_slab(void);
extern void pack_msg(msg_t **msg, unsigned int sender, unsigned int receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload);
extern void msg_to_hdr(msg_hdr_t *hdr, msg_t *msg);
extern void hdr_to_msg(msg_hdr_t *hdr, msg_t *msg);
extern void msg_release(msg_t *msg);

#ifndef NDEBUG
extern void validate_msg(msg_t *msg);
#else
#define validate_msg(msg)
#endif

#endif
