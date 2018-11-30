/**
*                       Copyright (C) 2008-2018 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file mpi.c
* @brief MPI support module
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

// true if the underlying MPI implementation support multithreading
bool mpi_support_multithread;

// This global lock is used by the lock/unlock_mpi macro to
// control access to MPI interface
spinlock_t mpi_lock;

// control access to the message receiving routine
static spinlock_t msgs_lock;

// counter of the kernels that have already reached the
// termination condition. Must be updated through the collect_termination() function.
static unsigned int terminated = 0;
static MPI_Request *termination_reqs;
static spinlock_t msgs_fini;


// MPI Operation to reduce statics struct
static MPI_Op reduce_stats_op;

static MPI_Datatype stats_mpi_t;

static MPI_Comm msg_comm;



/*
 * Check if there are pending messages from remote kernels with
 * the specific `tag`.
 *
 * This function is thread-safe.
 */
bool pending_msgs(int tag){
	int flag = 0;
	lock_mpi();
	MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
	unlock_mpi();
	return (bool) flag;
}


/*
 * Check if the MPI_Request `req` have been completed.
 *
 * This function is thread-safe.
 */
bool is_request_completed(MPI_Request* req){
	int flag = 0;
	lock_mpi();
	MPI_Test(req, &flag, MPI_STATUS_IGNORE);
	unlock_mpi();
	return (bool) flag;
}


/*
 * Send a message destined to an LP binded to a remote kernel.
 *
 * The message will be stored in the outgoing message queue and the
 * send operation will be perfomed asyncronusly.
 *
 * This function is thread-safe.
 */
void send_remote_msg(msg_t *msg){
	outgoing_msg *out_msg = allocate_outgoing_msg();
	out_msg->msg = msg;
	out_msg->msg->colour = threads_phase_colour[local_tid];
	unsigned int dest = find_kernel_by_gid(msg->receiver);

	register_outgoing_msg(out_msg->msg);

	lock_mpi();
	MPI_Isend(((char*)out_msg->msg) + MSG_PADDING, MSG_META_SIZE + msg->size, MPI_BYTE, dest, msg->receiver.to_int, msg_comm, &out_msg->req);
	unlock_mpi();
	// Keep the message in the outgoing queue until it will be delivered
	store_outgoing_msg(out_msg, dest);
}


/*
 * Receive messages sent from remote kernels and
 * destined to locally hosted LPs.
 *
 * This function will block until all the currently pending messages
 * have been received and inserted into the bottom-half.
 *
 * This function is thread-safe.
 */
void receive_remote_msgs(void){
	int size;
	msg_t *msg;
	MPI_Status status;
	MPI_Message mpi_msg;
	int pending;
	struct lp_struct *lp;
	GID_t gid;

	if(!spin_trylock(&msgs_lock))
		return;

	while(true){
		lock_mpi();
		MPI_Improbe(MPI_ANY_SOURCE, MPI_ANY_TAG, msg_comm, &pending, &mpi_msg, &status);
		unlock_mpi();

		if(!pending)
			goto out;

		MPI_Get_count(&status, MPI_BYTE, &size);

		if(likely(MSG_PADDING + size <= SLAB_MSG_SIZE)) {
			set_gid(gid, status.MPI_TAG);
			lp = find_lp_by_gid(gid);
			msg = get_msg_from_slab(lp);
		} else {
			msg = rsalloc(MSG_PADDING + size);
			bzero(msg, MSG_PADDING);
		}
		/* - `pending_msgs` and `MPI_Recv` need to be in the same critical section.
		 *    I could start an MPI_Recv with an empty incoming queue.
		 * - `MPI_Recv` and `insert_bottom_half` need to be in the same critical section.
		 *    messages need to be inserted in arrival order into the BH
		 */

		// Receive the message
		lock_mpi();
		MPI_Mrecv(((char*)msg) + MSG_PADDING, size, MPI_BYTE, &mpi_msg, MPI_STATUS_IGNORE);
		unlock_mpi();

		validate_msg(msg);
		insert_bottom_half(msg);
	}
    out:
	spin_unlock(&msgs_lock);
}


/* Return true if all the kernel have reached the termination condition
 *
 * This function can be used only after `broadcast_termination()`
 * function has been correctly executed.
 */
bool all_kernels_terminated(void){
	return (terminated == n_ker);
}


/*
 * Accumulate termination acknoledgements from remote kernels and update the `terminated` counter
 *
 * This function can be called at any point of the simulation
 * but it will be effective only after that broadcast_termination() have been called locally.
 *
 * This function is thread-safe
 */
void collect_termination(void){
	int res;
	unsigned int tdata;
	
	if(terminated == 0 || !spin_trylock(&msgs_fini))
		return;

	while(pending_msgs(MSG_FINI)) {
		lock_mpi();
		res = MPI_Recv(&tdata, 1, MPI_UNSIGNED, MPI_ANY_SOURCE, MSG_FINI, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		unlock_mpi();
		if(unlikely(res != 0)) {
			rootsim_error(true, "MPI_Recv did not complete correctly");
			return;
		}
		terminated++;
	}
	spin_unlock(&msgs_fini);
}



/* Notify all the kernels about local termination
 *
 * This function can be called multiple time,
 * but the actual broadcasting operation will be executed only
 * on the first call.
 *
 * Multithread infos:
 * This function is not thread safe and should be used
 * by only one thread at time.
 * This function can be used concurrently with other
 * MPI communication function.
 */
void broadcast_termination(void){
	unsigned int i;
	lock_mpi();
	for(i = 0; i < n_ker; i++) {
		if(i == kid)
			continue;
		MPI_Isend(&i, 1, MPI_UNSIGNED, i, MSG_FINI, MPI_COMM_WORLD, &termination_reqs[i]);
	}
	terminated++;
	unlock_mpi();
}

/*
 * This is the reduce operation for statistics,
 */
static void reduce_stat_vector(struct stat_t *in, struct stat_t *inout, int *len, MPI_Datatype *dptr) {
	int i = 0;
	(void)dptr;

	for(i = 0; i < *len; ++i) {
		inout[i].vec += in[i].vec;
		inout[i].gvt_round_time += in[i].gvt_round_time;
		inout[i].gvt_round_time_min = fmin(inout[i].gvt_round_time_min, in[i].gvt_round_time_min);
		inout[i].gvt_round_time_max = fmax(inout[i].gvt_round_time_max, in[i].gvt_round_time_max);
		inout[i].max_resident_set += in[i].max_resident_set;
	}
}

// this assumes struct stat_t contains only double floating point variables
#define MPI_TYPE_STAT_LEN (sizeof(struct stat_t)/sizeof(double))

static void stats_reduction_init(void) {
	// this is a compilation time fail-safe
	static_assert(offsetof(struct stat_t, gvt_round_time_max) == (sizeof(double) * 19),
			"The packing assumptions on struct stat_t are wrong or its definition has been modified");

	unsigned i;
	// boilerplate to create a new MPI data type
	MPI_Datatype type[MPI_TYPE_STAT_LEN];
	MPI_Aint disp[MPI_TYPE_STAT_LEN];
	int block_lengths[MPI_TYPE_STAT_LEN];
	// init those arrays (we asssume that struct stat_t is packed tightly)
	for(i = 0; i < MPI_TYPE_STAT_LEN; ++i){
		type[i] = MPI_DOUBLE;
		disp[i] = i * sizeof(double);
		block_lengths[i] = 1;
	}
	// create the custom type and commit the changes
	MPI_Type_create_struct(MPI_TYPE_STAT_LEN, block_lengths, disp, type, &stats_mpi_t);
	MPI_Type_commit(&stats_mpi_t);
	// create the mpi operation needed to reduce stats
	if(master_thread())
		MPI_Op_create((MPI_User_function *)reduce_stat_vector, true, &reduce_stats_op);
}
#undef MPI_TYPE_STAT_LEN


void mpi_reduce_statistics(struct stat_t *global, struct stat_t *local) {
	MPI_Reduce(local, global, 1, stats_mpi_t, reduce_stats_op, 0, MPI_COMM_WORLD);
}



/*
 * Initialize the distributed termination subsystem
 */
void dist_termination_init(void){
	/* init for collective termination */
	termination_reqs = rsalloc(n_ker * sizeof(MPI_Request));
	unsigned int i;
	for(i=0; i<n_ker; i++){
		termination_reqs[i] = MPI_REQUEST_NULL;
	}
	spinlock_init(&msgs_fini);
}


/*
 * Cleanup routine of the distributed termination subsystem
 */
void dist_termination_finalize(void){
	MPI_Waitall(n_ker, termination_reqs, MPI_STATUSES_IGNORE);
}


/**
 * Syncronize all the kernels:
 *
 * This function can be used as syncronization barrier between all the threads
 * of all the kernels.
 *
 * The function will return only after all the threads on all the kernels
 * have already entered this function.
 */
void syncronize_all(void){
	if(master_thread()) {
		MPI_Comm comm;
		MPI_Comm_dup(MPI_COMM_WORLD, &comm);
		MPI_Barrier(comm);
		MPI_Comm_free(&comm);
	}
	thread_barrier(&all_thread_barrier);
}


/*
 * Wrapper of MPI_Init call
 */
void mpi_init(int *argc, char ***argv){
	int mpi_thread_lvl_provided = 0;
	MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &mpi_thread_lvl_provided);

	mpi_support_multithread = true;
	if(mpi_thread_lvl_provided < MPI_THREAD_MULTIPLE){
		//MPI do not support thread safe api call
		if(mpi_thread_lvl_provided < MPI_THREAD_SERIALIZED){
			// MPI do not even support serialized threaded call we cannot continue
			rootsim_error(true, "The MPI implementation does not support threads "
			                    "[current thread level support: %d]\n", mpi_thread_lvl_provided);
		}
		mpi_support_multithread = false;
	}

	spinlock_init(&mpi_lock);

	MPI_Comm_size(MPI_COMM_WORLD, (int *)&n_ker);
	MPI_Comm_rank(MPI_COMM_WORLD, (int *)&kid);

	// Create a separate communicator which we use to send event messages
	MPI_Comm_dup(MPI_COMM_WORLD, &msg_comm);
}


void inter_kernel_comm_init(void) {
	spinlock_init(&msgs_lock);

	outgoing_window_init();
	gvt_comm_init();
	dist_termination_init();
	stats_reduction_init();
}


void inter_kernel_comm_finalize(void) {
	dist_termination_finalize();
	//outgoing_window_finalize();
	gvt_comm_finalize();
}


void mpi_finalize(void) {
	if(master_thread()) {
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Comm_free(&msg_comm);
		MPI_Finalize();
	} else {
		rootsim_error(true, "MPI finalize has been invoked by a non master thread: T%u\n", local_tid);
	}
}


#endif /* HAVE_MPI */
