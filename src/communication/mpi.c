/**
* @file communication/mpi.c
*
* @brief MPI Support Module
*
* This module implements all basic MPI facilities to let the distributed
* execution of a simulation model take place consistently.
*
* Several facilities are thread-safe, others are not. Check carefully which
* of these can be used by worker threads without coordination when relying
* on this module.
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

#include <stdbool.h>

#include <communication/mpi.h>
#include <communication/wnd.h>
#include <communication/gvt.h>
#include <communication/communication.h>
#include <queues/queues.h>
#include <core/core.h>
#include <arch/atomic.h>
#include <statistics/statistics.h>

/// Flag telling whether the MPI runtime supports multithreading
bool mpi_support_multithread;

/**
 * This global lock is used by the lock/unlock_mpi macro to
 * control access to MPI interface. If proper MPI threading support
 * is available from the runtime, then it is not used.
 */
spinlock_t mpi_lock;

/// A guard to ensure isolation in the the message receiving routine
static spinlock_t msgs_lock;

/**
 * This counter tells how many simulation kernel instances have already
 * reached the termination condition. This is updated via collect_termination().
 */
static unsigned int terminated = 0;

/// MPI Requests to handle termination detection collection asynchronously
static MPI_Request *termination_reqs;

/// A guard to ensure isolation in collect_termination()
static spinlock_t msgs_fini;

/// MPI Operation to reduce statics
static MPI_Op reduce_stats_op;

/// MPI Datatype to describe the content of a struct @ref stat_t
static MPI_Datatype stats_mpi_t;

/**
 * @brief MPI Communicator for event/control messages.
 *
 * To enable zero-copy message passing, we must know what LP is the destination
 * of an event, @e before extracting that event from the MPI layer. This
 * is necessary to determine from what slab/buddy the memory to keep
 * the event must be taken. Yet, this is impossible because the MPI layer
 * does not allow to do so.
 *
 * To actually be able to do that, the trick is to create a separate
 * MPI Communicator which is used @e only to exchance events across LPs
 * (control messages also fall in this category). Then, since we can
 * extract events from this communicator, we can match against both
 * MPI_ANY_SOURCE (to receive events from any simulation kernel instance)
 * and MPI_ANY_TAG (to match independently of the tag).
 *
 * We therefore use the tag to identify the GID of the LP.
 *
 * We can retrieve the information about the message sender and the size
 * of the message which will be extracted by inspecting the MPI_Status
 * variable after an MPI_Iprobe is completed.
 */
static MPI_Comm msg_comm;


/**
 * @brief Check if there are pending messages
 *
 * This function tells whether there is a pending message in the underlying
 * MPI library coming from any remote simulation kernel instance. If passing
 * a tag different from MPI_ANY_TAG to this function, a specific tag can
 * be extracted.
 *
 * Messages are only extracted from MPI_COMM_WORLD communicator.  This is
 * therefore only useful in startup/shutdown operations (this is used
 * indeed to initiate GVT and conclude the distributed simulation shutdown).
 *
 * @note This function is thread-safe.
 *
 * @param tag The tag of the messages to check for availability.
 *
 * @return @c true if a pending message tagged with @p tag is found,
 *         @c false otherwise.
 */
bool pending_msgs(int tag)
{
	int flag = 0;
	lock_mpi();
	MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
	unlock_mpi();
	return (bool)flag;
}

/**
 * @brief check if an MPI request has been completed
 *
 * This function checks whether the operation associated with the specified
 * MPI Request has been completed or not.
 *
 * @note This function is thread-safe.
 *
 * @param req A pointer to the MPI_Request to check for completion
 *
 * @return @c true if the operation associated with @p req is complete,
 *         @c false otherwise.
 */
bool is_request_completed(MPI_Request *req)
{
	int flag = 0;
	lock_mpi();
	MPI_Test(req, &flag, MPI_STATUS_IGNORE);
	unlock_mpi();
	return (bool)flag;
}

/**
 * @brief Send a message to a remote LP
 *
 * This function takes in charge an event to be delivered to a remote LP.
 * The sending operation is non-blocking: to this end, the message is
 * registered into the outgoing queue of the destination kernel, in order
 * to allow MPI to keep track of the sending operation.
 *
 * Also, the message being sent is registered at the sender thread, to
 * keep track of the white/red message information which is necessary
 * to correctly reduce the GVT value.
 *
 * @note This function is thread-safe.
 *
 * @param msg A pointer to the @ref msg_t keeping the message to be sent remotely
 */
void send_remote_msg(msg_t *msg)
{
	outgoing_msg *out_msg = allocate_outgoing_msg();
	out_msg->msg = msg;
	out_msg->msg->colour = threads_phase_colour[local_tid];
	unsigned int dest = find_kernel_by_gid(msg->receiver);

	register_outgoing_msg(out_msg->msg);

	lock_mpi();
	MPI_Isend(((char *)out_msg->msg) + MSG_PADDING, MSG_META_SIZE + msg->size, MPI_BYTE, dest, msg->receiver.to_int, msg_comm, &out_msg->req);
	unlock_mpi();

	// Keep the message in the outgoing queue until it will be delivered
	store_outgoing_msg(out_msg, dest);
}

/**
 * @brief Receive remote messages
 *
 * This function extracts from MPI events destined to locally-hosted
 * LPs. Only messages to LP can be extracted here, because the probing
 * is done towards the @ref msg_comm communicator.
 *
 * A message which is extracted here is placed (out of order) in the
 * bottom half of the destination LP, for later insertion (in order) in
 * the input queue.
 *
 * This function will try to extract as many messages as possible from
 * the underlying MPI library. In particular, once this function is
 * called, it will return only after that @e no @e message can be found
 * in the MPI library, destined to this simulation kernel instance.
 *
 * Currently, this function is called once per main loop iteration. Doing
 * more calls might significantly imbalance the workload of some worker
 * thread.
 *
 * @note This function is thread-safe.
 */
void receive_remote_msgs(void)
{
	int size;
	msg_t *msg;
	MPI_Status status;
	MPI_Message mpi_msg;
	int pending;
	struct lp_struct *lp;
	GID_t gid;

	// TODO: given the latest changes in the platform, this *might*
	// be removed.
	if (!spin_trylock(&msgs_lock))
		return;

	while (true) {
		lock_mpi();
		MPI_Improbe(MPI_ANY_SOURCE, MPI_ANY_TAG, msg_comm, &pending, &mpi_msg, &status);
		unlock_mpi();

		if (!pending)
			goto out;

		MPI_Get_count(&status, MPI_BYTE, &size);

		if (likely(MSG_PADDING + size <= SLAB_MSG_SIZE)) {
			set_gid(gid, status.MPI_TAG);
			lp = find_lp_by_gid(gid);
			msg = get_msg_from_slab(lp);
		} else {
			msg = rsalloc(MSG_PADDING + size);
			bzero(msg, MSG_PADDING);
		}

		// Receive the message. Use MPI_Mrecv to be sure that the very same message
		// which was matched by the previous MPI_Improbe is extracted.
		lock_mpi();
		MPI_Mrecv(((char *)msg) + MSG_PADDING, size, MPI_BYTE, &mpi_msg, MPI_STATUS_IGNORE);
		unlock_mpi();

		validate_msg(msg);
		insert_bottom_half(msg);
	}
    out:
	spin_unlock(&msgs_lock);
}



/**
 * @brief Check if all kernels have reached the termination condition
 *
 * This function checks whether all threads have been informed of the
 * fact that the simulation should be halted, and they have taken
 * proper actions to terminate. Once this function confirms this condition,
 * the process can safely exit.
 *
 * @warning This function can be called only @b after a call to
 *          broadcast_termination()
 *
 * @return @c true if all the kernel have reached the termination condition
 */
bool all_kernels_terminated(void)
{
	return (terminated == n_ker);
}



/**
 * @brief Check if other kernels have reached the termination condition
 *
 * This function accumulates termination acknoledgements from remote
 * kernels, and updates the @ref terminated counter.
 *
 * @note This function can be called at any point of the simulation,
 *       but it will be effective only after that broadcast_termination()
 *       has been called locally.
 *
 * @note This function is thread-safe
 */
void collect_termination(void)
{
	int res;
	unsigned int tdata;

	if (terminated == 0 || !spin_trylock(&msgs_fini))
		return;

	while (pending_msgs(MSG_FINI)) {
		lock_mpi();
		res =
		    MPI_Recv(&tdata, 1, MPI_UNSIGNED, MPI_ANY_SOURCE, MSG_FINI, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		unlock_mpi();
		if (unlikely(res != 0)) {
			rootsim_error(true, "MPI_Recv did not complete correctly");
			return;
		}
		terminated++;
	}
	spin_unlock(&msgs_fini);
}


/**
 * @brief Notify all the kernels about local termination
 *
 * This function is used to inform all other simulation kernel instances
 * that this kernel is ready to terminate the simulation.
 *
 * @warning This function is not thread safe and should be used only
 *          by one thread at a time
 *
 * @note This function can be used concurrently with other MPI functions
 *       (hence its thread unsafety)
 *
 * @note This function can be called multiple times, but the actual
 *       broadcast operation will be executed only on the first call.
 */
void broadcast_termination(void)
{
	unsigned int i;
	lock_mpi();
	for (i = 0; i < n_ker; i++) {
		if (i == kid)
			continue;
		MPI_Isend(&i, 1, MPI_UNSIGNED, i, MSG_FINI, MPI_COMM_WORLD, &termination_reqs[i]);
	}
	terminated++;
	unlock_mpi();
}


/**
 * @brief Reduce operation for statistics.
 *
 * This function implements a custom MPI Operation used to reduce globally
 * local statistics upon simulation shutdown. This function is bound to
 * @ref reduce_stats_op in stats_reduction_init().
 */
static void reduce_stat_vector(struct stat_t *in, struct stat_t *inout, int *len, MPI_Datatype *dptr)
{
	(void)dptr;
	int i = 0;

	for (i = 0; i < *len; ++i) {
		inout[i].vec += in[i].vec;
		inout[i].gvt_round_time += in[i].gvt_round_time;
		inout[i].gvt_round_time_min = fmin(inout[i].gvt_round_time_min, in[i].gvt_round_time_min);
		inout[i].gvt_round_time_max = fmax(inout[i].gvt_round_time_max, in[i].gvt_round_time_max);
		inout[i].max_resident_set += in[i].max_resident_set;
	}
}



/// The size in bytes of the statistics custom MPI Datatype. It assumes that @ref stat_t contains only double floating point members
#define MPI_TYPE_STAT_LEN (sizeof(struct stat_t)/sizeof(double))

/**
 * @brief Initialize MPI Datatype and Operation for statistics reduction
 *
 * To reduce statistics, we rely on a custom MPI Operation. This operation
 * requires a pre-built MPI Datatype to properly handle the structures which
 * we use to represent the local information.
 *
 * This function is called when initializing inter-kernel communication,
 * and its purpose is exactly that of setting up a custom MPI datatype in
 * @ref stats_mpi_t.
 *
 * Additionally, this function defines the custom operation implemented in
 * reduce_stat_vector() which is bound to the MPI Operation @ref reduce_stats_op.
 */
static void stats_reduction_init(void)
{
	// This is a compilation time fail-safe
	static_assert(offsetof(struct stat_t, gvt_round_time_max) == (sizeof(double) * 19), "The packing assumptions on struct stat_t are wrong or its definition has been modified");

	unsigned i;

	// Boilerplate to create a new MPI data type
	MPI_Datatype type[MPI_TYPE_STAT_LEN];
	MPI_Aint disp[MPI_TYPE_STAT_LEN];
	int block_lengths[MPI_TYPE_STAT_LEN];

	// Initialize those arrays (we asssume that struct stat_t is packed tightly)
	for (i = 0; i < MPI_TYPE_STAT_LEN; ++i) {
		type[i] = MPI_DOUBLE;
		disp[i] = i * sizeof(double);
		block_lengths[i] = 1;
	}

	// Create the custom type and commit the changes
	MPI_Type_create_struct(MPI_TYPE_STAT_LEN, block_lengths, disp, type, &stats_mpi_t);
	MPI_Type_commit(&stats_mpi_t);

	// Create the MPI Operation used to reduce stats
	if (master_thread()) {
		MPI_Op_create((MPI_User_function *)reduce_stat_vector, true, &reduce_stats_op);
	}
}

#undef MPI_TYPE_STAT_LEN


/**
 * @brief Invoke statistics reduction.
 *
 * This function is a simple wrapper of an MPI_Reduce operation, which
 * uses the custom reduce operation implemented in reduce_stat_vector()
 * to gather reduced statistics in the master kernel (rank 0).
 *
 * @param global A pointer to a struct @ref stat_t where reduced statistics
 *               will be stored. The reduction only takes place at rank 0,
 *               therefore other simulation kernel instances will never
 *               read actual meaningful information in that structure.
 * @param local A pointer to a local struct @ref stat_t which is used
 *               as the source of information for the distributed reduction
 *               operation.
 */
void mpi_reduce_statistics(struct stat_t *global, struct stat_t *local)
{
	MPI_Reduce(local, global, 1, stats_mpi_t, reduce_stats_op, 0, MPI_COMM_WORLD);
}



/**
 * @brief Setup the distributed termination subsystem
 *
 * To correctly terminate a distributed simulation, some care must be
 * taken. In particular:
 * * we must be use that no deadlock is generated, e.g. because some
 *   simulation kernel is already waiting for some synchronization action
 *   by other kernels
 * * we must be sure that no MPI action is in place/still pending, when
 *   MPI_Finalize() is called.
 *
 * To this end, a specific distributed termination protocol is put in place,
 * which requires some data structures to be available.
 *
 * This function initializes the subsystem and the datastructures which
 * ensure a clean a nice shutdown of distributed simulations.
 */
void dist_termination_init(void)
{
	/* init for collective termination */
	termination_reqs = rsalloc(n_ker * sizeof(MPI_Request));
	unsigned int i;
	for (i = 0; i < n_ker; i++) {
		termination_reqs[i] = MPI_REQUEST_NULL;
	}
	spinlock_init(&msgs_fini);
}



/**
 * @brief Cleanup routine of the distributed termination subsystem
 *
 * Once this function returns, it is sure that we can terminate safely
 * the simulation.
 */
void dist_termination_finalize(void)
{
	MPI_Waitall(n_ker, termination_reqs, MPI_STATUSES_IGNORE);
}

/**
 * @brief Syncronize all the kernels
 *
 * This function can be used as syncronization barrier between all the threads
 * of all the kernels.
 *
 * The function will return only after all the threads on all the kernels
 * have already entered this function.
 *
 * We create a new communicator here, to be sure that we synchronize
 * exactly in this function and not somewhere else.
 *
 * @warning This function is extremely resource intensive, wastes a lot
 *          of cpu cycles, and drops performance significantly. Avoid
 *          using it as much as possible!
 */
void syncronize_all(void)
{
	if (master_thread()) {
		MPI_Comm comm;
		MPI_Comm_dup(MPI_COMM_WORLD, &comm);
		MPI_Barrier(comm);
		MPI_Comm_free(&comm);
	}
	thread_barrier(&all_thread_barrier);
}


/**
 * @brief Initialize MPI subsystem
 *
 * This is mainly a wrapper of MPI_Init, which contains some boilerplate
 * code to initialize datastructures.
 *
 * Most notably, here we determine if the library which we are using
 * has suitable multithreading support, and we setup the MPI Communicator
 * which will be used later on to exhange model-specific messages.
 */
void mpi_init(int *argc, char ***argv)
{
	int mpi_thread_lvl_provided = 0;
	MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &mpi_thread_lvl_provided);

	mpi_support_multithread = true;
	if (mpi_thread_lvl_provided < MPI_THREAD_MULTIPLE) {
		//MPI do not support thread safe api call
		if (mpi_thread_lvl_provided < MPI_THREAD_SERIALIZED) {
			// MPI do not even support serialized threaded call we cannot continue
			rootsim_error(true, "The MPI implementation does not support threads [current thread level support: %d]\n", mpi_thread_lvl_provided);
		}
		mpi_support_multithread = false;
	}

	spinlock_init(&mpi_lock);

	MPI_Comm_size(MPI_COMM_WORLD, (int *)&n_ker);
	MPI_Comm_rank(MPI_COMM_WORLD, (int *)&kid);

	// Create a separate communicator which we use to send event messages
	MPI_Comm_dup(MPI_COMM_WORLD, &msg_comm);
}


/**
 * @brief Initialize inter-kernel communication
 *
 * This function initializes inter-kernel communication, by initializing
 * all the other communication subsystems.
 */
void inter_kernel_comm_init(void)
{
	spinlock_init(&msgs_lock);

	outgoing_window_init();
	gvt_comm_init();
	dist_termination_init();
	stats_reduction_init();
}


/**
 * @brief Finalize inter-kernel communication
 *
 * This function shutdown the subsystems associated with inter-kernel
 * communication.
 */
void inter_kernel_comm_finalize(void)
{
	dist_termination_finalize();
	//outgoing_window_finalize();
	gvt_comm_finalize();
}


/**
 * @brief Finalize MPI
 *
 * This function shutdown the MPI subsystem
 *
 * @note Only the master thread on each simulation kernel is expected
 *       to call this function
 */
void mpi_finalize(void)
{
	if (master_thread()) {
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Comm_free(&msg_comm);
		MPI_Finalize();
	} else {
		rootsim_error(true, "MPI finalize has been invoked by a non master thread: T%u\n", local_tid);
	}
}

#endif /* HAVE_MPI */
