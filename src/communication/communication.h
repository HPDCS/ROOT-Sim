/**
* @file communication/communication.h
*
* @brief Communication Routines
*
* This file contains all the communication routines, for exchanging
* messages among different logical processes and simulator instances.
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
*/

#pragma once

#include <core/core.h>

/**
 * @brief Slab allocator max message size.
 *
 * This is the size in bytes of a slab from the message slab allocator.
 * If messages are smaller than this size, then message buffers are taken
 * from the slab. Otherwise, the buddy system is queried directly.
 */
#define SLAB_MSG_SIZE		512

/**
 * @brief Simulation Platform Control Messages
 *
 * In some circumstances, ROOT-Sim wants to send to a certain LP a message
 * which should not be actually delivered to the simulation model. These
 * are what are referred to as control messages, and are used to implement
 * some sort of distributed (asynchronous) state changes at specific LPs.
 *
 * To properly manage control messages, they must be declared in this
 * enum. Note that the special @ref MIN_VALUE_CONTROL is used to reserve
 * space for application message codes. Anything below that value is
 * considered to be a model-specific message code.
 */
enum _control_msgs {
	RESERVED_MSG_CODE = 65532,
	TOPOLOGY_UPDATE,		/**< Used by the topology API to convey remote updates on costs/probabilities */
	ABM_UPDATE,			/**< Used by ABM API, right now these are treated as normal positive messages */
	ABM_VISITING,			/**< Used by ABM API, right now these are treated as normal positive messages */
	ABM_LEAVING,			/**< Used by ABM API, right now these are treated as normal positive messages */
	MIN_VALUE_CONTROL = 65537,	///< Separation value between model and platform messages
	RENDEZVOUS_START,		///< ECS protocol: start synchronizing two LPs for a page fault
	RENDEZVOUS_ACK,			///< ECS protocol: the sender LP has been synchronized and is now blocked
	RENDEZVOUS_UNBLOCK,		///< ECS protocol: the destination LP can resume its normal execution
	RENDEZVOUS_ROLLBACK,		///< ECS protocol: an ECS synchronization should be rolled back
	RENDEZVOUS_GET_PAGE,		///< ECS protocol: a remote LP is asked for a certain set of pages
	RENDEZVOUS_GET_PAGE_ACK,	///< ECS protocol: the sender LP is giving a lease on a set of pages
	RENDEZVOUS_PAGE_WRITE_BACK,	///< ECS protocol: modified pages are sent back to the owner LP
	MAX_VALUE_CONTROL		///< Anything after this value is considered as an impossible message
};

/// This macro tells whether a message is a control message, by its type
#define is_control_msg(type)	(type >= MIN_VALUE_CONTROL && type != RENDEZVOUS_START)


/**
 * @brief Internal MPI tags.
 *
 * This @c enum defines the MPI tags which are internally used by the
 * simulation runtime environment to exchange messages which are used
 * to synchronize upon specific activities.
 */
enum _mpi_tags {
	MSG_NEW_GVT = 100,	///< Master notifies the new GVT
	MSG_FINI		///< One rank informs the others that the simulation has to be stopped
};

/**
 * For performance reasons, while executing an event, newly-generated events
 * are packed and pre-buffered in a per-LP data structure defined by the
 * @ref outgoing_t type. This implements a resizable array of pointers
 * to @ref msg_t types. Anytime that an event generates more new events
 * than currently supported, the array is doubled in size.
 *
 * This macro tells the number of pointers to @ref msg_t which is allocated
 * in the resizable array, for each LP, when the simulation starts.
 */
#define INIT_OUTGOING_MSG 8

struct lp_struct;

/**
 * @brief Per-LP buffer of newly-generated events.
 *
 * For performance reasons, while executing an event, newly-generated events
 * are packed and pre-buffered in a per-LP data structure defined by the
 * @ref outgoing_t type. This implements a resizable array of pointers
 * to @ref msg_t types. Anytime that an event generates more new events
 * than currently supported, the array is doubled in size.
 *
 * This structure is used by the communication subsystem to handle
 * outgoing messages. At simulation startup, there is space for at most
 * @ref INIT_OUTGOING_MSG messages.
 *
 * The structure keeps pointers to messages which have been packed using
 * the pack_msg() function. This allows to implement from the beginning
 * a zero-copy message passing policy.
 */
typedef struct _outgoing_t {
	msg_t **outgoing_msgs;		///< Resizable array of message pointers
	unsigned int size;		///< How many events is this currently keeping
	unsigned int max_size;		///< Total space in @ref outgoing_msgs
	simtime_t *min_in_transit;	///< Smallest timestamp of events kept here
} outgoing_t;



extern void ParallelScheduleNewEvent(unsigned int, simtime_t, unsigned int, void *, unsigned int);
extern void communication_init(void);
extern void communication_fini(void);
extern void Send(msg_t * msg);
extern void insert_outgoing_msg(msg_t * msg);
extern void send_outgoing_msgs(struct lp_struct *);
extern void send_antimessages(struct lp_struct *, simtime_t);

extern void msg_hdr_release(msg_hdr_t * msg);
extern msg_t *get_msg_from_slab(struct lp_struct *);
extern msg_hdr_t *get_msg_hdr_from_slab(struct lp_struct *);
extern void pack_msg(msg_t ** msg, GID_t sender, GID_t receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload);
extern void msg_to_hdr(msg_hdr_t * hdr, msg_t * msg);
extern void hdr_to_msg(msg_hdr_t * hdr, msg_t * msg);
extern void msg_release(msg_t * msg);
extern void dump_msg_content(msg_t * msg);


#ifndef NDEBUG
extern void validate_msg(msg_t * msg);
#else
#define validate_msg(msg)
#endif
