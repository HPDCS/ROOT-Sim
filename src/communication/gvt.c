/**
* @file communication/gvt.c
*
* @brief Distributed GVT Support module
*
* This module implements communication primitives to implement the
* asynchronous distributed GVT reduction algorithm described in:
*
* T. Tocci, A. Pellegrini, F. Quaglia, J. Casanovas-García, and T. Suzumura,<br/>
* <em>“ORCHESTRA: An Asynchronous Wait-Free Distributed GVT Algorithm,”</em><br/>
* in Proceedings of the 21st IEEE/ACM International Symposium on Distributed
* Simulation and Real Time Applications<br/>
* 2017
*
* For a full understanding of the code, we encourage reading that paper.
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
#include <communication/mpi.h>
#include <communication/gvt.h>

/**
 * Colour phase of each thread. This is a dynamically sized array
 * which tells the current execution phase of each thread (white or red).
 */
phase_colour *threads_phase_colour;

/// Minimum time among all the outgoing red messages for each thread
simtime_t *min_outgoing_red_msg;

/**
 * There are multiple incarnations of a white phase, namely the white
 * phase @e before a red phase is different from the white phase @e after
 * that same red phase. At the end, we can reduce the multiple incarnations
 * to only two (the third white phase appearing in the execution can
 * be merged with the first, as the second one in between ensures that
 * no pending action on the first phase is present).
 *
 * This aspect is implemented using two different atomic counters of
 * white messages, so that @ref white_msg_recv must always point to the
 * actual counter of white message received by the current kernel
 * that have been sent during the last global white phase.
 * It will point either to @ref white_0_msg_recv or to @ref white_1_msg_recv.
 */
volatile atomic_t *white_msg_recv;

/**
 * Counter of received white messages for the first white phase in a period
 * of three. This is pointed to by @ref white_msg_recv when relating to
 * the current white phase.
 */
static atomic_t white_0_msg_recv;

/**
 * Counter of received white messages for the second white phase in a period
 * of three. This is pointed to by @ref white_msg_recv when relating to
 * the current white phase.
 */
static atomic_t white_1_msg_recv;

/**
 * Number of white message sent from this kernel to all the others
 * during the last white phase
 */
static atomic_t *white_msg_sent;

/// Temporary structure used for the MPI collective primitives
static int *white_msg_sent_buff;

/**
 * Number of expected white message to be received by this kernel
 * before partecipating to the next GVT reduction
 */
static int expected_white_msg;

/**
 * MPI Request operation to implement the asynchronous counting
 * of white messages
 */
static MPI_Request white_count_req;

/**
 * Separate MPI communicator to carry out the reduction of white
 * messages. This communicator lives for the whole duration of the
 * simulation.
 */
static MPI_Comm white_count_comm;

/**
 * Spinlock guard used to ensure that only one thread at a time
 * attempts to perform check operations on the reduction of white
 * messages.
 */
static spinlock_t white_count_lock;

/**
 * MPI Request operation to implement the distributed reduction
 * of the current GVT value.
 */
static MPI_Request gvt_reduction_req;

/**
 * Separate MPI communicator to carry out the reduction of the GVT.
 */
static MPI_Comm gvt_reduction_comm;

/**
 * Spinlock guard used to ensure that only one thread at a time
 * attempts to perform check operations on the reduction of the GVT.
 */
static spinlock_t gvt_reduction_lock;

/**
 * In the end, the GVT reduction is implemented using an MPI All Reduce.
 * This primitive requires some stable buffer in memory to keep the
 * values to be reduced across all ranks. This is a global variable
 * in which each kernel instance places its proposal for the GVT, which
 * is reduced using MPI all reduce.
 */
static simtime_t local_vt_buff;

/**
 * This is the target temporary buffer in which MPI all reduce will
 * place the reduced GVT value.
 */
static simtime_t reduced_gvt;

/**
 * A vector of MPI asynchronous operations used to communicate with all
 * kernel instances in the distributed run in order to initiate the GVT
 * reduction protocol.
 */
static MPI_Request *gvt_init_reqs;

/**
 * This variable is used to keep track of what round of GVT reduction we
 * are actually starting. This is used also to make a sanity check on
 * whether we are starting a new GVT reduction while the previous one
 * is still pending.
 */
static unsigned int gvt_init_round;


/**
 * @brief Initialize the MPI-based distributed GVT reduction submodule.
 *
 * This function is called after that the MPI subsystem is activated.
 * Its goal is to initialize all the variable-sized data structures
 * and MPI-related data structures which will be used while running the
 * distributed GVT reduction protocol.
 */
void gvt_comm_init(void)
{
	unsigned int i;

	min_outgoing_red_msg = rsalloc(n_cores * sizeof(simtime_t));
	threads_phase_colour = rsalloc(n_cores * sizeof(phase_colour));
	for (i = 0; i < n_cores; i++) {
		min_outgoing_red_msg[i] = INFTY;
		threads_phase_colour[i] = white_0;
	}

	atomic_set(&white_0_msg_recv, 0);
	atomic_set(&white_1_msg_recv, 0);
	white_msg_recv = &white_1_msg_recv;
	expected_white_msg = 0;

	white_msg_sent = rsalloc(n_ker * sizeof(atomic_t));
	for (i = 0; i < n_ker; i++) {
		atomic_set(&white_msg_sent[i], 0);
	}

	white_msg_sent_buff = rsalloc(n_ker * sizeof(unsigned int));

	white_count_req = MPI_REQUEST_NULL;
	MPI_Comm_dup(MPI_COMM_WORLD, &white_count_comm);
	spinlock_init(&white_count_lock);

	gvt_init_reqs = rsalloc(n_ker * sizeof(MPI_Request));
	for (i = 0; i < n_ker; i++) {
		gvt_init_reqs[i] = MPI_REQUEST_NULL;
	}

	gvt_reduction_req = MPI_REQUEST_NULL;
	MPI_Comm_dup(MPI_COMM_WORLD, &gvt_reduction_comm);
	spinlock_init(&gvt_reduction_lock);
}


/**
 * @brief Shut down the MPI-based distributed GVT reduction submodule.
 *
 * This function is called just before shutting down the MPI subsystem.
 * It frees all the data structures used to carry out the distributed GVT
 * reduction protocol, and nicely tells MPI to wipe out everything that
 * is no longer needed.
 * Right after this, MPI will be shut down as well.
 */
void gvt_comm_finalize(void)
{
	rsfree(min_outgoing_red_msg);
	rsfree(threads_phase_colour);
	rsfree(white_msg_sent);
	rsfree(white_msg_sent_buff);

	unsigned int i;
	for (i = 0; i < n_ker; i++) {
		if (gvt_init_reqs[i] != MPI_REQUEST_NULL) {
			MPI_Cancel(&gvt_init_reqs[i]);
			MPI_Request_free(&gvt_init_reqs[i]);
		}
	}

	if (gvt_init_pending()) {
		gvt_init_clear();
	}

	MPI_Wait(&white_count_req, MPI_STATUS_IGNORE);
	MPI_Comm_free(&white_count_comm);
	MPI_Wait(&gvt_reduction_req, MPI_STATUS_IGNORE);
	MPI_Comm_free(&gvt_reduction_comm);
	rsfree(gvt_init_reqs);
}


/**
 * @brief Make a thread enter into red phase.
 *
 * Once this function returns, the calling thread will be turned @b red.
 * The minimum timestamp of red messages sent by it will be reset.
 *
 * @warning Calling this function from a thread which is already in the
 *          red phase will cause a sanity check to immediately stop the
 *          simulation, as it will be no longer possible to ensure
 *          consistency in the GVT reduction.
 */
void enter_red_phase(void)
{
	if (unlikely(in_red_phase())) {
		rootsim_error(true, "Thread %u cannot enter in red phase because is already in red phase.\n", local_tid);
	}
	min_outgoing_red_msg[local_tid] = INFTY;
	threads_phase_colour[local_tid] = next_colour(threads_phase_colour[local_tid]);
}


/**
 * @brief Make a thread exit from red phase.
 *
 * Once this function returns, the calling thread will be turned @b white.
 *
 * @warning Calling this function from a thread which is already in the
 *          white phase will cause a sanity check to immediately stop the
 *          simulation, as it will be no longer possible to ensure
 *          consistency in the GVT reduction.
 */
void exit_red_phase(void)
{
	if (unlikely(!in_red_phase())) {
		rootsim_error(true, "Thread %u cannot exit from red phase because it wasn't in red phase.\n", local_tid);
	}
	threads_phase_colour[local_tid] = next_colour(threads_phase_colour[local_tid]);
}


/**
 * @brief Join the white message reduction collective operation.
 *
 * All kernels will share eachother the number of white message sent
 * during the last white phase.
 *
 * This is an asyncronous function and the actual completion of the collective
 * reduction can be tested through the `white_msg_redux_completed()` function.
 *
 * After the collective reduction will be terminated `expected_white_msg`
 * will hold the number of white message sent by all the other kernel to this during
 * the last white phase.
 */
void join_white_msg_redux(void)
{
	unsigned int i;
	for (i = 0; i < n_ker; i++) {
		white_msg_sent_buff[i] = atomic_read(&white_msg_sent[i]);
	}

	lock_mpi();
	MPI_Ireduce_scatter_block(white_msg_sent_buff, &expected_white_msg, 1, MPI_INT, MPI_SUM, white_count_comm, &white_count_req);
	unlock_mpi();
}


/**
 * @brief Test completion of white message reduction collective operation.
 *
 * This function checks whether the asynchronous collective operation
 * which counts whether the expected number of white messages has been
 * received is completed.
 *
 * This can be safely invoked concurrently by multiple worker threads,
 * as a lock guard is used to ensure that only one thread at a time
 * performs the check.
 *
 * @return @c true if the reduction operation is completed,
 *         @c false otherwise.
 */
bool white_msg_redux_completed(void)
{
	if (!spin_trylock(&white_count_lock))
		return false;

	bool compl = false;
	compl = is_request_completed(&white_count_req);

	spin_unlock(&white_count_lock);
	return compl;
}


/**
 * @brief Wait for the completion of wait message reduction.
 *
 * This function returns only when the white message reduction operation
 * currently being carried out is completed.
 *
 * @warning This is a blocking operation.
 */
void wait_white_msg_redux(void)
{
	MPI_Wait(&white_count_req, MPI_STATUS_IGNORE);
}


/**
 * @brief Check if white messages are all received
 *
 * Check if the number of white message received is equal to
 * the expected ones (retrieved through the white message reduction).
 *
 * @return @c true if the expected amount of white messages has bee
 *         already received, @c false otherwise.
 */
bool all_white_msg_received(void)
{
	return (atomic_read(white_msg_recv) == expected_white_msg);
}


/**
 * @brief Reset received white messages.
 *
 * This function is used to prepare for the next white phase of the
 * execution. In particular, after that the correct number of white
 * messages has been received, a call to this function will change the
 * white phase counter pointed by @ref white_msg_recv to either
 * @ref white_0_msg_recv or @ref white_1_msg_recv, depending on the
 * current phase which we are leaving.
 *
 * Furthermore, the white message counter is reset to zero---it will be
 * used in the next GVT round.
 *
 * @warning Calling this function if the correct number of white messages
 *          has been received will stop the simulation due to an inconsistency.
 */
void flush_white_msg_recv(void)
{
	// santy check: Exactly the expected number of white
	// messages should have been arrived in the last GVT round
	if (atomic_read(white_msg_recv) != expected_white_msg) {
		rootsim_error(true, "unexpected number of white messages received in the last GVT round: [expected: %d, received: %d]\n",
			      expected_white_msg, atomic_read(white_msg_recv));
	}
	// prepare the white msg counter for the next GVT round
	atomic_set(white_msg_recv, 0);

	if (white_msg_recv == &white_0_msg_recv) {
		white_msg_recv = &white_1_msg_recv;
	} else {
		white_msg_recv = &white_0_msg_recv;
	}
}


/**
 * @brief Reset sent white messages.
 *
 * This function is used to prepare for the next white phase of the
 * execution. In particular, after that all the threads have entered
 * the red phase (this ensures that white message counter can be
 * safely reset for all threads) the counters keeping the number of
 * white messages sent to other simulation kernel instances are reset.
 *
 * @warning Calling this function if at least one thread is still in the
 *          white phase will stop the simulation due to an inconsistency.
 */
void flush_white_msg_sent(void)
{
	unsigned int i;

	//sanity check
#ifndef NDEBUG
	for (i = 0; i < n_cores; i++) {
		if (!is_red_colour(threads_phase_colour[i]))
			rootsim_error(true, "flushing outgoing white message counter while some thread are not in red phase\n");
	}
#endif

	for (i = 0; i < n_ker; i++) {
		atomic_set(&white_msg_sent[i], 0);
	}
}


/**
 * @brief Initiate a distributed GVT.
 *
 * A call to this function sends to all kernel instances an asynchronous
 * request to initiate the distributed protocol for GVT reduction.
 *
 * If must be called by one single thread, and a consistent round value
 * should be passed as an argument.
 *
 * @param round A counter telling what is the GVT round that we want to
 *              initiate. @p round must be strictly monotonic.
 */
void broadcast_gvt_init(unsigned int round)
{
	unsigned int i;

	gvt_init_round = round;

	for (i = 0; i < n_ker; i++) {
		if (i == kid)
			continue;
		if (!is_request_completed(&(gvt_init_reqs[i]))) {
			rootsim_error(true, "Failed to send new GVT init round to kernel %u, because the old init request is still pending\n", i);
		}
		lock_mpi();
		MPI_Isend((const void *)&gvt_init_round, 1, MPI_UNSIGNED, i, MSG_NEW_GVT, MPI_COMM_WORLD, &gvt_init_reqs[i]);
		unlock_mpi();
	}
}


/**
 * @brief Check if there are pending GVT-init messages around.
 *
 * This function tells whether some kernel is still waiting for some GVT
 * message to be sent around, related to GVT initiation.
 * This is used when shutting down the communication
 * subsystem to avoid deadlock in some main loop of some simulation kernel.
 *
 * @return @c true if there is some GVT-related message still pending,
 *         @c false otherwise.
 */
bool gvt_init_pending(void)
{
	return pending_msgs(MSG_NEW_GVT);
}


/**
 * @brief Forcely extract GVT-init message from MPI.
 *
 * This function synchronously extracts messages related to GVT initiation
 * from the MPI library. This should be called as a fallback strategy
 * if gvt_init_pending() determined that a clean shutdown of the
 * communication subsystem is not possible at present time.
 */
void gvt_init_clear(void)
{
	unsigned int new_gvt_round;
	lock_mpi();
	MPI_Recv(&new_gvt_round, 1, MPI_UNSIGNED, MPI_ANY_SOURCE, MSG_NEW_GVT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	unlock_mpi();
}


/**
 * @brief Reduce the GVT value.
 *
 * A call to this function can be issued only after that all threads on
 * all simulation kernels have agreed upon a local candidate for the
 * GVT value, and this value has been posted by some thread in @ref
 * local_vt_buff.
 *
 * The goal of this function is to use an All Reduce non-blocking primitive
 * to compute the minimum among the values which have been posted by
 * all kernel instances in @ref local_vt_buff.
 *
 * Eventually, the minimum will be stored in @ref reduced_gvt of all
 * kernel instances.
 */
void join_gvt_redux(simtime_t local_vt)
{
	local_vt_buff = local_vt;
	lock_mpi();
	MPI_Iallreduce(&local_vt_buff, &reduced_gvt, 1, MPI_DOUBLE, MPI_MIN, gvt_reduction_comm, &gvt_reduction_req);
	unlock_mpi();
}


/**
 * @brief Check if final GVT reduction is complete.
 *
 * A call to this function, issued after a call to join_gvt_redux() will
 * tell whether the final phase of the GVT reduction is complete, and
 * the simulation kernel instance can safely access @ref reduced_gvt to
 * pick the newly-reduced value of the GVT.
 *
 * This function is thread safe, so it can be called by multiple threads
 * at one. A spinlock guard ensures that only one thread at a time performs
 * this operation.
 *
 * @return @c true if the GVT reduction operation associated with the
 *         last round is completed, @c false otherwise.
 */
bool gvt_redux_completed(void)
{
	if (!spin_trylock(&gvt_reduction_lock))
		return false;

	bool compl = false;
	compl = is_request_completed(&gvt_reduction_req);

	spin_unlock(&gvt_reduction_lock);
	return compl;
}


/**
 * @brief Return the last GVT value.
 *
 * This function returns the last GVT value which all the threads from
 * all simulation kernel instances have agreed upon.
 *
 * It is safe to call this function also while a GVT reduction operation
 * is taking place.
 *
 * @return The current value of the GVT
 */
simtime_t last_reduced_gvt(void)
{
	return reduced_gvt;
}



/**
 * @brief Register an outgoing message, if necessary.
 *
 * Any time that a simulation kernel is sending a message towards a
 * remote simulation kernel instance, the message should be passed to
 * this function beforehand.
 *
 * In this way, if the thread which sends the message is in a red phase,
 * the minimum timestamp of red messages sent aroung is updated.
 *
 * On the other hand, if the thread which sends the message is in a
 * white phase, the counter of white messages sent towards the destination
 * kernel is increased.
 *
 * All this is fundamental information to ensure that the GVT reduction
 * is consistent.
 *
 * @param msg The message to register as an outgoing message.
 */
void register_outgoing_msg(const msg_t *msg)
{
#ifdef HAVE_CROSS_STATE
	if (is_control_msg(msg->type))
		return;
#endif

	unsigned int dst_kid = find_kernel_by_gid(msg->receiver);

	if (dst_kid == kid)
		return;

	if (is_red_colour(msg->colour)) {
		min_outgoing_red_msg[local_tid] = min(min_outgoing_red_msg[local_tid], msg->timestamp);
	} else {
		atomic_inc(&white_msg_sent[dst_kid]);
	}
}


/**
 * @brief Register an incoming message, if necessary.
 *
 * Any message being received by a simulation kernel instance should
 * be passed to this function, right before being registered into any
 * queue.
 *
 * In this way, if the destination kernel is in a white phase, the
 * total number of white messages received can be incremented.
 * Consistency is ensured by the fact that atomic counters are used,
 * making this function inherently thread-safe.
 *
 * @param msg The message to register as an incoming message.
 */
void register_incoming_msg(const msg_t * msg)
{
#ifdef HAVE_CROSS_STATE
	if (is_control_msg(msg->type))
		return;
#endif

	unsigned int src_kid = find_kernel_by_gid(msg->sender);

	if (src_kid == kid)
		return;

	if (msg->colour == white_0) {
		atomic_inc(&white_0_msg_recv);
	} else if (msg->colour == white_1) {
		atomic_inc(&white_1_msg_recv);
	}
}

#endif
