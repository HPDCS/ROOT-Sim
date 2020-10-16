/**
 * @file powercap/powercap.c
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

/* ################################################################### *
 * POWERCAP FUNCTIONS
 * ################################################################### */


#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <core/init.h>
#include <powercap/powercap.h>
#include <powercap/stats_t.h>
#include <powercap/macros.h>

double commits_when_exploiting = 0;
double energy_when_exploiting = 0;
double time_when_exploiting = 0;



#define rsalloc(size)  \
({ \
	void *__ptr = rsalloc(5 * size);\
	if(!__ptr) {\
		printf("FUUUUUU\n");\
		abort();\
	}\
	__ptr;\
})

/////////////////////////////////////////////////////////////////
//	Global variables
/////////////////////////////////////////////////////////////////


int* running_array;				// Array of integers that defines if a thread should be running
pthread_t* pthread_ids;			// Array of pthread id's to be used with signals
int total_threads;				// Total number of threads that could be used by the transcational operation
volatile int powercap_active_threads;	// Number of currently active threads, reflects the number of 1's in running_array
int nb_cores; 					// Number of cores. Detected at startup and used to set DVFS parameters for all cores
int nb_packages;				// Number of system package. Necessary to monitor energy consumption of all packages in th system
int cache_line_size;			// Size in byte of the cache line. Detected at startup and used to alloc memory cache aligned
int* pstate;					// Array of p-states initialized at startup with available scaling frequencies
int max_pstate;					// Maximum index of available pstate for the running machine
int current_pstate;				// Value of current pstate, index of pstate array which contains frequencies
int total_commits_round; 		// Number of total commits for each heuristics step
int starting_threads;			// Number of threads running at the start of the heuristic search. Defined in config.txt
int static_threads;				// Immutable value, initially set as the same value of starting_threads 

int static_pstate;				// Static -state used for the execution with heuristic 8. Defined in config.txt
int steps;						// Number of steps required for the heuristic to converge
int exploit_steps;				// Number of steps that should be waited until the next exploration is started. Defined in config.txt
int current_exploit_steps;		// Current number of steps since the last completed exploration
double extra_range_percentage;	// Defines the range in percentage over power_limit which is considered valid for the HIGH and LOW configurations. Used by dynamic_heuristic1. Defined in config.txt
int window_size; 				// Defines the lenght of the window, defined in steps, that should achieve a power consumption within power_limit. Used by dynamic_heuristic1. Defined in config.txt
double hysteresis;				// Defines the amount in percentage of hysteresis that should be applied when deciding the next step in a window based on the current value of window_power. Used by dynamic_heuristic1. Defined in config.txt
double power_uncore;			// System specific parameter that defines the amount of power consumption used by the uncore part of the system, which we consider to be constant. Defined in config.txt

stats_t** stats_array;			// Pointer to pointers of struct stats_s, one for each thread
volatile int round_completed;   // Defines if round completed and thread 0 should collect stats and call the heuristic function
double** power_profile; 		// Power consumption matrix of the machine. Precomputed using profiler.c included in root folder.
			    // Rows are threads, columns are p-states. It has total_threads+1 rows as first row is filled with 0 for the profile with 0 threads
double power_limit;				// Maximum power that should be used by the application expressed in Watt. Defined in config.txt
double energy_per_tx_limit;		// Maximum energy per tx that should be drawn by the application expressed in micro Joule. Defined in config.txt
int heuristic_mode;				// Used to switch between different heuristics mode. Can be set from 0 to 14.
volatile int shutdown;			// Used to check if should shutdown
long effective_commits; 		// Number of commits during the phase managed by the heuristics. Necessary due to the delay at the end of execution with less than max threads
int detection_mode; 			// Defines the detection mode. Value 0 means detection is disabled. 1 restarts the exploration from the start. Detection mode 2 resets the execution after a given number of steps. Defined in config.txt and loaded at startup
int core_packing;				// 0-> threads scheduling, 1 -> core packing
int lower_sampled_model_pstate;	// Define the lower sampled pstate to compute the model
int throughput_measure;			// Defines how the throughput is measured. This is specific for time warp executions. If set to 0, it relies on regular GVT computation, value set to 1 means that it is computed as an estimation from the rate of forward event processed and rollbacks in a given period. If set to 2, the throughput estimation relies on the MACRO-MICRO sample approach 
int micro_period_ms;			// Defines the duration in milliseconds of the micro period in the MACRO-MICRO sample approach. Only effectively used by the heuristics when throughput_measure is set to 2. 
int micro_period_dly_ms;		// Defines after how many milliseconds of the macro period the micro period begins in the MACRO-MICRO sample approach. Only effectively used by the heuristics when throughput_measure is set to 2. 
int alternated_threads;			// Defines the number of threads of the alternated configuration used with heuristic_mode 17. 
int alternated_pstate;			// Defines the p-state of the alternated configuration used with heuristic_mode 17. 


// Barrier detection variables
int barrier_detected; 			// If set to 1 should drop current statistics round, had to wake up all threads in order to overcome a barrier
int pre_barrier_threads;	    // Number of threads before entering the barrier, should be restored afterwards

// Timeline plot related variables
FILE* timeline_plot_file;		// File used to plot the timeline of execution
long time_application_startup;	// Application start time

// Statistics of the last heuristic round
double old_throughput;
double old_power;
double old_abort_rate;
double old_energy_per_tx;

// Variables that define the currently best configuration
double best_throughput;
int best_threads;
int best_pstate;
double best_power;

// Variables that define the current level best configuration. Used by HEURISTIC_MODE 0, 3, 4
double level_best_throughput;
int level_best_threads;
int level_best_pstate;
int level_starting_threads;
int level_starting_energy_per_tx;

// Variable to keep track of the starting configuration for phase 1 and 2 of dynamic heuristic 0 and 1 (9 and 10)
int phase0_pstate;
int phase0_threads;

// Variables used to define the state of the search
int new_pstate;					// Used to check if just arrived to a new p_state in the heuristic search
int decreasing;					// If 0 heuristic should remove threads until it reaches the limit
int stopped_searching;			// While 1 the algorithm searches for the best configuration, if 0 the algorithm moves to monitoring mode
int phase;						// The value of phase has different semantics based on the running heuristic mode


// Variables specific to dynamic_heuristic1
double high_throughput;
int high_pstate;
int high_threads;
double high_power;

double low_throughput;
int low_pstate;
int low_threads;
double low_power;

int current_window_slot;		// Current slot within the window
double window_time;				// Expressed in nano seconds. Defines the current sum of time passed in the current window of configuration fluctuation
double window_power; 			// Expressed in Watt. Current average power consumption of the current fluctuation window

int fluctuation_state;			// Defines the configuration used during the last step, -1 for LOW, 0 for BEST, 1 for HIGH

int boost;					    // Defines if currently boost (such as TurboBoost) is either enabled or disabled

// Variables specific to LOCK_BASED_TRANSACTIONS. Statistics taken in global variables
pthread_spinlock_t spinlock_variable;
long lock_start_time;
long lock_end_time;
long lock_start_energy;
long lock_end_energy;
long lock_commits;

// Variable specific to NET_STATS
long net_time_sum;
long net_energy_sum;
long net_commits_sum;
long net_aborts_sum;

// Variables necessary to compute the error percentage from power_limit, computed once every seconds
long net_time_slot_start;
long net_energy_slot_start;
long net_time_accumulator;
double net_error_accumulator;
long net_discard_barrier;

// Variables used by the binary_search heuristic
int min_pstate_search;
int max_pstate_search;

int min_thread_search;
int max_thread_search;
double min_thread_search_throughput;
double max_thread_search_throughput;

// Model-based variables

// Matrices of predicted power consumption and throughput for different configurations.
// Rows are p-states, columns are threads. It has total_threads+1 column as first column is filled with 0s
// since it is not meaningful to run with 0 threads.
double** power_model;
double** throughput_model;

double** power_validation;
double** throughput_validation;

double** power_real;
double** throughput_real;

// Variable necessary to validate the effectiveness of the models
int validation_pstate;

//Introduced to support heuristic on USE
long start_time_slot, start_energy_slot;
long heuristic_start_time_slot, heuristic_start_energy_slot;


// Used by the heuristic 
int set_pstate(int input_pstate)
{
	if(rootsim_config.powercap <= 0.000001)
		return 0;

	int i;
	char fname[64];
	FILE* frequency_file;

	#ifdef DEBUG_OVERHEAD
		long time_heuristic_start;
		long time_heuristic_end;
		double time_heuristic_microseconds;

		time_heuristic_start = get_time();
	#endif 
	
	if(input_pstate > max_pstate)
		return -1;
		

	if(current_pstate != input_pstate){
		int frequency = pstate[input_pstate];

		for(i=0; i<nb_cores; i++){
			sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", i);
			frequency_file = fopen(fname,"w+");
			if(frequency_file == NULL){
				printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
				exit(0);		
			}		
			fprintf(frequency_file, "%d", frequency);
			fflush(frequency_file);
			fclose(frequency_file);
		}
		current_pstate = input_pstate;
	}

	#ifdef DEBUG_OVERHEAD
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("DEBUG OVERHEAD - inside set_pstate() %lf microseconds\n", time_heuristic_microseconds);
	#endif 
	
	return 0;
}

// Executed inside stm_init: sets the governor to userspace and sets the highest frequency
int init_DVFS_management(void) {

	
	char fname[64];
	char* freq_available;
	int frequency, i;
	FILE* governor_file;

	if(rootsim_config.powercap <= 0.000001)
		return 0;

	//Set governor to userspace
	nb_cores = sysconf(_SC_NPROCESSORS_ONLN);

	for(i=0; i<nb_cores;i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		governor_file = fopen(fname,"w+");
		if(governor_file == NULL){
			printf("Error opening cpu%d scaling_governor file. Must be superuser\n", i);
			exit(0);		
		}		
		fprintf(governor_file, "userspace");
		fflush(governor_file);
		fclose(governor_file);
	}

	
	// Init array of available frequencies
	FILE* available_freq_file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","r");
	if(available_freq_file == NULL){
		printf("Cannot open scaling_available_frequencies file\n");
		exit(0);
	}
	freq_available = rsalloc(sizeof(char)*256);
	fgets(freq_available, 256, available_freq_file);
	
	pstate = rsalloc(sizeof(int)*32);
	i = 0; 
	char * end;

	for (frequency = strtol(freq_available, &end, 10); freq_available != end; frequency = strtol(freq_available, &end, 10)){
		pstate[i]=frequency;
		freq_available = end;
		i++;
	}
	max_pstate = --i;

	#ifdef DEBUG_HEURISTICS
		printf("Found %d p-states in the range from %d MHz to %d MHz\n", max_pstate, pstate[max_pstate]/1000, pstate[0]/1000);
	#endif
	fclose(available_freq_file);


	set_pstate(max_pstate);
	set_boost(0);

	return 0;
}

// SIGUSR1 handler. Doesn't need to execute any code
//TODO: verify if sig is required
void sig_func(int sig){
    (void) sig;//suppress warning
}

// Executed inside stm_init

// Executed inside stm_init
//INCLUDED IN MAIN.C in start_simulation
void init_thread_management(int threads){

	char* filename;
	FILE* numafile;
	int package_last_core;
	int i;

	// Init total threads and active threads
	total_threads = threads;

	#ifdef DEBUG_HEURISTICS
		printf("Set total_threads to %d\n", threads);
	#endif

	powercap_active_threads = total_threads;

	// Init running array with all threads running 	
	running_array = rsalloc(sizeof(int)*n_cores);
	for(i=0; i<total_threads; i++)
		running_array[i] = 1;	

	// Allocate memory for pthread_ids
	pthread_ids = rsalloc(sizeof(pthread_t)*n_cores);//TODO: usa un'array inzializzato con MACRO CPU_COUNT

	//Registering SIGUSR1 handler
	signal(SIGUSR1, sig_func);

	//init number of packages
	filename = rsalloc(sizeof(char)*64);
	sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id", nb_cores-1);
	numafile = fopen(filename,"r");
	if (numafile == NULL){
		printf("Cannot read number of packages\n");
		exit(1);
	} 
	fscanf(numafile ,"%d", &package_last_core);
	nb_packages = package_last_core+1;

	#ifdef DEBUG_HEURISTICS
		printf("Number of packages detected: %d\n", nb_packages);
	#endif
}

// Used by the heuristics to tune the number of active threads 
int wake_up_thread(int thread_id)
{	
	if( running_array[thread_id] != 0){
		printf("Waking up a thread already running\n");
		return -1;
	}

	running_array[thread_id] = 2; // 2 = to be woken up
	powercap_active_threads++;
	return 0;
}

void wake_up_sleeping_threads(void)
{
	unsigned int i;

	for(i = 0; i < n_cores; i++) {
		if(running_array[i] == 2) { // 2 = to be woken up
			running_array[i] = 1;
			pthread_kill(pthread_ids[i], SIGUSR1);
			printf("woken up thread %d\n", i);
		}
	}
}

// Used by the heuristics to tune the number of active threads 
int pause_thread(int thread_id)
{
	if( running_array[thread_id] == 0 ){
		
		#ifdef DEBUG_HEURISTICS
			printf("Pausing a thread already paused\n");
		#endif

		return -1;
	}

	running_array[thread_id] = 0;
	powercap_active_threads--;
	return powercap_active_threads;
}

// Executed inside stm_init
void init_stats_array_pointer(int threads){

	// Allocate memory for the pointers of stats_t
	stats_array = rsalloc(sizeof(stats_t*)*n_cores); 

	cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

	#ifdef DEBUG_HEURISTICS
		printf("D1 cache line size: %d bytes\n", cache_line_size);
	#endif
}

// Executed by each thread inside stm_pre_init_thread
stats_t* alloc_stats_buffer(int thread_number){
	
	stats_t* stats_ptr = stats_array[thread_number];

	int ret = posix_memalign(((void**) &stats_ptr), cache_line_size, sizeof(stats_t) * 2);
	if ( ret != 0 ){ printf("Error allocating stats_t for thread %d\n", thread_number);
		exit(0);
	}

	stats_ptr->total_commits = total_commits_round/n_cores;
	stats_ptr->commits = 0;
	stats_ptr->aborts = 0;
	stats_ptr->nb_tx = 0;
	stats_ptr->start_energy = 0;
	stats_ptr->end_energy = 0;
	stats_ptr->start_time = 0;
	stats_ptr->end_time = 0;

	stats_array[thread_number] = stats_ptr;

	return stats_ptr;
}

// Executed inside stm_init to load the precomputed power profile for the current machine and put inside power_profile
// Rows are threads, columns are p-states. It has total_threads+1 rows as first row is filled with 0 for the profile with 0 threads 
void load_profile_file(void) {

	double power;
	int i;

	// Allocate the matrix 
	power_profile = (double**) rsalloc(sizeof(double*) * (n_cores+1)); 
	for (i = 0; i < (total_threads+1); i++)
		power_profile[i] = (double *) rsalloc(sizeof(double) * (max_pstate+1));

	// Init first row with all zeros 
	for(i =0; i<=max_pstate; i++){
		power_profile[0][i] = 0;
	}

	// Open file and copy to string
	FILE* profile_file = fopen("profile_file.txt","r");
	if(profile_file == NULL){
		printf("Cannot open profile_file. Execute the profiler in root folder to profile the machine power consumption\n");
		exit(0);
	}
	char* profile_string = rsalloc(sizeof(char)*8192);
	fgets(profile_string, 8192, profile_file);
	
	char * end;
	i=1;
	int j=0;
	for (power = strtod(profile_string, &end); profile_string != end && i <= total_threads; power = strtod(profile_string, &end)){
		power_profile[i][j] = power;
		profile_string = end;
		if( j == max_pstate){
			i++;
			j = 0;
		}
		else j++;
	}

	#ifdef DEBUG_HEURISTICS
		printf("Power consumption profile loaded\n");
	#endif
}


void init_model_matrices(void) {

	int i;

	// Allocate the matrices
	power_model = (double**) rsalloc(sizeof(double*) * (max_pstate+1));
	throughput_model = (double**) rsalloc(sizeof(double*) * (max_pstate+1)); 

	// Allocate the validation matrices
	power_validation = (double**) rsalloc(sizeof(double*) * (max_pstate+1));
	throughput_validation = (double**) rsalloc(sizeof(double*) * (max_pstate+1)); 

	// Allocate matrices to store real values during validation
	power_real = (double**) rsalloc(sizeof(double*) * (max_pstate+1));
	throughput_real = (double**) rsalloc(sizeof(double*) * (max_pstate+1)); 

	for (i = 0; i <= max_pstate; i++){
		power_model[i] = (double *) rsalloc(sizeof(double) * (n_cores));
		throughput_model[i] = (double *) rsalloc(sizeof(double) * (n_cores));

		power_validation[i] = (double *) rsalloc(sizeof(double) * (n_cores));
		throughput_validation[i] = (double *) rsalloc(sizeof(double) * (n_cores));

		power_real[i] = (double *) rsalloc(sizeof(double) * (n_cores));
		throughput_real[i] = (double *) rsalloc(sizeof(double) * (n_cores));
	}

	// Init first row with all zeros 
	for(i = 0; i <= max_pstate; i++){
		power_model[i][0] = 0;
		throughput_model[i][0] = 0;

		power_validation[i][0] = 0;
		throughput_validation[i][0] = 0;

		power_real[i][0] = 0;
		throughput_real[i][0] = 0;
	}
}

void load_config_file(void) {
	
	// Load config file 
	FILE* config_file;
	if ((config_file = fopen("config.txt", "r")) == NULL) {
		printf("Error opening POWERCAP configuration file.\n");
		exit(1);
	}
	if (fscanf(config_file, "STARTING_THREADS=%d STATIC_PSTATE=%d POWER_LIMIT=%lf COMMITS_ROUND=%d THROUGHPUT_MEASURE=%d MICRO_PERIOD_MS=%d MICRO_PERIOD_DLY_MS=%d HEURISTIC_MODE=%d DETECTION_MODE=%d EXPLOIT_STEPS=%d EXTRA_RANGE_PERCENTAGE=%lf WINDOW_SIZE=%d HYSTERESIS=%lf POWER_UNCORE=%lf CORE_PACKING=%d LOWER_SAMPLED_MODEL_PSTATE=%d ALTERNATED_THREADS=%d ALTERNATED_PSTATE=%d", 
			 &starting_threads, &static_pstate, &power_limit, &total_commits_round, &throughput_measure, &micro_period_ms, &micro_period_dly_ms, &heuristic_mode, &detection_mode, &exploit_steps, &extra_range_percentage, &window_size, &hysteresis, &power_uncore, &core_packing, &lower_sampled_model_pstate, &alternated_threads, &alternated_pstate)!=18) {
		printf("The number of input parameters of the POWERCAP configuration file does not match the number of required parameters.\n");
		exit(1);
	}
	static_threads = starting_threads;

	printf("THE VALUE OF throughput_measure is: %d\n", throughput_measure);

	if(throughput_measure < 0 || throughput_measure > 2){
		printf("The value throughput_measure in config.txt is set to an invalid value. Should be set to either 0, 1 or 2\n");
		exit(1);
	}

	if(micro_period_ms < 0 || micro_period_dly_ms < 0 ){
		printf("The values micro_period_ms or micro_period_dly_ms in config.txt are set to invalid values. Should be both positive integer\n");
		exit(1);
	}

	if(extra_range_percentage < 0 || extra_range_percentage > 100){
		printf("Extra_range_percentage value is not a percentage. Should be a floating point number in the range from 0 to 100\n");
		exit(1);
	}


	if(hysteresis < 0 || hysteresis > 100){
		printf("Hysteresis value is not a percentage. Should be a floating point number in the range from 0 to 100\n");
		exit(1);
	}

	// Necessary for the static execution in order to avoid running for the first step with a different frequency than manually set in config.txt
	if(heuristic_mode == 8){
		if(/*static_pstate >= 0 && */ static_pstate <= max_pstate)
			set_pstate(static_pstate);
		else 
			printf("The parameter manual_pstate is set outside of the valid range for this CPU. Setting the CPU to the slowest frequency/voltage\n");
	}else if(heuristic_mode == 12 || heuristic_mode == 13 || heuristic_mode == 15){
		set_pstate(max_pstate);
		set_threads(1);
	}

	if(core_packing != 0){
		printf("Core packing is not yet supported on STAMP\n");
		exit(1);
	}

	fclose(config_file);
}


// Returns energy consumption of package 0 cores in micro Joule
long get_energy(void) {

	if(rootsim_config.powercap <= 0.000001)
		return 1;

	#ifdef DEBUG_OVERHEAD
		long time_heuristic_start;
		long time_heuristic_end;
		double time_heuristic_microseconds;

		time_heuristic_start = get_time();
	#endif 
	
	long energy;
	int i;
	FILE* energy_file;
	long total_energy = 0;
	char fname[64];

	for(i = 0; i<nb_packages; i++){

		// Package energy consumtion
		sprintf(fname, "/sys/class/powercap/intel-rapl/intel-rapl:%d/energy_uj", i);
		energy_file = fopen(fname, "r");
		
		// Cores energy consumption
		//FILE* energy_file = fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:0/energy_uj", "r");	

		// DRAM module, considered inside the package
		//FILE* energy_file = fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:1/energy_uj", "r");	

		if(energy_file == NULL){
			printf("Error opening energy file\n");		
		}
		fscanf(energy_file,"%ld",&energy);
		fclose(energy_file);
		total_energy+=energy;
	}

	#ifdef DEBUG_OVERHEAD
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("DEBUG OVERHEAD -  Inside get_energy(): %lf microseconds\n", time_heuristic_microseconds);
	#endif 

	return total_energy;
}


// Return time as a monotomically increasing long expressed as nanoseconds 
long get_time(void) {
	
	long time =0;
	struct timespec ts;

clock_gettime(CLOCK_MONOTONIC, &ts);
time += (ts.tv_sec*1000000000);
time += ts.tv_nsec;

	return time;
}

// Function used to set the number of running threads. Based on powercap_active_threads and threads might wake up or pause some threads 
void set_threads(int to_threads)
{
	int i;

	#ifdef DEBUG_OVERHEAD
		long time_heuristic_start;
		long time_heuristic_end;
		double time_heuristic_microseconds;

		time_heuristic_start = get_time();
	#endif
	
	starting_threads = powercap_active_threads;

	if(starting_threads != to_threads){
		if(starting_threads > to_threads){
			for(i = to_threads; i<starting_threads; i++)
				pause_thread(i);
		}
		else{
			for(i = starting_threads; i<to_threads; i++)
				wake_up_thread(i);
		}
	}

	#ifdef DEBUG_OVERHEAD
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("DEBUG OVERHEAD -  Inside set_threads(): %lf microseconds\n", time_heuristic_microseconds);
	#endif 
}

// Initialization of global variables 
void init_global_variables(void) {

	#ifdef DEBUG_HEURISTICS
		printf("Initializing global variables\n");
	#endif

	round_completed=0;
	old_throughput = -1;
	old_power = -1;
	old_energy_per_tx = -1;
	level_best_throughput = -1; 
	level_best_threads = 0;
	level_starting_threads = starting_threads;
	new_pstate = 1;
	decreasing = 0;
	stopped_searching = 0;
	steps=0;
	shutdown = 0;
	effective_commits = 0;
	phase = 0; 
	current_exploit_steps = 0;
	barrier_detected = 0;
	pre_barrier_threads = 0;

	high_throughput = -1;
	high_threads = -1;
	high_pstate = -1;

	best_throughput = -1;
	best_pstate = -1;
	best_threads = -1;

	lock_commits = 0; 

	low_throughput = -1;
	low_threads = -1;
	low_pstate = -1;

	current_window_slot = 0;
	window_time = 0;
	window_power = 0;

	net_time_sum = 0;
    net_energy_sum = 0;
    net_commits_sum = 0;
	net_aborts_sum = 0;

	net_time_slot_start= 0;
    net_energy_slot_start= 0;
    net_time_accumulator= 0;
	net_error_accumulator= 0; 
	net_discard_barrier= 0;

	min_pstate_search = 0;
	max_pstate_search = max_pstate;

	min_thread_search = 1;
	max_thread_search = total_threads;
	min_thread_search_throughput = -1;
	max_thread_search_throughput = -1;

	validation_pstate = max_pstate-1;
}

// Reset all threads when reaching a barrier
void setup_before_barrier(void) {

	int i;

	//TX_GET;

	if(tid == 0 && !barrier_detected && powercap_active_threads!=total_threads) {
	
		#ifdef DEBUG_HEURISTICS
			printf("Thread 0 detected a barrier\n");
		#endif
			
		// Next decision phase should be dropped
		barrier_detected = 1;

		// Dont consider next slot for power_limit error measurements
		net_discard_barrier = 1;

		// Save number of threads that should be restored after the barrier
		pre_barrier_threads = powercap_active_threads;

		// Wake up all threads
		for(i=powercap_active_threads; i< total_threads; i++){
			wake_up_thread(i);
		}
	}
}



// Used to either enable or disable boosting facilities such as TurboBoost. Boost is disabled whenever the current config goes out of the powercap 
void set_boost(int value)
{

	if(rootsim_config.powercap <= 0.000001)
		return;

	//char fname[64];
	FILE* boost_file;

	#ifdef DEBUG_OVERHEAD
		long time_heuristic_start;
		long time_heuristic_end;
		double time_heuristic_microseconds;

		time_heuristic_start = get_time();
	#endif 
	
	if(value != 0 && value != 1){
		printf("Set_boost parameter invalid. Shutting down application\n");
		exit(1);
	}

	boost_file = fopen("/sys/devices/system/cpu/cpufreq/boost", "w+");
	fprintf(boost_file, "%d", value);
	fflush(boost_file);
	fclose(boost_file);

	#ifdef DEBUG_OVERHEAD
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("DEBUG OVERHEAD - inside set_boost() %lf microseconds\n", time_heuristic_microseconds);
	#endif 
	
	return;
}

//INCLUDED IN main.c
//inits structures and variable used by powercap subsystem.
//It has to be called before spawning threads
int init_powercap_mainthread(unsigned int threads)
{
    int i;

	#ifdef DEBUG_HEURISTICS
	    printf("TinySTM - POWERCAP mode started\n");
	#endif

	/* This seems to be useless, its all already on node 0 expect stats arrays which should be local
	// Set mem_policy to numa node 0
	if(numa_available() < 0){
	    printf("System doesn't support NUMA API\n")
	} else{
	    numa_set_preferred(0);
	    printf("Set node 0 as preferred numa node for memory allocation\n");
	}*/

	init_DVFS_management();
	init_thread_management(threads);
	init_stats_array_pointer(threads);
	load_config_file();

	if(heuristic_mode == 1 || heuristic_mode == 2)
	    load_profile_file();

	if(heuristic_mode == 15)
	    init_model_matrices();

	init_global_variables();
	set_boost(1);

	#ifdef DEBUG_HEURISTICS
	    printf("Heuristic mode: %d\n", heuristic_mode);
	#endif

	if(starting_threads > total_threads){
	    printf("Starting threads set higher than total threads. Please modify this value in config.txt\n");
	    exit(1);
	}

	// Set powercap_active_threads to starting_threads
	for(i = starting_threads; i<total_threads;i++){
	    pause_thread(i);
	}

	return starting_threads;
}

//INCLUDED IN main.c
//inits thread-structures and variable used by powercap subsystem.
//It has to be called by each thread after it has been created
void init_powercap_thread(unsigned int id)
{
	//thread_number_init = 1; //this variable does not exist in STM-POWERCAP

	//TODO: check if stats_ptr is useless
	stats_t* stats_ptr = alloc_stats_buffer(id);

	// Initialization of stats struct
	stats_ptr->total_commits = total_commits_round;

	pthread_ids[id]=pthread_self();

	if(id == 0){
	    start_time_slot = heuristic_start_time_slot = get_time();
		start_energy_slot = heuristic_start_energy_slot = get_energy();
	}

}

//INCLUDED IN core.c in main_loop
//Used to wake_up all threads
//It has to be called at the end of the simulation
void end_powercap_mainthread(void) {
    int i = 0;

    shutdown = 1;

    for(i=0; i< total_threads; i++){
		wake_up_thread(i);
	}
    wake_up_sleeping_threads();
}


//Used to comput the power consumption on a long time period
void sample_average_powercap_violation(void) {
        long end_time_slot, end_energy_slot, time_interval, energy_interval;
        double power, error_signed, error;

        end_time_slot = get_time();

        time_interval = end_time_slot - start_time_slot; //Expressed in nano seconds

        if (time_interval > 1000000000) { //If higher than 1 second update the accumulator with the value of error compared to power_limit
                end_energy_slot = get_energy();
                energy_interval = end_energy_slot - start_energy_slot; // Expressed in micro Joule
                power = ((double) energy_interval) / (((double) time_interval) / 1000);

                error_signed = power - power_limit;
                error = 0;
                if (error_signed > 0)
                        error = error_signed / power_limit * 100;

		// TODO: STATS
                net_error_accumulator = (net_error_accumulator * ((double) net_time_accumulator) + error * ((double) time_interval)) / (((double) net_time_accumulator) + ((double) time_interval));
                net_time_accumulator += time_interval;

                //Reset start counters
                start_time_slot = end_time_slot;
                start_energy_slot = end_energy_slot;
        }
}

//returns the number of threads which should be considered as active for the current configuration
int start_heuristic(double throughput) {
        long end_time_slot, end_energy_slot, time_interval, energy_interval;
        double power;

        end_time_slot = get_time();
        end_energy_slot = get_energy();
        time_interval = end_time_slot - heuristic_start_time_slot; //Expressed in nano seconds
        energy_interval = end_energy_slot - heuristic_start_energy_slot; // Expressed in micro Joule
        power = ((double) energy_interval) / (((double) time_interval) / 1000);

        // We don't call the heuristic if the energy results are out or range due to an overflow
        if (power >= 0) {
                net_time_sum += time_interval; // TODO: STATS
                net_energy_sum += energy_interval;  // TODO: STATS
                heuristic(throughput, power, time_interval);
        }

        heuristic_start_time_slot = end_time_slot;
        heuristic_start_energy_slot = end_energy_slot;

	if(current_exploit_steps > 0) {
		commits_when_exploiting += throughput * (time_interval / 1000000000.0);
		energy_when_exploiting += energy_interval / 1000000.0;
		time_when_exploiting += (time_interval / 1000000000.0);
	}

        return powercap_active_threads;
}


void reset_measures_for_filtering(void) {
        heuristic_start_time_slot = get_time();
        heuristic_start_energy_slot = get_energy();
}


double get_power_stats(enum power_stats_t stat)
{
	switch(stat) {
		case POWER_CONSUMPTION:
			return (double)net_energy_sum / ((double)net_time_sum / 1000);
		case OBSERVATION_TIME:
			return (double)net_time_sum / 1000000000;
		case EXCEEDING_CAP:
			return net_error_accumulator;
		default:
			rootsim_error(false, "Unknown statistic requested");
			return -1.0;
	}
}
