/**
* @file communication/wnd.c
*
* @brief Message delivery support
*
* This module implements the message delivery support functions.
* Message delivery is carried out asynchronously via MPI, therefore
* we need a way to let MPI notify us when a certain message has been
* delivered to the destination simulation kernel and is now in charge of it.
*
* This module implements a set of per-remote-kernel queues in which each
* remote message is registered before an Isend operation is initiated.
*
* MPI will eventually notify us of the delivery of that message. By
* scanning the outgoing queues, we can know what messages have reached
* destination and remove the corresponding entry.
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
* @author Tommaso Tocci
*/

#ifdef HAVE_MPI

#include <communication/communication.h>
#include <communication/wnd.h>
#include <communication/mpi.h>

/**
 * Outgoing queues are used to keep track, at the MPI level, of what
 * messages have been scheduled for asynchronous delivery through MPI.
 * These queues are used to let MPI notify ROOT-Sim that a certain
 * message has been delivered to the destination simulation kernel, and
 * it is now in charge of its management.
 *
 * We keep one outgoing queue for each remote kernel instance.
 */
static outgoing_queue *outgoing_queues;

/// The number of outgoing queues which are managed by the submodule
static int n_queues = 0;


/**
 * @brief Compute the size of all outgoing queues
 *
 * This function determines the total size (in number of managed nodes)
 * of the local outgoing queues.
 *
 * @return The total number of elements in all the local outgoing queues
 */
static size_t outgoing_queues_size(void)
{
	int i;
	size_t size = 0;
	for (i = 0; i < n_queues; i++) {
		size += list_sizeof(outgoing_queues[i].queue);
	}
	return size;
}


/**
 * @brief Outgoing queue initialization
 *
 * This function is called at simulation startup, and initializes the
 * outgoing queue subsystem, to keep track of the delivery of messages
 * to remote simulation kernel instances.
 */
void outgoing_window_init(void)
{
	n_queues = n_ker;
	outgoing_queues = rsalloc(n_queues * sizeof(outgoing_queue));
	outgoing_queue *oq;
	int i;
	for (i = 0; i < n_queues; i++) {
		oq = outgoing_queues + i;
		spinlock_init(&(oq->lock));
		oq->queue = new_list(outgoing_msg);
	}
}


/**
 * @brief Finalize the message delivery subsystem
 *
 * At simulation shutdown, this function is invoked to release all
 * the datastructures which have been used to keep track of remote
 * message delivery.
 */
void outgoing_window_finalize(void)
{
	prune_outgoing_queues();
	size_t pending_out_msgs = outgoing_queues_size();

	if (unlikely(pending_out_msgs > 0)) {
		rootsim_error(true, "Outgoing queues not empty on exit: %zu\n", outgoing_queues_size());
	}

	// TODO: release also lists allocated via new_list in outgoing_window_init()
	rsfree(outgoing_queues);
}


/**
 * @brief Allocate a buffer for an outgoing message node
 *
 * This function allocates a buffer to keep track of one message
 * which is being remotely sent through MPI
 *
 * @return a pointer to a buffer keeping an @ref outgoing_msg to
 *         be populated before linking to an outgoing queue
 */
outgoing_msg *allocate_outgoing_msg(void)
{
	return rsalloc(sizeof(outgoing_msg));
}


/**
 * @brief Check if a message has been delivered
 * 
 * Given a message node in an outgoing queue, this function determines
 * whether the corresponding MPI Request tells that the operation is
 * comple, therefore meaning that the message has been delivered to
 * the destination simulation kernel instance.
 *
 * @param msg A pointer to an @ref outgoing_msg to check for delivery
 *
 * @return @c true if the message has been delivered, @c false otherwise
 */
static inline bool is_msg_delivered(outgoing_msg *msg)
{
	return is_request_completed(&(msg->req));
}


/**
 * @brief Store an outgoing message
 *
 * Given an @ref outgoing_msg node and a destination kernel id, this
 * function properly links the node in @p out_msg to the proper
 * outgoing queue associated with the destination kernel.
 *
 * @note This function is thread safe
 *
 * @param out_msg A pointer to an @ref outgoing_msg node
 * @param dest_kid The global ID of the simulation kernel instance for
 *                 which this node represents an outgoing message
 */
void store_outgoing_msg(outgoing_msg *out_msg, unsigned int dest_kid)
{
	outgoing_queue *oq = &outgoing_queues[dest_kid];

	spin_lock(&(oq->lock));
	list_insert_tail(oq->queue, out_msg);
	spin_unlock(&(oq->lock));
}


/**
 * @brief Prune an outgoing queue
 *
 * Given an outgoing queue, this function scans through it looking for
 * all messages which have been correctly delivered to their destination.
 *
 * The nodes associated with operations which have completed are removed
 * and freed.
 *
 * @note This function is thread safe
 * 
 * @param oq The output queue to inspect
 */
static int prune_outgoing_queue(outgoing_queue *oq)
{
	int pruned = 0;

	spin_lock(&(oq->lock));

	outgoing_msg *msg = list_head(oq->queue);

	// check all the outgoing messages in the queue starting from the
	// head (the entry with the minimum timestamp) and delete them
	// if they have been already delivered
	while (msg != NULL && is_msg_delivered(msg)) {
		msg_release(msg->msg);

		list_delete_by_content(oq->queue, msg);
		pruned++;
		msg = list_head(oq->queue);
	}

	spin_unlock(&(oq->lock));

	return pruned;
}


/**
 * @brief Prune all outgoing queues
 *
 * This function scans through all the local outgoing queues, looking
 * for messages which have been correctly delivered to the destination.
 *
 * Such message nodes are removed from the queue.
 *
 * Actual queue pruning is performed in prune_outgoing_queue()
 */
int prune_outgoing_queues(void)
{
	int i;
	int pruned = 0;
	for (i = 0; i < n_queues; i++) {
		pruned += prune_outgoing_queue(outgoing_queues + i);
	}
	return pruned;
}

#endif
