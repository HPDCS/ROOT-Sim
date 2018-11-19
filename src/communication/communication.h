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
	RESERVED_MSG_CODE = 65532,
	TOPOLOGY_UPDATE,		/**< Used by the topology API to convey remote updates on costs/probabilities */
	ABM_UPDATE,			/**< Used by ABM API, right now these are treated as normal positive messages */
	ABM_VISITING,			/**< Used by ABM API, right now these are treated as normal positive messages */
	ABM_LEAVING,			/**< Used by ABM API, right now these are treated as normal positive messages */
	MIN_VALUE_CONTROL = 65537,
	RENDEZVOUS_START,
	RENDEZVOUS_ACK,
	RENDEZVOUS_UNBLOCK,
	RENDEZVOUS_ROLLBACK,
	RENDEZVOUS_GET_PAGE,
	RENDEZVOUS_GET_PAGE_ACK,
	RENDEZVOUS_PAGE_WRITE_BACK,
	MAX_VALUE_CONTROL
};

#define is_control_msg(type)	(type >= MIN_VALUE_CONTROL && type != RENDEZVOUS_START)

enum _mpi_msg_code{
// Message Codes for PVM
	MSG_EVENT = 		2,
// Message Codes for GVT operations
	MSG_NEW_GVT = 		52, /**< Master notifies the new GVT */
	MSG_FINI = 		55
};

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
extern void insert_outgoing_msg(msg_t *msg);
extern void send_outgoing_msgs(LID_t);
extern void send_antimessages(LID_t, simtime_t);
extern void communication_fini_thread(void);
extern void communication_init_thread(void);


extern void msg_hdr_release(msg_hdr_t *msg);
extern msg_t *get_msg_from_slab(void);
extern msg_hdr_t *get_msg_hdr_from_slab(void);
extern void pack_msg(msg_t **msg, GID_t sender, GID_t receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload);
extern void msg_to_hdr(msg_hdr_t *hdr, msg_t *msg);
extern void hdr_to_msg(msg_hdr_t *hdr, msg_t *msg);
extern void msg_release(msg_t *msg);
extern void dump_msg_content(msg_t *msg);

#ifndef NDEBUG
extern void validate_msg(msg_t *msg);
#else
#define validate_msg(msg)
#endif

#endif
