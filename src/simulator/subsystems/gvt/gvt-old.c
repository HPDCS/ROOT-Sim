/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file gvt-old.c
* @brief This module implements the old GVT operations. Currently it's not compiled
*        compiled into the final executable, it's kept only for historical reasons.
*        This file will be removed in future revisions of the simulator.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/

#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <mm/malloc.h>
#include <arch/thread.h>
#include <arch/atomic.h>
#include <scheduler/process.h>

// Serve per delle macro di messaggi MPI scambiati. Sarebbe più carino toglierlo
#include <communication/communication.h>

// Va tolto
#include <arch/thread.h>


/// Each worker thread notifies its current minimum LVT using this array
simtime_t *min_bound_per_thread;

simtime_t *min_in_transit_per_thread;

/// This lock is used for the critical section around final GVT reduction on this node
spinlock_t *worker_thread_gvt_lock;
spinlock_t gvt_global_lock;


__thread simtime_t last_adopted_gvt;
simtime_t fresh_gvt;

timer gvt_timer;
int *gvt_messages_sent;
int *gvt_messages_rcvd;


/// This variables count the number of gvt computed, as the gvt now is computed with a periodic behavior it can be used as a wall-click-time reference
unsigned int	num_gvt;

/// This global variable holds the new GVT value, upon GVT operations
//simtime_t new_gvt = -1;

/// This global variable tells if the gvt request included also the snapshot computation
static bool compute_snapshot;

//FILE 		*f_gvt_stat; // TOD: questo andrebbe spostato nel sottosistema statistiche










/// This global variable tells if we are computing GVT
static volatile bool computing_gvt = false;

/// This barrier is used to synchronize KLT during the GVT operations
static barrier_t gvt_barrier;

/// This is the variable the keeps the current time barrier value during the computation
static simtime_t new_gvt_value = INFTY;




/**
* Initializer of the GVT subsystem
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @author Francesco Quaglia
*
*/
void gvt_init(void) {
	unsigned int i;

	gvt_messages_sent = (int *)rsalloc(sizeof(int) * n_ker);
	gvt_messages_rcvd = (int *)rsalloc(sizeof(int) * n_ker);

	for(i = 0; i < n_ker; i++) {
		gvt_messages_sent[i] = 0;
		gvt_messages_rcvd[i] = 0;
	}

	min_bound_per_thread = (simtime_t *)rsalloc(sizeof(simtime_t) * n_cores);
	min_in_transit_per_thread = (simtime_t *)rsalloc(sizeof(simtime_t) * n_cores);
	worker_thread_gvt_lock = (spinlock_t *)rsalloc(sizeof(spinlock_t) * n_cores);	
	for(i = 0; i < n_cores; i++) {
		min_bound_per_thread[i] = 0.0;
		min_in_transit_per_thread[i] = INFTY;
		spinlock_init(&worker_thread_gvt_lock[i]);
	}
	spinlock_init(&gvt_global_lock);


	start_ack_timer();
	// Timer for re-entering in the GVT phase
	timer_start(gvt_timer);

	num_gvt = 0;
	
	barrier_init(&gvt_barrier, n_cores);
}



/**
* Finalizer of the GVT subsystem
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/
void gvt_fini(void){
	rsfree(gvt_messages_sent);
	rsfree(gvt_messages_rcvd);
	rsfree(min_bound_per_thread);
	rsfree(min_in_transit_per_thread);
	rsfree(worker_thread_gvt_lock);
}


static simtime_t reduce_gvt(void) {
	simtime_t min_bound = INFTY;
	simtime_t min_transit = INFTY;
	unsigned int i;

	spin_lock(&gvt_global_lock);

	for(i = 0; i < n_cores; i++) {
		min_bound = min(min_bound, min_bound_per_thread[i]);
		min_transit = min(min_transit, min_in_transit_per_thread[i]);
	}
	
	spin_unlock(&gvt_global_lock);

	return min(min_bound, min_transit);
}


void update_bound(simtime_t bound_time) {
	
	if(spin_trylock(&gvt_global_lock)) {
		min_bound_per_thread[tid] = bound_time;
		spin_unlock(&gvt_global_lock);
	}
	
}

void update_in_transit(unsigned int last_scheduled) {
	unsigned int i;

	spin_lock(&gvt_global_lock);
	

	for(i = 0; i < n_cores; i++) {
		if(LPS[last_scheduled]->outgoing_buffer.min_in_transit[i] < min_in_transit_per_thread[i]) {
			min_in_transit_per_thread[i] = LPS[last_scheduled]->outgoing_buffer.min_in_transit[i];
		}
		LPS[last_scheduled]->outgoing_buffer.min_in_transit[i] = INFTY;
	}
	
	spin_unlock(&gvt_global_lock);
}


static int flush_in_transit;
static void flush_in_transit_bound(void) {
	int cycle = (tid + 1) * ((n_prc_per_thread / 10) + 1);

	flush_in_transit++;

	if(flush_in_transit - cycle * (flush_in_transit / cycle) != 0) {
		return;
	}

	if(spin_trylock(&gvt_global_lock)) {
		if(min_in_transit_per_thread[tid] < min_bound_per_thread[tid]) {
			min_bound_per_thread[tid] = min_in_transit_per_thread[tid];
		}
		min_in_transit_per_thread[tid] = INFTY;
		spin_unlock(&gvt_global_lock);
	}
}


__thread unsigned int tot_committed_per_th = 0;



/**
* This function is used by slave Kernels to receive the new GVT value from the Master Kernel
*
* @author Francesco Quaglia
*/
/*simtime_t receive_gvt(void) {
	int should_check;
	static int first_snapshot_taken = 0;
	unsigned int i;
	simtime_t my_time_barrier = -1.;

	// It is useless to execute this function if we are not in the GVT phase
	if (!computing_gvt)
		return my_time_barrier;


	do {
//		comm_probe(MPI_ANY_SOURCE, MSG_NEW_GVT, MPI_COMM_WORLD, &should_check, MPI_STATUS_IGNORE);

	} while(should_check == false && rootsim_config.blocking_gvt);


	if(should_check) {
		// Receive the reply
//		comm_recv(&new_gvt, sizeof(new_gvt), MPI_CHAR, 0, MSG_NEW_GVT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		send_forced_ack();

		// Adopt the newly computed GVT
		my_time_barrier = adopt_new_gvt();

		// Exclude the transient from the measurement of committed events
		if (first_snapshot_taken == 0) {

			int delta_simulation_timer = timer_value(simulation_timer);	
	
			if (abs(delta_simulation_timer) > (int) STARTUP_TIME) {
				first_snapshot_taken = 1;
		
				for (i = 0; i < n_prc; i++) {
//					memcpy(&LPS[i]->first_snapshot, &LPS[i]->last_snapshot, sizeof(state_t));
					rootsim_error(true, "This is a case not yet covered in the reimplementation!\n");
				}
			}
		}

		receive_ack();

		// Reset the state for the next GVT computation phase
		compute_snapshot = false;
		computing_gvt = false;
	}
	return my_time_barrier;
}
*/








































#if 0


/**
* This function is used by slave Kernels to check whether the GVT calculation phase should
* start or not.
*
* @author Francesco Quaglia
*/
void start_computation_slave(void) {
/*	int should_check;

	// Do we have to check for a message from the Master Kernel?
	comm_probe(0, MSG_COMPUTE_GVT, MPI_COMM_WORLD, &should_check, MPI_STATUS_IGNORE);
	if (should_check) {

		// Cannot be asked to compute GVT if I'm already computing it
		if (computing_gvt)
			rootsim_error(true, "I'm asked to compute GVT, but I'm already computing it...\n");


		new_gvt_value = INFTY;
		computing_gvt = true;


		// Get the message from the Kernel, stating whether I have to compute the snapshot or not
		comm_recv(&compute_snapshot,sizeof(compute_snapshot), MPI_CHAR, 0, MSG_COMPUTE_GVT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		
		// Compute the minumum timestamp for this node and send it back to Master Kernel
		simtime_t min_ts = local_min_timestamp();
		comm_basic_send(&min_ts, sizeof(simtime_t), MPI_CHAR, 0, MSG_INFO_GVT, MPI_COMM_WORLD);
	}
*/
}





/**
* This function is used by Master Kernel to collect replies to its GVT operation request.
*
* @author Francesco Quaglia
*/
simtime_t receive_replies(void) {

	int should_check = 0; // Should check is int rather than bool 'cause it's set by MPI
	static simtime_t final_reduction = INFTY;
	static unsigned int missing_replies = 0;
	simtime_t received_barrier;
	register unsigned int i;
	
	// It is pointless to execute this function, if we are not in the GVT phase
	if (!computing_gvt)
		return -1.0;

	// If there is only one kernel, no MPI communication is required
	if(n_ker > 1) {
		
		// If this is the first invocation of a phase, update the missing replies and the current final reduction
		if(missing_replies == 0) {
			missing_replies = n_ker - 1;
			final_reduction = INFTY;
		}
		
		do { // GVT computation can be either blocking or non-blocking
			
			// Check if a reply exists only if such a message has been received
			comm_probe(MPI_ANY_SOURCE, MSG_INFO_GVT, MPI_COMM_WORLD, &should_check, MPI_STATUS_IGNORE);
			if(!should_check) {
				break;
			}
			
			// Receive one reply
			comm_recv(&received_barrier, sizeof(simtime_t), MPI_CHAR, MPI_ANY_SOURCE, MSG_INFO_GVT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			missing_replies--;
			
			// Update the final reduction
			if(!D_EQUAL(received_barrier, -1.0)) {
				final_reduction = min(final_reduction, received_barrier);
			}
					
		} while(rootsim_config.blocking_gvt);
	}
	
	// If we have received all the replies (or if no replies where needed)
	if(missing_replies == 0) {
		
		// If there is more than one kernel, we must communicate the new GVT
		if(n_ker > 1) {
			for(i = 1; i < n_ker; i++) {
				comm_basic_send(&final_reduction, sizeof(simtime_t), MPI_CHAR, i, MSG_NEW_GVT, MPI_COMM_WORLD);
			}
		}
		
		// We can now return the final reduction
		return final_reduction;
	} else {
		return -1.0;
	}



/*

	do {
		
				
				send_forced_ack();
	
				// Adopts the newly computed GVT
				my_time_barrier = adopt_new_gvt();
	
				// Collects the states computed by the slaves
				if (compute_snapshot)
					collect_states();
	
				// Exclude the transient from the measurement of committed events
				if (first_snapshot_taken == 0) {

					int delta_simulation_timer = timer_value(simulation_timer);	
	
					if (abs(delta_simulation_timer) > (int) STARTUP_TIME) {
						first_snapshot_taken = 1;
		
						for (i = 0; i < n_prc; i++) {
//							memcpy(&LPS[i]->first_snapshot, &LPS[i]->last_snapshot, sizeof(state_t));
							rootsim_error(true, "This is a case not yet covered in the reimplementation!\n");
						}
		
					}
				}

				receive_ack();
	
				// Reset the state for the next GVT computation phase
				computing_gvt = false; // GVT phase has ended
				return my_time_barrier;
			}
		}
	} while(;

	return my_time_barrier;
*/
}


/**
* This function is used by the Master Kernel to start the GVT calculation phase.
* If the GVT computation is selected to be non blocking, after having computed the local
* minimum it gets back to event execution, even though the other instances are still
* computing their minima.
*
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @todo If compute snapshot is false wy it is sent to all kernels?
*/
static bool start_computation_master(void) {
	register unsigned int i;
	static int snapshot_cycles;

	// Get the time passed from the last GVT operation
	int delta_gvt_timer = timer_value(gvt_timer);

	// We enter the GVT computation if enough time has passed
	if ((abs(delta_gvt_timer) > (int)rootsim_config.gvt_time_period) && (computing_gvt == false)) {

		timer_restart(gvt_timer);
		
		// If the GVT reduction takes too long, or if the interval is too short, the previous iteration might still be running...
		if (computing_gvt) {
			rootsim_error(false, "The GVT interval has passed, but the previous reduction is not completed yet. Skipping...\n");
			return true; // we must continue the computation
		}

		// Snapshot should be recomputed only periodically
		snapshot_cycles++;
		compute_snapshot = ((snapshot_cycles % rootsim_config.gvt_snapshot_cycles) == 0);
		

		// TODO: rimpiazzare con una broadcast!
		// Master sends the other kernel the request for snapshot computation
		for(i = 1; i < n_ker; i++) {
			comm_basic_send(&compute_snapshot, sizeof(compute_snapshot), MPI_CHAR, i, MSG_COMPUTE_GVT, MPI_COMM_WORLD);
		}
		
		new_gvt_value = INFTY;
			
		return true;
	}
	
	return false;
}








/**
* This function is called by the main loop to perform gvt operations
*
* @author Francesco Quaglia
*
*/
simtime_t gvt_operations(void) {
	static int snapshot_cycles = 0;
	int delta_gvt_timer;


	flush_in_transit_bound();

	if(master_thread()) {

		// Get the time passed from the last GVT operation
        	delta_gvt_timer = timer_value(gvt_timer);

	        // We enter the GVT computation if enough time has passed
	        if ((abs(delta_gvt_timer) > (int)rootsim_config.gvt_time_period)) {

	                timer_restart(gvt_timer);

	                // Snapshot should be recomputed only periodically
        	        snapshot_cycles++;
//                	compute_snapshot = ((snapshot_cycles % rootsim_config.gvt_snapshot_cycles) == 0);
			compute_snapshot = snapshot_cycles - rootsim_config.gvt_snapshot_cycles * (snapshot_cycles / rootsim_config.gvt_snapshot_cycles);

			last_adopted_gvt = fresh_gvt = reduce_gvt();

			return adopt_new_gvt();
        	}

	} else {

		if(fresh_gvt > last_adopted_gvt) {

			last_adopted_gvt = fresh_gvt;

			adopt_new_gvt();
				
		}
	}


	return -1.0;
	
}



simtime_t gvt_operations_old(void) {
	simtime_t others_time_barrier = -1.0;
	simtime_t adopted_gvt;
	
	simtime_t new_way_of_gvt;
	unsigned int i;
	
	// If the master kernel asked to compute the GVT, other KLT should wait for it to finish
	if(!master_thread()) {
		if(n_cores > 1 && computing_gvt) {
			// Two barrier calls here: the first one allows the master thread to know
			// that no other KLT is processing events. The second one is used by
			// other KLTs to wait for the master thread to finish computing the GVT
			thread_barrier(&gvt_barrier);
			thread_barrier(&gvt_barrier);
		}
		return -1.0;
	}
	
	if(master_kernel() && master_thread()) {

		// Shall we start the GVT phase?
		computing_gvt = start_computation_master();
		
		if(!computing_gvt) {
			return -1.0;
		}
		
		if(n_cores > 1 && computing_gvt) {
			
			// All threads must synchronize. Only master thread can reduce the GVT
			// Wait for other KLTs to stop processing events.
			thread_barrier(&gvt_barrier);
		}

		// If we have to compute GVT, do local operations and acquire other values
		new_gvt_value = local_min_timestamp();
		
		new_way_of_gvt = reduce_gvt();
		
		printf("I due minimi sono: nuovo %f vecchio %f\n", new_way_of_gvt, new_gvt_value);
		
		
		
		others_time_barrier = receive_replies();
		new_gvt_value = min(new_gvt_value, others_time_barrier);

	} else {
/*
		// Check whether there is a running GVT phase, and in the case enter it
		start_computation_slave();

		// Slaves can always receive a MSG_NEW_GVT message
		my_time_barrier = receive_gvt();
*/
	}
	
	// Unlock other KLTs. When running with 1 thread, it's an un-needed costly operation
	if(n_cores > 1) {
		thread_barrier(&gvt_barrier);
	}

	adopted_gvt = adopt_new_gvt();
	
	return adopted_gvt;
}

#endif
