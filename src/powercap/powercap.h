#include <unistd.h>
/**
 * @file powercap/powercap.h
 *
 * @brief Power Cap Management Module
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
 * @author Stefano Conoci
 */


#pragma once

/////////////////////////////////////////////////////////////////
//	Macro definitions
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//	Includes 
/////////////////////////////////////////////////////////////////

#include "stats_t.h"
#include  <stdio.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>

#include <arch/thread.h>
#include <scheduler/scheduler.h>

extern int* running_array;				// Array of integers that defines if a thread should be running
extern pthread_t* pthread_ids;			// Array of pthread id's to be used with signals
extern int total_threads;				// Total number of threads that could be used by the transcational operation
extern volatile int powercap_active_threads;	// Number of currently active threads, reflects the number of 1's in running_array
extern int nb_cores; 					// Number of cores. Detected at startup and used to set DVFS parameters for all cores
extern int nb_packages;					// Number of system package. Necessary to monitor energy consumption of all packages in th system
extern int cache_line_size;				// Size in byte of the cache line. Detected at startup and used to alloc memory cache aligned
extern int* pstate;						// Array of p-states initialized at startup with available scaling frequencies
extern int max_pstate;					// Maximum index of available pstate for the running machine
extern int current_pstate;				// Value of current pstate, index of pstate array which contains frequencies
extern int total_commits_round; 		// Number of total commits for each heuristics step
extern int starting_threads;			// Number of threads running at the start of the heuristic search. Defined in config.txt
extern int static_threads;				// Immutable value, initially set as the same value of starting_threads 
extern int static_pstate;				// Static -state used for the execution with heuristic 8. Defined in config.txt
extern int steps;						// Number of steps required for the heuristic to converge
extern int exploit_steps;				// Number of steps that should be waited until the next exploration is started. Defined in config.txt
extern int current_exploit_steps;		// Current number of steps since the last completed exploration
extern double extra_range_percentage;	// Defines the range in percentage over power_limit which is considered valid for the HIGH and LOW configurations. Used by dynamic_heuristic1. Defined in config.txt
extern int window_size; 				// Defines the lenght of the window, defined in steps, that should achieve a power consumption within power_limit. Used by dynamic_heuristic1. Defined in config.txt
extern double hysteresis;				// Defines the amount in percentage of hysteresis that should be applied when deciding the next step in a window based on the current value of window_power. Used by dynamic_heuristic1. Defined in config.txt
extern double power_uncore;				// System specific parameter that defines the amount of power consumption used by the uncore part of the system, which we consider to be constant. Defined in config.txt

extern stats_t** stats_array;			// Pointer to pointers of struct stats_s, one for each thread
extern volatile int round_completed;	// Defines if round completed and thread 0 should collect stats and call the heuristic function
extern double** power_profile; 			// Power consumption matrix of the machine. Precomputed using profiler.c included in root folder.
										// Rows are threads, columns are p-states. It has total_threads+1 rows as first row is filled with 0 for the profile with 0 threads
extern double power_limit;				// Maximum power that should be used by the application expressed in Watt. Defined in config.txt
extern double energy_per_tx_limit;		// Maximum energy per tx that should be drawn by the application expressed in micro Joule. Defined in config.txt
extern int heuristic_mode;				// Used to switch between different heuristics mode. Can be set from 0 to 14.
extern volatile int shutdown;			// Used to check if should shutdown
extern long effective_commits; 			// Number of commits during the phase managed by the heuristics. Necessary due to the delay at the end of execution with less than max threads
extern int detection_mode; 				// Defines the detection mode. Value 0 means detection is disabled. 1 restarts the exploration from the start. Detection mode 2 resets the execution after a given number of steps. Defined in config.txt and loaded at startup
extern int core_packing;				// 0-> threads scheduling, 1 -> core packing
extern int lower_sampled_model_pstate;	// Define the lower sampled pstate to compute the model
extern int throughput_measure;			// Defines how the throughput is measured. This is specific for time warp executions. If set to 0, it relies on regular GVT computation, value set to 1 means that it is computed as an estimation from the rate of forward event processed and rollbacks in a given period. If set to 2, the throughput estimation relies on the MACRO-MICRO sample approach 
extern int micro_period_ms;				// Defines the duration in milliseconds of the micro period in the MACRO-MICRO sample approach. Only effectively used by the heuristics when throughput_measure is set to 2. 
extern int micro_period_dly_ms;			// Defines after how many milliseconds of the macro period the micro period begins in the MACRO-MICRO sample approach. Only effectively used by the heuristics when throughput_measure is set to 2. 
extern int alternated_threads;			// Defines the number of threads of the alternated configuration used with heuristic_mode 17. 
extern int alternated_pstate;			// Defines the p-state of the alternated configuration used with heuristic_mode 17. 

// Barrier detection variables
extern int barrier_detected; 			// If set to 1 should drop current statistics round, had to wake up all threads in order to overcome a barrier
extern int pre_barrier_threads;	    	// Number of threads before entering the barrier, should be restored afterwards

// Timeline plot related variables
extern FILE* timeline_plot_file;		// File used to plot the timeline of execution
extern long time_application_startup;	// Application start time

// Statistics of the last heuristic round
extern double old_throughput;
extern double old_power;
extern double old_abort_rate;
extern double old_energy_per_tx;

// Variables that define the currently best configuration
extern double best_throughput;
extern int best_threads;
extern int best_pstate;
extern double best_power;

// Variables that define the current level best configuration. Used by HEURISTIC_MODE 0, 3, 4
extern double level_best_throughput;
extern int level_best_threads;
extern int level_best_pstate;
extern int level_starting_threads;
extern int level_starting_energy_per_tx;

// Variable to keep track of the starting configuration for phase 1 and 2 of dynamic heuristic 0 and 1 (9 and 10)
extern int phase0_pstate;
extern int phase0_threads;

// Variables used to define the state of the search
extern int new_pstate;					// Used to check if just arrived to a new p_state in the heuristic search
extern int decreasing;					// If 0 heuristic should remove threads until it reaches the limit
extern int stopped_searching;			// While 1 the algorithm searches for the best configuration, if 0 the algorithm moves to monitoring mode
extern int phase;						// The value of phase has different semantics based on the running heuristic mode


// Variables specific to dynamic_heuristic1
extern double high_throughput;
extern int high_pstate;
extern int high_threads;
extern double high_power;

extern double low_throughput;
extern int low_pstate;
extern int low_threads;
extern double low_power;

extern int current_window_slot;		// Current slot within the window
extern double window_time;				// Expressed in nano seconds. Defines the current sum of time passed in the current window of configuration fluctuation
extern double window_power; 			// Expressed in Watt. Current average power consumption of the current fluctuation window

extern int fluctuation_state;			// Defines the configuration used during the last step, -1 for LOW, 0 for BEST, 1 for HIGH

extern int boost;					    // Defines if currently boost (such as TurboBoost) is either enabled or disabled

// Variables specific to LOCK_BASED_TRANSACTIONS. Statistics taken in global variables
extern pthread_spinlock_t spinlock_variable;
extern long lock_start_time;
extern long lock_end_time;
extern long lock_start_energy;
extern long lock_end_energy;
extern long lock_commits;

// Variable specific to NET_STATS
extern long net_time_sum;
extern long net_energy_sum;
extern long net_commits_sum;
extern long net_aborts_sum;

// Variables necessary to compute the error percentage from power_limit, computed once every seconds
extern long net_time_slot_start;
extern long net_energy_slot_start;
extern long net_time_accumulator;
extern double net_error_accumulator;
extern long net_discard_barrier;

// Variables used by the binary_search heuristic
extern int min_pstate_search;
extern int max_pstate_search;

extern int min_thread_search;
extern int max_thread_search;
extern double min_thread_search_throughput;
extern double max_thread_search_throughput;

// Model-based variables

// Matrices of predicted power consumption and throughput for different configurations.
// Rows are p-states, columns are threads. It has total_threads+1 column as first column is filled with 0s
// since it is not meaningful to run with 0 threads.
extern double** power_model;
extern double** throughput_model;

extern double** power_validation;
extern double** throughput_validation;

extern double** power_real;
extern double** throughput_real;

// Variable necessary to validate the effectiveness of the models
extern int validation_pstate;

//Introduced to support heuristic on USE
extern long start_time_slot, start_energy_slot;
extern long heuristic_start_time_slot, heuristic_start_energy_slot;



extern double commits_when_exploiting;
extern double energy_when_exploiting;
extern double time_when_exploiting;

/////////////////////////////////////////////////////////////////
//	Function declerations
/////////////////////////////////////////////////////////////////


static inline void check_running_array(int threadId) {
    while(running_array[threadId] == 0){
	n_prc_per_thread = 0;
	printf("thread %d going to sleep\n", tid);
        pause();
    }
}

extern int  init_powercap_mainthread(unsigned int threads);
extern void end_powercap_mainthread(void);

extern void init_powercap_thread(unsigned int id);
extern void sample_average_powercap_violation(void);
extern int start_heuristic(double throughput);
extern int set_pstate(int input_pstate);
extern void set_threads(int to_threads);
extern void heuristic(double throughput, double power, long time);
extern void set_boost(int value);
extern int pause_thread(int thread_id);
extern int wake_up_thread(int thread_id);

enum power_stats_t {
	POWER_CONSUMPTION,
	OBSERVATION_TIME,
	EXCEEDING_CAP
};

extern double get_power_stats(enum power_stats_t stat);
extern void wake_up_sleeping_threads(void);
extern void reset_measures_for_filtering(void);
