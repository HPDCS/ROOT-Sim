#ifdef HAS_MPI


#include <communication/communication.h>
#include <communication/mpi.h>
#include <communication/gvt.h>


// current colour phase of each thread
phase_colour* threads_phase_colour;

// minimum time among all the outgoing red messages for each thread
simtime_t* min_outgoing_red_msg;

/*
 * `white_msg_recv` must always point to the
 * actual counter of white message received by the current kernel
 * that have been sent during the last global white phase.
 * It will point either to `white_0_msg_recv` or
 * to `white_1_msg_recv`.
 */
volatile atomic_t* white_msg_recv;
atomic_t white_0_msg_recv;
atomic_t white_1_msg_recv;

/* number of white message sent from this kernel
   to all the others during the last white phase */
atomic_t *white_msg_sent;

// temporary structure used for the MPI collective
static int *white_msg_sent_buff;
/* number of expected white message to be received by this kernel
   before to partecipate to the next GVT agreement */
static int expected_white_msg;
MPI_Request white_count_req;
MPI_Comm white_count_comm;
spinlock_t white_count_lock;
bool white_count_pending;

MPI_Request gvt_reduction_req;
MPI_Comm gvt_reduction_comm;
spinlock_t gvt_reduction_lock;
simtime_t local_vt_buff;
simtime_t reduced_gvt;

MPI_Request *gvt_init_reqs;
unsigned int gvt_init_round;


void gvt_comm_init(void){
	unsigned int i;

	min_outgoing_red_msg = rsalloc(n_cores * sizeof(simtime_t));
	threads_phase_colour = rsalloc(n_cores * sizeof(phase_colour));
	for(i=0; i<n_cores; i++){
		min_outgoing_red_msg[i] = INFTY;
		threads_phase_colour[i] = white_0;
	}

	atomic_set(&white_0_msg_recv, 0);
	atomic_set(&white_1_msg_recv, 0);
	white_msg_recv = &white_1_msg_recv;
	expected_white_msg = 0;

	white_msg_sent = rsalloc(n_ker * sizeof(atomic_t));
	for(i=0; i<n_ker; i++){
		atomic_set(&white_msg_sent[i], 0);
	}

	white_msg_sent_buff = rsalloc(n_ker * sizeof(unsigned int));

	white_count_req = MPI_REQUEST_NULL;
	MPI_Comm_dup(MPI_COMM_WORLD, &white_count_comm);
	spinlock_init(&white_count_lock);

	gvt_init_reqs = rsalloc(n_ker * sizeof(MPI_Request));
	for(i=0; i<n_ker; i++){
		gvt_init_reqs[i] = MPI_REQUEST_NULL;
	}

	gvt_reduction_req = MPI_REQUEST_NULL;
	MPI_Comm_dup(MPI_COMM_WORLD, &gvt_reduction_comm);
	spinlock_init(&gvt_reduction_lock);
}


void gvt_comm_finalize(void){
	rsfree(min_outgoing_red_msg);
	rsfree(threads_phase_colour);
	rsfree(white_msg_sent);
	rsfree(white_msg_sent_buff);


	unsigned int i;
	for(i=0; i<n_ker; i++){
		if(gvt_init_reqs[i] != MPI_REQUEST_NULL){
			MPI_Cancel(&gvt_init_reqs[i]);
			MPI_Request_free(&gvt_init_reqs[i]);
		}
	}

	if(gvt_init_pending()){
		gvt_init_clear();
	}

	MPI_Wait(&white_count_req, MPI_STATUS_IGNORE);
	MPI_Comm_free(&white_count_comm);
	MPI_Wait(&gvt_reduction_req, MPI_STATUS_IGNORE);
	MPI_Comm_free(&gvt_reduction_comm);
	rsfree(gvt_init_reqs);
}


/*
 * Make a thread enter into the red phase.
 *
 * The calling thread phase will be turn to red and
 * its minimum timestamp among the sent red messages will be reset.
 *
 * An error will be raised in the case the calling thread is already into
 * a red phase.
 */
void enter_red_phase(void){
	if(in_red_phase()){
		rootsim_error(true, "Thread %u cannot enter in red phase "
				"because is already in red phase.\n");
	}
	min_outgoing_red_msg[local_tid] = INFTY;
	threads_phase_colour[local_tid] = next_colour(threads_phase_colour[local_tid]);
}


/*
 * Make a thread exit from the red phase.
 *
 * The calling thread phase will be turn to white.
 *
 * An error will be raised in the case the calling thread is not in red phase.
 */
void exit_red_phase(void){
	if(!in_red_phase()){
		rootsim_error(true, "Thread %u cannot exit from red phase "
				"because it wasn't in red phase.\n");
	}
	threads_phase_colour[local_tid] = next_colour(threads_phase_colour[local_tid]);
}


/*
 * Join the white message reduction collective operation.
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
void join_white_msg_redux(void){
	unsigned int i;
	for(i=0; i<n_ker; i++){
		white_msg_sent_buff[i] = atomic_read(&white_msg_sent[i]);
	}

	lock_mpi();
	MPI_Ireduce_scatter_block(white_msg_sent_buff, &expected_white_msg, 1, MPI_INT, MPI_SUM, white_count_comm, &white_count_req);
	unlock_mpi();
}


/*
 * Test completion of white message reduction collective operation.
 */
bool white_msg_redux_completed(void){
	if(!spin_trylock(&white_count_lock)) return false;

	bool compl = false;
	compl = is_request_completed(&white_count_req);

	spin_unlock(&white_count_lock);
	return compl;
}


/*
 * Syncronously wait for the completion of white message reduction collective operation.
 */
void wait_white_msg_redux(void){
	MPI_Wait(&white_count_req, MPI_STATUS_IGNORE);
}


/*
 * Check if the number of white message received is equal to
 * the expected ones (retrieved through the white message reduction).
 */
bool all_white_msg_received(void){
	return ( atomic_read(white_msg_recv) == expected_white_msg );
}


void flush_white_msg_recv(void){
	// santy check: Exactly the expected number of white
	// messages should have been arrived in the last GVT round
	if(atomic_read(white_msg_recv) != expected_white_msg){
		rootsim_error(true,
			"unexpected number of white messages received in the last GVT round: [expected: %d, received: %d]\n",
			expected_white_msg, atomic_read(white_msg_recv));
	}

	// prepare the white msg counter for the next GVT round
	atomic_set(white_msg_recv, 0);

	if(white_msg_recv == &white_0_msg_recv){
		white_msg_recv = &white_1_msg_recv;
	}else{
		white_msg_recv = &white_0_msg_recv;
	}
}


void flush_white_msg_sent(void){
	unsigned int i;
	//sanity check
	for(i = 0; i < n_cores; i++){
		if(!is_red_colour(threads_phase_colour[i]))
			rootsim_error(true, "flushing outgoing white message counter while some thread are not in red phase\n");
	}

	for(i = 0; i < n_ker; i++){
		atomic_set(&white_msg_sent[i], 0);
	}
}


void broadcast_gvt_init(unsigned int round){
	unsigned int i;

	gvt_init_round = round;

	for(i = 0; i < n_ker; i++){
		if(i==kid)
			continue;
		if(!is_request_completed(&(gvt_init_reqs[i]))){
			rootsim_error(true, "Failed to send new GVT init round to kernel %u, "
					"because the old init request is still pending\n");
		}
		lock_mpi();
		MPI_Isend((const void *)&gvt_init_round, 1, MPI_UNSIGNED, i, MSG_NEW_GVT, MPI_COMM_WORLD, &gvt_init_reqs[i]);
		unlock_mpi();
	}
}


bool gvt_init_pending(void){
	return pending_msgs(MSG_NEW_GVT);
}


void gvt_init_clear(void){
	unsigned int new_gvt_round;
	lock_mpi();
	MPI_Recv(&new_gvt_round, 1, MPI_UNSIGNED, MPI_ANY_SOURCE, MSG_NEW_GVT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	unlock_mpi();
}


void join_gvt_redux(simtime_t local_vt){
	local_vt_buff = local_vt;
	lock_mpi();
	MPI_Iallreduce(&local_vt_buff, &reduced_gvt, 1, MPI_DOUBLE, MPI_MIN, gvt_reduction_comm, &gvt_reduction_req);
	unlock_mpi();
}


bool gvt_redux_completed(void){
	if(!spin_trylock(&gvt_reduction_lock)) return false;

	bool compl = false;
	compl = is_request_completed(&gvt_reduction_req);

	spin_unlock(&gvt_reduction_lock);
	return compl;
}


simtime_t last_reduced_gvt(void){
	return reduced_gvt;
}


void register_outgoing_msg(const msg_t* msg){
	unsigned int dst_kid = GidToKernel(msg->receiver);

	if(dst_kid == kid) return;

	if(is_red_colour(msg->colour)){
		min_outgoing_red_msg[local_tid] = min(min_outgoing_red_msg[local_tid], msg->timestamp);
	}else{
		atomic_inc(&white_msg_sent[dst_kid]);
	}
}


void register_incoming_msg(const msg_t* msg){
	unsigned int src_kid = GidToKernel(msg->sender);

	if(src_kid == kid) return;

	if(msg->colour == white_0){
		atomic_inc(&white_0_msg_recv);
	}else if(msg->colour == white_1){
		atomic_inc(&white_1_msg_recv);
	}
}


#endif
