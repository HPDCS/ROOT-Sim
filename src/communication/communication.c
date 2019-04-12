/**
* @file communication/communication.c
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

#include <stdlib.h>
#include <float.h>

#include <core/core.h>
#include <gvt/gvt.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <statistics/statistics.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <datatypes/list.h>
#include <mm/mm.h>
#include <arch/atomic.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

/// This is the function pointer to correctly set ScheduleNewEvent API version, depending if we're running serially or in parallel
void (*ScheduleNewEvent)(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

/**
* @brief Initialize the communication subsystem
*
* This function initializes the communication subsystem. It is called
* by the init module upon simulation startup. Any initialization of
* this subsystem should be placed here.
*/
void communication_init(void)
{
#ifdef HAVE_MPI
	inter_kernel_comm_init();
#endif
}


/**
* @brief Finalize the communication subsystem
*
* This function finalizes the communication subsystem. It is called
* by at simulation shutdown, both if the simulation was successful or
* if it failed. This is the place where to cleanly shutdown the
* communication subsystem.
*/
void communication_fini(void)
{
#ifdef HAVE_MPI
	inter_kernel_comm_finalize();
	mpi_finalize();
#endif

	// Release memory used for remaining input/output queues
	foreach_lp(lp) {
		while (!list_empty(lp->queue_in)) {
			list_pop(lp->queue_in);
		}
		while (!list_empty(lp->queue_out)) {
			list_pop(lp->queue_out);
		}
	}
}


/**
* @brief Find a slab to allocate a message buffer
*
* Messages are kept in per-LP memory. This function is used to find out
* from what LP slab a message buffer should be allocated. The reason for
* such a function to exist is because messages can be targeted to local
* or remote LPs, but in both cases we need some memory. Therefore, this
* function takes the GID of two LPs and finds out whether the destination
* LP is local or not. If it is local, then the message will be incorporated
* in some local LP queue, therefore we take memory from there. On the
* other hand, if the LP is remote, it means that we are packing a message
* which will be later passed to MPI for remote transmission---as soon as
* the transmission is completed, that buffer will be released. Therefore,
* in that case, we take the memory from the source LP.
*
* @param sender The GID of the sender LP of a message
* @param receiver the GID of the destination LP of the message
*/
static inline struct lp_struct *which_slab_to_use(GID_t sender, GID_t receiver)
{
	if (find_kernel_by_gid(receiver) == kid)
		return find_lp_by_gid(receiver);
	return find_lp_by_gid(sender);
}


/**
* @brief Release a message header
*
* Message headers are taken always from the sender LP, as they are the
* compact representation of an antimessage. Therefore, the release function
* does not check whether the LP is local or not, but it frees memory
* directly from the sender slab allocator.
*
* @param msg A pointer to the message header to release
*/
void msg_hdr_release(msg_hdr_t *msg)
{
	struct lp_struct *lp;

	lp = find_lp_by_gid(msg->sender);
	slab_free(lp->mm->slab, msg);
}


/**
* @brief Get a buffer to keep a message header
*
* Message headers are the compact way used to represent antimessages. This
* function retrieves a buffer to keep a message header. Antimessages are
* associated with the sender LP, so the @ref lp_struct used here must be
* the one of the sender LP.
*
* @todo This function is hacky: to preserve memory separation, we rely on
*       the LP message slab. Nevertheless, currently we have only a slab
*       for one single size, which is the size of a message. Therefore,
*       we are wasting a lot lot lot of memory here to keep antimessages.
*       A separate per-LP slab allocator, sized to the size of a message
*       header, should be added.
*
* @param lp A pointer to the @ref lp_struct where to take the message header
*           from. The slab allocator of the LP is used.
*
* @return A pointer to the freshly allocated buffer. It is large enough to
*         keep a @ref msg_hdr_t datatype.
*/
msg_hdr_t *get_msg_hdr_from_slab(struct lp_struct *lp)
{
	// TODO: The magnitude of this hack compares to that of the national debt.
	// We are wasting a lot of memory from the LP buddy just to keep antimessages!
	msg_hdr_t *msg = (msg_hdr_t *) get_msg_from_slab(lp);
	bzero(msg, SLAB_MSG_SIZE);
	return msg;
}


/**
* @brief Get a buffer to keep a message.
*
* This function allocates a buffer from the slab of the LP identified by
* the specified @ref lp_struct to keep a message.
*
* @warning The slab allocator is configured at simulation startup to keep
*          buffers of size @ref SLAB_MSG_SIZE. The type @ref msg_t uses
*          a flexible array (the @c event_content member) to keep also
*          the model-specific payload. Therefore, if the size of the payload
*          is such that @c sizeof(msg_t)+payload is larger that @ref SLAB_MSG_SIZE,
*          relying on this function to allocate a @ref msg_t will generate
*          a memory overflow. @b ALWAYS check the size of the payload before
*          getting a message buffer from here!
*
* @param lp A pointer to the @ref lp_struct where to take the message
*           buffer from. The slab allocator of the LP is used.
*
* @return A pointer to the freshly allocated buffer. It is large enough to
*         keep a @ref msg_t datatype, but it might be too small to also
*         keep the event payload.
*/
msg_t *get_msg_from_slab(struct lp_struct *lp)
{
	msg_t *msg = (msg_t *) slab_alloc(lp->mm->slab);
	bzero(msg, SLAB_MSG_SIZE);
	return msg;
}


/**
 * @brief Release a message buffer
 *
 * This function releases a message buffer which is no longer needed
 * (i.e., it was keeping a message annihilated by a corresponding
 * antimessage, or a message which is now beyond the commit horizon
 * identified by the GVT).
 *
 * To free the message, this function checks againts the total size
 * of the message, considering both the size of the @ref msg_t structure
 * and that of the payload kept in the @c event_content member of @ref msg_t.
 * If the total size is smaller than @ref SLAB_MSG_SIZE, then the message
 * was taken from a slab, otherwise it has been taken by the buddy system.
 * Therefore, we free the buffer from the corresponding data structure.
 *
 * Messages are freed using this function both if they are stable and
 * transient in this simulation instance, i.e. if they were destined
 * for a local LP or if they were temporarily allocated here to be
 * transmitted to a remote rank using MPI. Therefore, the function
 * which_slab_to_use() is queried to find out the proper slab to use
 * for deallocating the buffer.
 *
 * @param msg A pointer to the message buffer to release.
 */
void msg_release(msg_t *msg)
{
	struct lp_struct *lp;

	if (likely(sizeof(msg_t) + msg->size <= SLAB_MSG_SIZE)) {
		lp = which_slab_to_use(msg->sender, msg->receiver);
		slab_free(lp->mm->slab, msg);
	} else {
		rsfree(msg);
	}
}


/**
 * @brief Schedule a new message to some LP
 *
 * This is one of the entry points from the application model, used in
 * parallel/distributed simulations. The simulation model calls
 * ScheduleNewEvent() which is a function pointer, set to point to this
 * implementation if the @c --sequential flag is not passed as an option.
 *
 * This function performs all the required sanity checks:
 * * Is the destination LP id valid?
 * * Are we sending an event to the past?
 * * Is the event type in a valid range?
 *
 * If all the checks pass, then the event content is copied in a platform-level
 * buffer and a pointer to it is placed in the temporary LP outgoing buffer,
 * for later delivery (possibly via MPI).
 *
 * If the LP is running in silent execution, this function simply returns
 * as the event has already been sent during a previous event execution.
 * 
 * @param gid_receiver Global id of logical process at which the message must be delivered
 * @param timestamp Logical Virtual Time associated with the event enveloped into the message
 * @param event_type Type of the event
 * @param event_content Payload of the event
 * @param event_size Size of event's payload
 */
void ParallelScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size)
{
	msg_t *event;
	GID_t receiver;

	switch_to_platform_mode();

	// Internally to the platform, the receiver is a GID, while models
	// have no difference across GIDs and LIDs. We convert here the passed
	// id to a GID.
	set_gid(receiver, gid_receiver);

	// In Silent execution, we do not send again already sent messages
	if (unlikely(current->state == LP_STATE_SILENT_EXEC)) {
		return;
	}
	// Check whether the destination LP is out of range
	if (unlikely(gid_receiver > n_prc_tot - 1)) {	// It's unsigned, so no need to check whether it's < 0
		rootsim_error(false, "Warning: the destination LP %u is out of range. The event has been ignored\n", gid_receiver);
		goto out;
	}
	// Check if the associated timestamp is negative
	if (unlikely(timestamp < lvt(current))) {
		rootsim_error(true, "LP %u is trying to generate an event (type %d) to %u in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n",
			      current->gid, event_type, gid_receiver,
			      lvt(current), timestamp);
	}
	// Check if the event type is mapped to an internal control message
	if (unlikely(event_type >= RESERVED_MSG_CODE)) {
		rootsim_error(true, "LP %u is generating an event with type %d which is a reserved type. Switch event type to a value less than %d. Aborting...\n",
			      current->gid, event_type, MIN_VALUE_CONTROL);
	}

	// Copy all the information into the event structure
	pack_msg(&event, current->gid, receiver, event_type, timestamp, lvt(current), event_size, event_content);
	event->mark = generate_mark(current);

	if (unlikely(event->type == RENDEZVOUS_START)) {
		event->rendezvous_mark = current_evt->rendezvous_mark;
		printf("rendezvous_start mark=%llu\n", event->rendezvous_mark);
		fflush(stdout);
	}

	insert_outgoing_msg(event);

 out:
	switch_to_application_mode();
}


/**
 * @brief Send all antimessages for a certain LP
 *
 * This function send all the antimessages for a certain LP, provided
 * a simulation time (which is associated with the time at which
 * we are rolling back.
 *
 * After that the antimessage is sent, the header is immediately removed
 * from the output queue, as MPI guarantees that the antimessage is
 * eventually received at the destination.
 *
 * @param lp A pointer to the LP lp_struct for which antimessages should be sent
 * @param after_simtime The simulation time instant after which to send antimessages
 */
void send_antimessages(struct lp_struct *lp, simtime_t after_simtime)
{
	msg_hdr_t *anti_msg, *anti_msg_prev;
	msg_t *msg;

	if (unlikely(list_empty(lp->queue_out)))
		return;

	// Scan the output queue backwards, sending all required antimessages
	anti_msg = list_tail(lp->queue_out);
	while (anti_msg != NULL && anti_msg->send_time > after_simtime) {
		msg = get_msg_from_slab(which_slab_to_use(anti_msg->sender, anti_msg->receiver));
		hdr_to_msg(anti_msg, msg);
		msg->message_kind = negative;

		Send(msg);

		// Remove the already-sent antimessage from the output queue
		anti_msg_prev = list_prev(anti_msg);
		list_delete_by_content(lp->queue_out, anti_msg);
		msg_hdr_release(anti_msg);
		anti_msg = anti_msg_prev;
	}
}



/**
 * @brief Send a message
 *
 * This function sends a message. This is the decision point where a
 * message receiver is checked to understand whether it must be sent using
 * MPI, or if it is heading towards a local LP and therefore it can be
 * placed in the bottom half buffer.
 *
 * This function is therefore a uniform internal API function to implement
 * message passing in a parallel/distributed simulation environment.
 *
 * @param msg A pointer to the message to send
 */
void Send(msg_t *msg)
{
	validate_msg(msg);

#ifdef HAVE_MPI
	// Check whether the message recepient kernel is remote
	if (find_kernel_by_gid(msg->receiver) != kid) {
		send_remote_msg(msg);
		return;
	}
#endif
	insert_bottom_half(msg);
}


/**
 * @brief Place a message in the temporary LP outgoing buffer
 *
 * To quickly finish the execution of events, once a simulation model
 * calls ScheduleNewEvent(), the event is not actually immediately sent.
 * On the other hand, the message is packed and placed in a temporary
 * output queue. Once the event's execution is completed, this queue
 * is scanned to send out all the generated events.
 *
 * This function places a newly-scheduled event into this temporary queue,
 * which is implemented as a resizable array of pointers to message buffers.
 *
 * @param msg The packed message to insert in the temporary outgoing queue.
 */
void insert_outgoing_msg(msg_t *msg)
{

	// if the model is generating many events at the same time, reallocate the outgoing buffer
	if (unlikely(current->outgoing_buffer.size == current->outgoing_buffer.max_size)) {
		current->outgoing_buffer.max_size *= 2;
		current->outgoing_buffer.outgoing_msgs = rsrealloc(current->outgoing_buffer.outgoing_msgs, sizeof(msg_t *) * current->outgoing_buffer.max_size);
	}

	current->outgoing_buffer.outgoing_msgs[current->outgoing_buffer.size++] = msg;

	// Store the minimum timestamp of outgoing messages
	// TODO: check whether this is still used by preemptive Time Warp or not
	if (msg->timestamp < current->outgoing_buffer.min_in_transit[current->worker_thread]) {
		current->outgoing_buffer.min_in_transit[current->worker_thread] = msg->timestamp;
	}
}


/**
 * @brief Send all pending outgoing messages
 *
 * This function sends all messages registered in the outgoing message
 * queue during the execution of an event (see insert_outgoing_msg())
 * to the destination LPs. Also, this function records in the output
 * queue of the sender LP that at a certain simulation time a certain
 * message was sent---this is done using the @ref msg_hdr_t type.
 * This information is used upon a rollback to send out antimessages.
 *
 * After the execution of this function, the temporary outgoing queue
 * is considered as empty.
 *
 * @param lp A pointer to the LP's @ref lp_struct for which we want to
 *           finalize the event send operation.
 */
void send_outgoing_msgs(struct lp_struct *lp)
{
	register unsigned int i = 0;
	msg_t *msg;
	msg_hdr_t *msg_hdr;

	for (i = 0; i < lp->outgoing_buffer.size; i++) {
		msg_hdr = get_msg_hdr_from_slab(lp);
		msg = lp->outgoing_buffer.outgoing_msgs[i];
		msg_to_hdr(msg_hdr, msg);

		Send(msg);

		// register the message in the sender's output queue, for antimessage management
		list_insert(lp->queue_out, send_time, msg_hdr);
	}

	lp->outgoing_buffer.size = 0;
}


/**
 * @brief Pack a message in a platform-level data structure
 *
 * This function takes all the parameters which represent a model-level
 * event and pack it in a simulation-level datastructure representing
 * a message (namely, a @ref msg_t type).
 *
 * This function also allocates the buffer for that message. To this end,
 * it determines whether the buffer can be taken from some slab allocator
 * or not (depending on the size of the payload, which determines whether
 * the final message fits into a buffer of size @ref SLAB_MSG_SIZE).
 *
 * This is a uniform internal API which can be used in any situation.
 * Indeed, it relies on the which_slab_to_use() internal function to find
 * out whether this message will be kept in the local instance of a
 * distributed simulation or not.
 *
 * @param msg A double pointer to a @ref msg_t type. Since this function
 *            allocates the buffer, a pointer to a @c msg_t @c * datatype
 *            should be passed, in order for the caller to receive the
 *            pointer to the message.
 * @param sender The GID of the sender
 * @param receiver The GID of the receiver
 * @param type A numerical code identifying the event type. This can be a
 *            model-specific type, or a platform-level code used to
 *            identify a control message.
 * @param timestamp This is the simulation time at which the destination LP
 *            will have to execute this event.
 * @param send_time This event has been sent by @p sender at this particular
 *            simulation time. This information is used to handle
 *            antimessages upon a rollback operation.
 * @param size The size of the model-specific payload.
 * @param payload A pointer to the model-specific payload. It can be
 *            a pointer to whatever, e.g. the stack of a ULT in which
 *            the LP is running, or a data structure in the simulation
 *            state of the LP. This is not a problem because we make
 *            a full copy of the event payload. Problems might arise
 *            if a pointer is present in the payload, and the ECS
 *            subsystem is not running, but at that point it is the
 *            simulation model's responsibility to make the simulation
 *            inconsistent or to crash the run.
 */
void pack_msg(msg_t **msg, GID_t sender, GID_t receiver, int type, simtime_t timestamp, simtime_t send_time, size_t size, void *payload)
{
	// Check if we can rely on a slab to initialize the message
	if (likely(sizeof(msg_t) + size <= SLAB_MSG_SIZE)) {
		*msg = get_msg_from_slab(which_slab_to_use(sender, receiver));
	} else {
		*msg = rsalloc(sizeof(msg_t) + size);
		bzero(*msg, sizeof(msg_t) + size);
	}

	(*msg)->sender = sender;
	(*msg)->receiver = receiver;
	(*msg)->type = type;
	(*msg)->message_kind = positive;
	(*msg)->timestamp = timestamp;
	(*msg)->send_time = send_time;
	(*msg)->size = size;
	// TODO: si può generare qua dentro la marca, perché si usa sempre il sender. Occhio al gid/lid!!!!

	if (payload != NULL && size > 0)
		memcpy((*msg)->event_content, payload, size);
}


/**
 * @brief Convert a message to a message header
 *
 * This function takes an already packed message pointed by @p msg
 * and populates the relevant fields of the message header pointed by
 * @p hdr to create a compact representation of the message being sent
 * out. This is necessary to later send antimessages, upon a rollback
 * operation.
 *
 * @param hdr A pointer to a @ref msg_hdr_t where to store the relevant
 *            information to represent an antimessage.
 * @param msg A pointer to an already-packed message from which to take
 *            the relevant information to populate the header
 */
void msg_to_hdr(msg_hdr_t *hdr, msg_t *msg)
{
	validate_msg(msg);

	hdr->sender = msg->sender;
	hdr->receiver = msg->receiver;
	hdr->type = msg->type;
	hdr->rendezvous_mark = msg->rendezvous_mark;
	hdr->timestamp = msg->timestamp;
	hdr->send_time = msg->send_time;
	hdr->mark = msg->mark;
}


/**
 * @brief convert a message header into a message
 *
 * This is a commodity function which prepares a message data structure
 * from a message header. Both the header and the message buffers must
 * be already allocated.
 *
 * The purpose of this function is to prepare the sending of an antimessage.
 * Indeed, an antimessage is sent as a message of size zero, and the
 * information is taken from compact versions of the originally sent
 * messages, kept in the @ref msg_hdr_t type. When an antimessage must
 * be sent out, this is done by copying the message header into a
 * @ref msg_t type.
 * This is required because all message sending logic assumes that
 * a @ref msg_t data structure is being passed (this avoid having to
 * perform multiple checks or multiple casts in the code base).
 *
 * @param hdr A pointer to the message header from which the message
 *            information is taken.
 * @param msg A pointer to the message where the header information is
 *            copied.
 */
void hdr_to_msg(msg_hdr_t *hdr, msg_t *msg)
{
	msg->sender = hdr->sender;
	msg->receiver = hdr->receiver;
	msg->type = hdr->type;
	msg->rendezvous_mark = hdr->rendezvous_mark;
	msg->timestamp = hdr->timestamp;
	msg->send_time = hdr->send_time;
	msg->mark = hdr->mark;
}


/**
 * @brief Dump the content of a message
 *
 * This function dumps the content of a message. This is mainly used
 * when some runtime error is encountered, to provide on screen information
 * which might be used for debugging what is going on.
 *
 * @param msg A pointer to the message to dump on screen.
 */
void dump_msg_content(msg_t *msg)
{
	printf("\tsender: %u\n", msg->sender.to_int);
	printf("\treceiver: %u\n", msg->sender.to_int);
#ifdef HAVE_MPI
	printf("\tcolour: %d\n", msg->colour);
#endif
	printf("\ttype: %d\n", msg->type);
	printf("\tmessage_kind: %d\n", msg->message_kind);
	printf("\ttimestamp: %f\n", msg->timestamp);
	printf("\tsend_time: %f\n", msg->send_time);
	printf("\tmark: %llu\n", msg->mark);
	printf("\trendezvous_mark: %llu\n", msg->rendezvous_mark);
	printf("\tsize: %d\n", msg->size);
}



#ifndef NDEBUG

/**
 * @brief Tell the GID of the sender of a message, given its mark
 *
 * This function inverts the Cantor pairing function used to generate
 * unique message marks. It can be used to perform sanity checks on
 * the marks, to see whether they are correct or corrupted. Also,
 * it can assist in debugging errors in the management of messages.
 *
 * @warning This function is computationally costly! @b never @b ever
 *          use it in a production environment. The @c NDEBUG guard
 *          ensures that it is never compiled in a final version of the
 *          runtime environment, so keep it only as a debugging function.
 *
 * @param mark The mark to invert.
 *
 * @return The GID of the sender of the message stamped with the @p mark.
 *         The GID is not actually represented as a @ref GID_t, rather
 *         as an @c int.
 */
unsigned int mark_to_gid(unsigned long long mark)
{
	double z = (double)mark;
	double w = floor((sqrt(8 * z + 1) - 1) / 2.0);
	double t = (w * w + w) / 2.0;
	double y = z - t;
	double x = w - y;

	return (int)x;
}


/**
 * @brief Perform some sanity checks on a message buffer
 *
 * This is a debugging function which performs some sanity checks on a
 * message buffer, and aborts the simulation if these checks do not pass.
 *
 * The checks performed are:
 * * Is the sender LP GID in a valid range?
 * * Is the destination LP GID in a valid range?
 * * Is the message kind of a valid type?
 * * Is the sender associated with the message mark in a valid range?
 * * Is the sender associated with a rendezvous mark in a valid range?
 * * Is the message type in a valid range?
 *
 * If a message is corrupted due to any reason, the likelihood that this
 * function spots the corruption is very high.
 *
 * @warning This function is computationally costly! @b never @b ever
 *          use it in a production environment. The @c NDEBUG guard
 *          ensures that it is never compiled in a final version of the
 *          runtime environment, so keep it only as a debugging function.
 * 
 * @param msg A pointer to the message to validate
 */
void validate_msg(msg_t *msg)
{
	assert(msg->sender.to_int <= n_prc_tot);
	assert(msg->receiver.to_int <= n_prc_tot);
	assert(msg->message_kind == positive || msg->message_kind == negative || msg->message_kind == control);
	assert(mark_to_gid(msg->mark) <= n_prc_tot);
	assert(mark_to_gid(msg->rendezvous_mark) <= n_prc_tot);
	assert(msg->type < MAX_VALUE_CONTROL);
}
#endif
