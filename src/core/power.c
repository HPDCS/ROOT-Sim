/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is rsfree software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the rsfree Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the rsfree Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file power.c
* @brief This module implements power management facilities
* @author Stefano Conoci
* @date 23/01/2018
*/

#include "power.h"
#include <mm/dymelor.h>
#include <core/init.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>

// Interval of time needed to obtain an accurate sample of energy consumption. Expressed in milliseconds.  
#define ENERGY_SAMPLE_INTERVAL 30

// Interval of milliserconds during which the average power consumption is sampled to compute the powercap error
#define POWERCAP_ERROR_INTERVAL 1000

// Percentage of hysteresis applied to the powercap 
#define HYSTERESIS 0.01

// Percentage of powercap deviation compared to the converged result after which the exploration is restarted
#define RESTART_EXPLORATION_RANGE 0.03

// Static state machine function declaration
static int static_state_machine();

// Number of logical cores. Detected at startup and used to apply DVFS setting for all cores
static int nb_cores;			

// Number of physical cores. Detected at startup and needed to allow per-core DVFS settings when HT is enabled
static int nb_phys_cores;		

// Number of system package. Necessary to monitor energy consumption of all packages in th system
static int nb_packages;			

// Array of P-states initialized at startup which associates a frequency to each P-state 
static int* pstate;				

// Index of the highest available P-state for the system 
static int max_pstate;			

// Array of P-state values for all physical cores  
static int* current_pstates; 				

// Defines if frequency boosting (such as TurboBoost) is either enabled or disabled 
static int boost;

// Current state in the powercap state machine. The semantics of this value is specific to each exploration strategy
static int exploration_state;

// The current number of controller threads 
static int current_controllers;

// Variable needed to compute intervals of wall clock time  
static long start_time;

// Variable needed to compute the amount of energy consumed in a time interval
static long start_energy;

// Variable needed to compute intervals of wall clock time to compute the powercap error
static long error_start_time;

// Variable needed to compute the amount of energy consumed in a time interval to compute the powercap error
static long error_start_energy;

// Variables needed to compute the power error at runtime
static double error_time_sum, error_weighted; 

// Extra state variable for the static state machine, indicates the power consumption of the selected configuration
static double converged_power;


//////////////////////////////////////////////////////////////////////////////
//	Power management functions 
//////////////////////////////////////////////////////////////////////////////

/**
* This function initializes variables and data structure needed for power management.
* It sets the governor to userspace, reads the number of packages available in the system,
* inits the pstate array which associated P-states to CPU frequencies and reads the
* current P-state setting for each physical core.  
*
* @Author: Stefano Conoci
*/
static int init_DVFS_management(){
	char fname[64];
	char* freq_available;
	char* filename;
	int frequency, i, package_last_core, read_frequency;
	FILE* governor_file;
	FILE* numafile;
	FILE* frequency_file;
	uint32_t registers[4];

	// Retrieve number of virtual and physical cores, checking at runtime if hyperthreading is enabled
	nb_cores = sysconf(_SC_NPROCESSORS_ONLN);
	__asm__ __volatile__ ("cpuid " :
                  "=a" (registers[0]),
                  "=b" (registers[1]),
                  "=c" (registers[2]),
                  "=d" (registers[3])
                  : "a" (1), "c" (0));

	unsigned int CPUFeatureSet = registers[3];
	unsigned int hyperthreading = CPUFeatureSet & (1 << 28);
	if(hyperthreading)
		nb_phys_cores = nb_cores/2;
	 else 
		nb_phys_cores = nb_cores;
	#ifdef DEBUG_POWER
		printf("Virtual cores = %d - Physical cores = %d\n - Hyperthreading: %d", nb_cores, nb_phys_cores, hyperthreading);
	#endif

	// Set governor of all cores to userspace
	for(i=0; i<nb_cores;i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		governor_file = fopen(fname,"w+");
		if(governor_file == NULL){
			printf("Error opening cpu%d scaling_governor file. Must be superuser\n", i);
			return(-1);		
		}		
		fprintf(governor_file, "userspace");
		fflush(governor_file);
		fclose(governor_file);
	}

	// Read number of packages in the system
	filename = rsalloc(sizeof(char)*64); 
	sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id", nb_cores-1);
	numafile = fopen(filename,"r");
	if (numafile == NULL){
		printf("Cannot read number of packages\n");
		return(-1);
	} 
	fscanf(numafile ,"%d", &package_last_core);
	nb_packages = package_last_core+1;
	rsfree(filename);

	// Init array that associates frequencies to P-states
	FILE* available_freq_file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","r");
	if(available_freq_file == NULL){
		printf("Cannot open scaling_available_frequencies file\n");
		return(-1);
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
  	rsfree(freq_available);

  	// Retrieve current P-state for all cores
  	current_pstates = rsalloc(sizeof(int)*nb_phys_cores);
	for(i=0; i<nb_phys_cores; i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", i);
		frequency_file = fopen(fname,"r");
		if(frequency_file == NULL){
			printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
			return(-1);		
		}		
		fscanf(frequency_file, "%d", &read_frequency);
		fflush(frequency_file);
		fclose(frequency_file);

		int found = 0; 
		for(int c = 0; c <= max_pstate && !found; c++){
			if(pstate[c] == read_frequency){
				found = 1;
				current_pstates[i] = c;
				printf("Core %d P-state %d\n", i, c);
			}
		}
	}

	#ifdef DEBUG_POWER
  		printf("Found %d p-states in the range from %d MHz to %d MHz\n", max_pstate, pstate[max_pstate]/1000, pstate[0]/1000);
  	#endif
  	fclose(available_freq_file);

	return 0;
}



/**
* This function sets all cores in the system to the P-state passed as parameter.
*
* @Author: Stefano Conoci
*/
static int set_pstate(int input_pstate){
	
	int i;
	char fname[64];
	FILE* frequency_file;

	#ifdef OVERHEAD_POWER
		long time_heuristic_start;
		long time_heuristic_end;
		double time_heuristic_microseconds;

		time_heuristic_start = get_time();
	#endif 
	
	if(input_pstate > max_pstate)
		return -1;
		
	int frequency = pstate[input_pstate];

	for(i=0; i<nb_cores; i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", i);
		frequency_file = fopen(fname,"w+");
		if(frequency_file == NULL){
			printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
			return(-1);		
		}		
		fprintf(frequency_file, "%d", frequency);
		if(i < nb_phys_cores)
			current_pstates[i] = input_pstate;
		fflush(frequency_file);
		fclose(frequency_file);
	}

	#ifdef OVERHEAD_POWER
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("OVERHEAD_POWER -  set_pstate() %lf microseconds\n", time_heuristic_microseconds);
	#endif 
	
	return 0;
}

/**
* This function sets the P-state of one core, and if supported its virtual sibling, to the passed value
*
* @Author: Stefano Conoci
*/	
static int set_core_pstate(int core, int input_pstate){
		
		int i;
		char fname[64];
		FILE* frequency_file;

		#ifdef OVERHEAD_POWER
			long time_heuristic_start;
			long time_heuristic_end;
			double time_heuristic_microseconds;

			time_heuristic_start = get_time();
		#endif 
		
		if(input_pstate > max_pstate)
			return -1;

		if(current_pstates[core] != input_pstate){
			int frequency = pstate[input_pstate];
			
			// Changing frequency to the physical core
			sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", core);
			frequency_file = fopen(fname,"w+");
			if(frequency_file == NULL){
				printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
				return(0);		
			}		
			fprintf(frequency_file, "%d", frequency);
			current_pstates[core] = input_pstate;
			fflush(frequency_file);
			fclose(frequency_file);

			// Changing frequency to the Hyperthreading sibling, if enabled on the system
			if(nb_cores != nb_phys_cores){
				sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", core+nb_phys_cores);
				frequency_file = fopen(fname,"w+");
				if(frequency_file == NULL){
					printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
					return(-1);		
				}		
				fprintf(frequency_file, "%d", frequency);
				fflush(frequency_file);
				fclose(frequency_file);
			}
		}

		#ifdef OVERHEAD_POWER
			time_heuristic_end = get_time();
			time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
			printf("OVERHEAD_POWER - inside set_core_pstate() %lf microseconds\n", time_heuristic_microseconds);
		#endif 
		
		return 0;
	}

/**
* This function returns the sum of the energy counters of all packages, expressed in micro Joule. 
* Can be used to compute the energy consumption in a time interval, and consequently, the power consumption. 
*
* @Author: Stefano Conoci
*/
static long get_energy(){

	#ifdef OVERHEAD_POWER
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

	// Sum the energy counters for all packages 
	for(i = 0; i<nb_packages; i++){

		sprintf(fname, "/sys/class/powercap/intel-rapl/intel-rapl:%d/energy_uj", i);
		energy_file = fopen(fname, "r");
		
		if(energy_file == NULL){
			printf("Error opening energy file\n");		
		}
		fscanf(energy_file,"%ld",&energy);
		fclose(energy_file);
		total_energy+=energy;
	}

	#ifdef OVERHEAD_POWER
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("OVERHEAD_POWER - get_energy(): %lf microseconds\n", time_heuristic_microseconds);
	#endif 

	return total_energy;
}

/**
* This function returns time as a monotomically increasing long, expressed in nanoseconds.
* Unlike some other techniques for reading time, this is not influenced by changes in CPU frequency.
*
* @Author: Stefano Conoci
*/
long get_time(){
	
	long time =0;
	struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time += (ts.tv_sec*1000000000);
    time += ts.tv_nsec;
	return time;
}

/**
* This function can be used to enable or disable frequency boosting techniques,
* such as TurboBoost for Intel CPUs. Frequency boosting is managed by hardware when
* running at P-state 0. 
*
* @Author: Stefano Conoci
*/
static inline void set_boost(int value){

	int i;
	char fname[64];
	FILE* boost_file;

	#ifdef OVERHEAD_POWER
		long time_heuristic_start;
		long time_heuristic_end;
		double time_heuristic_microseconds;

		time_heuristic_start = get_time();
	#endif 
	
	if(value != 0 && value != 1){
		printf("Set_boost parameter invalid. Shutting down application\n");
		exit(-1);
	}
	
	boost_file = fopen("/sys/devices/system/cpu/cpufreq/boost", "w+");
	fprintf(boost_file, "%d", value);
	fflush(boost_file);
	fclose(boost_file);

	#ifdef OVERHEAD_POWER
		time_heuristic_end = get_time();
		time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
		printf("OVERHEAD_POWER - set_boost() %lf microseconds\n", time_heuristic_microseconds);
	#endif 
	
	return;
}

/**
* This function rsfrees dymically allocated memory. Should be executed at shutdown.
*
* @Author: Stefano Conoci
*/
static void rsfree_memory(){

	rsfree(pstate);
	rsfree(current_pstates);
}


/**
* This function sets the P-state of all controllers threads to the passed parameter. 
* It reads the number of controllers from the global variable current_controllers.
*
* @Author: Stefano Conoci
*/
static int set_controllers_pstate(int input_pstate){

	for(int i=0; i<current_controllers; i++){
		set_core_pstate(i, input_pstate);
	}
}


/**
* This function sets the P-state of all processing threads to the passed parameter. 
* It computes the number of processing threads from the global variables current_controllers
* and nb_phys_cores.
*
* @Author: Stefano Conoci
*/
static int set_processing_pstate(int input_pstate){

	for(int i=current_controllers; i<nb_phys_cores; i++){
		set_core_pstate(i, input_pstate);
	}
}

//////////////////////////////////////////////////////////////////////////////
//	Extern functions 
//////////////////////////////////////////////////////////////////////////////

/**
* This function initializes all data structures and variables for the powercap module.
* Should be executed at startup. 
*
* @Author: Stefano Conoci
*/
int init_powercap_module(){

	if(init_DVFS_management() != 0){
		printf("Cannot init DVFS management\n");
		return -1;
	}

	// Set all cores to the P-state with frequency equal to the controllers_freq parameter
	int frequency_KHz = rootsim_config.controllers_freq*1000;
	int pstate_config = -1;
	for(int i = 0; i<=max_pstate && pstate_config == -1; i++)
		if(pstate[i] == frequency_KHz)
			pstate_config = i; 
	
	if(pstate_config == -1){
		printf("init_powercap_module: The controllers_freq parameter is invalid for this system\n");
		return -1;
	}
	if(set_pstate(pstate_config) != 0){
		printf("init_powercap_module: set_pstate() error\n");
		return -1;
	}

	// Set the powercap state machine to the source state
	exploration_state = 0; 

	// Set current controllers to the starting parameter set by user
	current_controllers = rootsim_config.num_controllers;

	// Start timer and read initial energy 
	start_time = get_time();
	start_energy = get_energy();

	// Start timer and read initial energy for the computation of the powercap error
	error_start_time = get_time();
	error_start_energy = get_energy();

	// Initialize power cap error variables
	error_weighted = 0; 
	error_time_sum = 0;
}

/**
* This function is the entry point into the state machines of the different powercap exploration strategies. 
* It redirects to the state machine of the exploration strategy passed as the "powercap_exploration" parameter.
* Should be called periodically by the controller thread with the lowest local id. 
*
* @Author: Stefano Conoci
*/
int powercap_state_machine(){

	double error_time_interval, error_energy_interval, error_sample_power;
	long end_time, end_energy;

	// Compute powercap error, if time interval has passed
	end_time = get_time();
	if((double) end_time-start_time / 1000000 > POWERCAP_ERROR_INTERVAL){	
		// Compute average power consumption in the sampling interval
		end_energy = get_energy();
		error_time_interval = (double) end_time-start_time;
		error_energy_interval = (double) end_energy-start_energy;
		error_sample_power = error_energy_interval*1000/error_time_interval;

		double error = (error_sample_power - rootsim_config.powercap)/rootsim_config.powercap;
		if (error < 0) 
		 error = 0; 

		error_weighted+= (error_weighted*error_time_sum+error_time_interval*error)/(error_time_sum+error_time_interval);
		error_time_sum+= error_time_interval;

		error_start_time = get_time();
		error_start_energy = get_energy();
	}

	// Redirect execution flow to the specific state machine 
	switch(rootsim_config.powercap_exploration){
		case 0: 
			return static_state_machine();
			break;
		default:
			printf("Invalid powercap_exploration parameter\n");
			return -1;
			break;
	}
}

/**
* This function rsfrees memory and prints power capping results
*
* @Author: Stefano Conoci
*/
void shutdown_powercap_module(){

	rsfree_memory();
	printf("Powercap error percentage: %lf\n", error_weighted);
}


//////////////////////////////////////////////////////////////////////////////
//	State machines 
//////////////////////////////////////////////////////////////////////////////


/**
* This function implements the state machine for powercap_exploration 0. 
* State transition are triggered each interval of time defined by ENERGY_SAMPLE_INTERVAL   
* The exploration policy of this machine is static w.r.t the application performance, 
* it searches for the configuration with "num_controllers" controller threads at frequency 
* "controllers_freq" and processing threads with the highest possible frequency that
* allows to operate within the powercap. In case it is not possible to provide a 
* configuration with power consumption lower than the powercap with the passed parameters, 
* the state machine will converge to the configuration with the highest supported P-state 
* for all the cores of the processing threads   
*
* @Author: Stefano Conoci
*/
static int static_state_machine(){
	
	double time_interval, energy_interval, sample_power, high_powercap, low_powercap;
	long end_time, end_energy;

	end_time = get_time();

	// If sampling interval is not completed return
	if((double) end_time-start_time / 1000000 < ENERGY_SAMPLE_INTERVAL)
		return 1; 

	// Compute average power consumption in the sampling interval
	end_energy = get_energy();
	time_interval = (double) end_time-start_time;
	energy_interval = (double) end_energy-start_energy;
	sample_power = energy_interval*1000/time_interval;

	#ifdef DEBUG_POWER
		printf("Number of Controllers: %d - CT P-state: %d  - PT P-state: %d - Interval %lf ms - Powercap %lf Watt - Sampled Power %lf Watt\n",
			current_controllers, current_pstates[0], current_pstates[current_controllers], 
			time_interval/1000000, rootsim_config.powercap, sample_power);
	#endif

	// Compute hysteresis range for the powercap
	high_powercap = rootsim_config.powercap*(1+HYSTERESIS);

	switch(exploration_state){
		case 0:	// Source state
			
			if(sample_power < high_powercap){
				if(current_pstates[current_controllers] == 0){
					exploration_state = 3;
				} else{
					// Increase frequency and transition to state 1
					set_processing_pstate(current_pstates[current_controllers]-1);
					exploration_state = 1;
				}
			}
			else{ // sample_power >= high_powercap
				if(current_pstates[current_controllers] == max_pstate){
					exploration_state = 3; 
				}
				else{
					// Decrease frequency and transition to state 2
					set_processing_pstate(current_pstates[current_controllers]+1);
					exploration_state = 2; 
				}
			}
			break;
		
		case 1:	// Increase frequency until powercap is exceeded
			
			if(sample_power > high_powercap || current_pstates[current_controllers] == 0){
				// Decrease frequency since it is now too high and transition to state 3
				set_processing_pstate(current_pstates[current_controllers]+1);
				exploration_state = 3; 
			}
			else{	
				// Should stay in state 1 and increase frequency again 
				set_processing_pstate(current_pstates[current_controllers]-1);
			}
			break;
		
		case 2: // Decrease frequency until powercap is met

			if(sample_power < high_powercap || current_pstates[current_controllers] == max_pstate)
				exploration_state = 3; 
			else
				set_processing_pstate(current_pstates[current_controllers]+1);
			break;

		case 3:	// Save the power consumption of the converged configuration
			
			#ifdef DEBUG_POWER
				printf("Converged to p-state %d for PTs\n", current_pstates[current_controllers]);
			#endif
			converged_power = sample_power;
			exploration_state = 4;
			break;

		case 4: // Check if should restart the exploration

			// If power consumption is higher than the cap and it is 3% higher
			// than at the time of convergence, should explore lower frequencies
			if ( sample_power > high_powercap && sample_power > converged_power*(1+RESTART_EXPLORATION_RANGE))
				exploration_state = 2;

			// If power consumption is lower than the cap and it is 3% lower
			// than at the time of convergence, should explore higher frequencies
			if(sample_power < high_powercap && sample_power < converged_power*(1-RESTART_EXPLORATION_RANGE))
				exploration_state = 1;
			break;
	}

	// Reset intervals 
	start_time = get_time();
	start_energy = get_energy();

	return 0;

}