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
* Copyright (C) 2008-2018 HPDCS Group
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
 * Test completion of white message reduction collective operation.
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
 * @brief Syncronously wait for the completion of white message reduction
 *        collective operation.
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
 */
bool all_white_msg_received(void)
{
	return (atomic_read(white_msg_recv) == expected_white_msg);
}

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

bool gvt_init_pending(void)
{
	return pending_msgs(MSG_NEW_GVT);
}

void gvt_init_clear(void)
{
	unsigned int new_gvt_round;
	lock_mpi();
	MPI_Recv(&new_gvt_round, 1, MPI_UNSIGNED, MPI_ANY_SOURCE, MSG_NEW_GVT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	unlock_mpi();
}

void join_gvt_redux(simtime_t local_vt)
{
	local_vt_buff = local_vt;
	lock_mpi();
	MPI_Iallreduce(&local_vt_buff, &reduced_gvt, 1, MPI_DOUBLE, MPI_MIN, gvt_reduction_comm, &gvt_reduction_req);
	unlock_mpi();
}

bool gvt_redux_completed(void)
{
	if (!spin_trylock(&gvt_reduction_lock))
		return false;

	bool compl = false;
	compl = is_request_completed(&gvt_reduction_req);

	spin_unlock(&gvt_reduction_lock);
	return compl;
}

simtime_t last_reduced_gvt(void)
{
	return reduced_gvt;
}

void register_outgoing_msg(const msg_t * msg)
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
