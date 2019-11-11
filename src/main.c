/**
*                       Copyright (C) 2008-2019 HPDCS Group
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
* @file main.c
* @brief This module contains the entry point for the simulator, and some core functions
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/

#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>

#ifdef HAVE_NUMA
#include <numa.h>
#endif

#include <core/core.h>
#include <arch/thread.h>
#include <statistics/statistics.h>
#include <gvt/ccgs.h>
#include <scheduler/binding.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/mm.h>
#include <score/score.h>

#ifdef HAVE_CROSS_STATE
#include <mm/ecs.h>
#endif

#include <serial/serial.h>
#include <communication/mpi.h>

#define _INIT_FROM_MAIN
#include <core/init.h>
#undef _INIT_FROM_MAIN

int controller_committed_events = 0;
atomic_t final_processed_events;
__thread int my_processed_events = 0;
timer threads_config_timer;
double check_interval = 0.5;
static int reassign = false;

/// How many CTs have been stopped for thread reassignation (needed for the PT)
static atomic_t CTs_to_stop;

/// How many CTs have been stopped for thread reassignation (needed for the PT)
static atomic_t PTs_to_stop;


///add or remove CTs
static int thread_configuration_modifier;


/**
 * This jump buffer allows rootsim_error, in case of a failure, to jump
 * out of any point in the code to the final part of the loop in which
 * all threads synchronize. This avoids side effects like, e.g., accessing
 * a NULL pointer.
 */

jmp_buf exit_jmp;

/**
* This function checks the different possibilities for termination detection termination.
*/
static bool end_computing(void) {

	// Did CCGS decide to terminate the simulation?
	if (ccgs_can_halt_simulation()) {
		return true;
	}
	// Termination detection based on passed (committed) simulation time
	if (rootsim_config.simulation_time != 0 && (int)get_last_gvt() >= rootsim_config.simulation_time) {
		return true;
	}
	// If some KLT has encountered an error condition, we neatly shut down the simulation
	if (simulation_error()) {
		return true;
	}

	if (user_requested_exit())
		return true;

	return false;
}


static void finish(void) {

    thread_barrier(&all_thread_barrier);
    
    if (simulation_error()) {  //If we're exiting due to an error, we neatly shut down the simulation
        simulation_shutdown(EXIT_FAILURE);
    }
    simulation_shutdown(EXIT_SUCCESS);
}

static void wait(void){
    thread_barrier(&all_thread_barrier);
}

static void symmetric_execution(void) {

    simtime_t my_time_barrier = -1.0;

    #ifdef HAVE_CROSS_STATE
    lp_alloc_thread_init();
    #endif

    initialize_worker_thread();  //Do the initial (local) LP binding, then execute INIT at all (local) LPs

    #ifdef HAVE_MPI
    syncronize_all();
    #endif


    if (master_kernel() && master_thread()) {  //Notify the statistics subsystem that we are now starting the simulation
        statistics_start();
        printf("****************************\n"
               "*    Simulation Started    *\n"
               "****************************\n");
    }

    if (setjmp(exit_jmp) != 0) {
        finish();
    }

    while (!end_computing()) {

        #ifdef HAVE_POWER_MANAGEMENT
        if(master_thread()){
            powercap_state_machine();
        }
        #endif

        rebind_LPs();  //Recompute the LPs-thread core_binding

        #ifdef HAVE_MPI
        receive_remote_msgs();   // Check whether we have new ingoing messages sent by remote instances
        //            prune_outgoing_queues();
        #endif

        process_bottom_halves();  //Forward the messages from the kernel incoming message queue to the destination LPs

        schedule();  //Activate one LP and process one event. Send messages produced during the events' execution

        my_time_barrier = gvt_operations();

        // Only a master thread on master kernel prints the time barrier
        if (master_kernel() && master_thread() && D_DIFFER(my_time_barrier, -1.0)) {
            if (rootsim_config.verbose == VERBOSE_INFO || rootsim_config.verbose == VERBOSE_DEBUG) {
                #ifdef HAVE_PREEMPTION
                printf("TIME BARRIER %f - %d preemptions - %d in platform mode - %d would preempt\n", my_time_barrier,
                        atomic_read(&preempt_count), atomic_read(&overtick_platform), atomic_read(&would_preempt));
                #else
                printf("TIME BARRIER %f\n", my_time_barrier);
                #endif
                fflush(stdout);
            }
        }
        #ifdef HAVE_MPI
        collect_termination();
        #endif
    }
    finish();
}

static void asymmetric_execution(void) {

    simtime_t my_time_barrier = -1.0;
    timer_start(threads_config_timer);

    if(master_kernel() && master_thread ()){
        atomic_set(&CTs_to_stop, rootsim_config.num_controllers);
        atomic_set(&PTs_to_stop, n_cores-rootsim_config.num_controllers);
    }

    if (Threads[tid]->incarnation == THREAD_CONTROLLER) {

        initialize_worker_thread();//Do the initial (local) LP binding, then execute INIT at all (local) LPs

        #ifdef HAVE_MPI
        syncronize_all();
        #endif

        CONTROLLER_THREAD:

        while (!end_computing()) {

            reset_score();

            // We assume that thread with tid 0 should be a controller.
            // Should be adapted for MPI.

            #ifdef HAVE_POWER_MANAGEMENT
            if(master_thread()){
                powercap_state_machine();
            }
            #endif

            rebind_LPs();  //Recompute the LPs-thread binding

            #ifdef HAVE_MPI
            // Check whether we have new ingoing messages sent by remote instances
            receive_remote_msgs();
            prune_outgoing_queues();
            #endif

            update_last_processed();

            asym_extract_generated_msgs();  //Read output ports of all bound PTs

            process_bottom_halves();  //Forward the messages from the kernel incoming message queue to the destination LPs

            asym_schedule();  //Activate one LP and process one event. Send messages produced during the events' execution

            my_time_barrier = gvt_operations();

            // Only a master thread on master kernel prints the time barrier
            if (master_kernel() && master_thread () && D_DIFFER(my_time_barrier, -1.0)) {
                if (rootsim_config.verbose == VERBOSE_INFO || rootsim_config.verbose == VERBOSE_DEBUG) {

                    #ifdef HAVE_PREEMPTION
                    printf("TIME BARRIER %f - %d preemptions - %d in platform mode - %d would preempt\n", my_time_barrier, atomic_read(&preempt_count), atomic_read(&overtick_platform), atomic_read(&would_preempt));
                    #else
                    fprintf(stdout,"TIME BARRIER %f, # OF CONTROLLER THREADS:%d\n", my_time_barrier, rootsim_config.num_controllers);
                    #endif

                    //fprintf(stdout,"\tPorts-> ");
                    unsigned int i;
                    for(i = 0; i < n_cores; i++) {
                        if(Threads[i]->incarnation == THREAD_PROCESSING){
                            unsigned int port_curr_size = get_port_current_size(Threads[i]->input_port[PORT_PRIO_LO]);
                            //fprintf(stdout,"PT%d: %d/%d | ",i, port_curr_size, Threads[i]->port_batch_size);
                        }
                    }
                    //fprintf(stdout,"\n");
                    //fflush(stdout); 
                }
            }

            #ifdef HAVE_MPI
            collect_termination();
            #endif

            if (master_kernel() && master_thread () && is_idle() ) {
                if(get_score() >= SCORE_HIGHER_THRESHOLD && rootsim_config.num_controllers+1<= n_cores/2){
                   // printf(">= SCORE_HIGHER_THRESHOLD - SCORE: %d\n",get_score());
                    thread_configuration_modifier = 1;
                }
                else if(get_score() <= SCORE_LOWER_THRESHOLD && rootsim_config.num_controllers-1 >= 1){
                   // printf("<= SCORE_LOWER_THRESHOLD - SCORE: %d\n",get_score());
                    thread_configuration_modifier = -1;
                }
                else{
                    thread_configuration_modifier = 0;
                }

                if (reassign == false && thread_configuration_modifier!=0 ){
                    reassign = true;
                   // fprintf(stdout,"\nREASSIGNATION... \n");
                }
                else if (reassign == true){
                    printf("'false' reassign status expected\n");
                    abort();
                }
            }

            if(reassign == true) {
                atomic_dec(&CTs_to_stop);

                while(atomic_read(&PTs_to_stop) != 0);

                asym_extract_generated_msgs();

                if(check_output_channels_emptiness()==false) {
                    printf("Channels are not empty\n");
                    abort();
                }

                wait();
                if(master_kernel() && master_thread ()) {
                    threads_reassign(thread_configuration_modifier);
                    update_participants();
                   // fprintf(stdout, "REASSIGNATION DONE !\n\n");
                    reassign = false;
                    atomic_set(&CTs_to_stop, rootsim_config.num_controllers);
                    atomic_set(&PTs_to_stop, n_cores-rootsim_config.num_controllers);

                    timer_restart(threads_config_timer);
                    wait();
                }
                else {
                    wait();
                }

                if(Threads[local_tid]->incarnation == THREAD_PROCESSING){
                    goto PROCESSING_THREAD;
                }

                reassignation_rebind();
            }
        }

        finish();
    }


    if (Threads[local_tid]->incarnation == THREAD_PROCESSING) {

        #ifdef HAVE_CROSS_STATE
        lp_alloc_thread_init();
        #endif

        PROCESSING_THREAD:

        while (!end_computing()) {
            asym_process();

            if(are_input_channels_empty(local_tid)){
                if(reassign == true && atomic_read(&CTs_to_stop) == 0){
                    atomic_dec(&PTs_to_stop);

                    while(get_port_current_size(Threads[local_tid]->output_port) != 0);
                    if(are_input_channels_empty(local_tid)&&is_out_channel_empty(tid)){
                        wait();
                    }
                    else {
                        printf("OUTPUT or INPUT CHANNELS NOT EMPTY\n");
                        abort();
                    }
                    wait();
                    if(Threads[local_tid]->incarnation == THREAD_CONTROLLER){
                        reassignation_rebind();
                        update_GVT();
                        goto CONTROLLER_THREAD;
                    }
                }
            }
        }
        atomic_add(&final_processed_events, my_processed_events);
        finish();
    }
}

#ifdef HAVE_PREEMPTION
extern atomic_t preempt_count;
extern atomic_t overtick_platform;
extern atomic_t would_preempt;
#endif

/**
* This function implements the main simulation loop
*
* @param arg This argument should be always set to NULL
*/

static void *main_simulation_loop(void *arg) __attribute__((noreturn));
static void *main_simulation_loop(void *arg) {
    (void)arg;
    
    enum thread_incarnation incarnation = Threads[local_tid]->incarnation;

    if(incarnation == THREAD_SYMMETRIC) {
        symmetric_execution();
    }

    else if(incarnation == THREAD_PROCESSING || incarnation == THREAD_CONTROLLER){
        asymmetric_execution();
    }

    else {
        fprintf(stderr, "\n%s:%d: ERROR: unknown incarnation for thread %d\n", __FILE__, __LINE__, tid);
        abort();
    }
}

/**
* This function implements the ROOT-Sim's entry point
*
* @param argc Number of arguments passed to ROOT-Sim
* @param argv Arguments passed to ROOT-Sim
*
* @return Exit code
*/
int main(int argc, char **argv)
{
#ifdef HAVE_MPI
	volatile int __wait = 0;
	char hostname[256];

		if ((getenv("WGDB")) != NULL && *(getenv("WGDB")) == '1') {
			gethostname(hostname, sizeof(hostname));
			printf("PID %d on %s ready for attach\n", getpid(), hostname);
			fflush(stdout);

			while (__wait == 0)
					sleep(5);
	}
	#endif

	#ifdef HAVE_NUMA
	if(numa_available() < 0) {
		fprintf(stderr, "Your system does not support NUMA API\n");
		exit(EXIT_FAILURE);
	}
	#endif

	SystemInit(argc, argv);

	if (rootsim_config.core_binding)
		set_affinity(0);

	if (rootsim_config.serial) {
		serial_simulation();
	} else {
		// The number of locally required threads is now set. Detach them and then join the main simulation loop
		if (!simulation_error()) {
			if (n_cores > 1) {
				create_threads(n_cores - 1, main_simulation_loop, NULL);
			}

			main_simulation_loop(NULL);
		}
	}

	return 0;
}
