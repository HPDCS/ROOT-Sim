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


#include <communication/mpi.h>
#include <communication/wnd.h>
#include <communication/gvt.h>
#include <communication/communication.h>
#include <queues/queues.h>
#include <core/core.h>
#include <arch/atomic.h>


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
	out_msg->msg->colour = threads_phase_colour[tid];
	unsigned int dest = GidToKernel(msg->receiver);

	register_outgoing_msg(out_msg->msg);

	lock_mpi();
	MPI_Isend(((char*)out_msg->msg) + MSG_PADDING, MSG_META_SIZE + msg->size, MPI_BYTE, dest, MSG_EVENT, MPI_COMM_WORLD, &out_msg->req);
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
	int pending;

	if(!spin_trylock(&msgs_lock))
		return;
	
	while(true){
		lock_mpi();
		MPI_Iprobe(MPI_ANY_SOURCE, MSG_EVENT, MPI_COMM_WORLD, &pending, &status);
		unlock_mpi();

		if(!pending)
			goto out;

		MPI_Get_count(&status, MPI_BYTE, &size);

		if(MSG_PADDING + size <= SLAB_MSG_SIZE)
			msg = get_msg_from_slab();
		else{
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
		MPI_Recv(((char*)msg) + MSG_PADDING, size, MPI_BYTE, MPI_ANY_SOURCE, MSG_EVENT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
	if(terminated == 0 || !spin_trylock(&msgs_fini)) return;

	int res;
	unsigned int tdata;
	while(pending_msgs(MSG_FINI)){
		lock_mpi();
		res = MPI_Recv(&tdata, 1, MPI_UNSIGNED, MPI_ANY_SOURCE, MSG_FINI, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		unlock_mpi();
		if(res != 0){
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
	for(i = 0; i < n_ker; i++){
		if(i==kid)
			continue;
		MPI_Isend(&i, 1, MPI_UNSIGNED, i, MSG_FINI, MPI_COMM_WORLD, &termination_reqs[i]);
	}
	terminated++;
	unlock_mpi();
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


/*
 * Syncronize all the kernels:
 *
 * This function can be used as synchronization barrier between all the threads
 * of all the kernels.
 *
 * The function will return only after all the threads on all the kernels
 * have already entered this function.
 */
void synchronize_all(void){
	if(master_thread()){
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
}


void inter_kernel_comm_init(void){
	spinlock_init(&msgs_lock);

	outgoing_window_init();
	gvt_comm_init();
	dist_termination_init();
}


void inter_kernel_comm_finalize(void){
	dist_termination_finalize();
	//outgoing_window_finalize();
	gvt_comm_finalize();
}


void mpi_finalize(void){
	if(master_thread()){
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Finalize();
	}else{
		rootsim_error(true, "MPI finalize has been invoked by a non master thread: T%u\n", tid);
	}
}


#endif /* HAVE_MPI */
