#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <arch/thread.h>
#include <core/core.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <queues/queues.h>
#include <mm/state.h>
#include <core/timer.h>
#include <mm/dymelor.h>
#include <mm/malloc.h>


/// This is a timer that start during the initialization of statistics subsystem and can be used to know the total simulation time
static timer simulation_timer;

/// Pointers to unique files
static FILE **unique_files;
/// Pointers to per-thread files
static FILE ***thread_files;

/// Keeps statistics on a per-LP base
static struct stat_t *lp_stats;
/// Keeps statistics on a per-LP base
static struct stat_t *lp_stats_gvt;
static struct stat_t *thread_stats;
static struct stat_t system_wide_stats;

static __thread char thread_dir[512];


/**
* This function creates a new file
*
* @author Roberto Vitali
*
* @param file_name The name of the file. If the file is local to each kernel, the kernel id is suffixed at the file name
* @param type It indicates if the file is kernel-wide (one file per kernel), or system-wide (a unique file)
* @param index The index where to store the file in the corresponding array
* 
*/
static void new_file(char *file_name, int type, int index) {
	register unsigned int i;
	FILE	*f;
	char 	*destination_dir;
	char	f_name[1024];
	
	// Sanity check
	if(index >= NUM_FILES) {
		rootsim_error(true, "Asking for too many statistics files. Check the value of NUM_FILES. Aborting...\n");
	}

	switch (type) {
		case STAT_PER_THREAD:
			destination_dir = thread_dir;
			break;

		case STAT_UNIQUE:
			destination_dir = rootsim_config.output_dir;
			break;
			
		default:
			rootsim_error(true, "Wrong type of statistics file specified at %s:%d\n", __FILE__, __LINE__);
			return;
	}

	snprintf(f_name, 1024, "%s/%s", destination_dir, file_name);

	// Create the actual file
	if ( (type == STAT_UNIQUE && master_kernel() && master_thread()) ) {
		if ( (f = fopen(f_name, "w")) == NULL)  {
			rootsim_error(true, "Cannot open %s\n", f_name);
		}
		unique_files[index] = f;
		
	} else if (type == STAT_PER_THREAD) {
		for(i = 0; i < n_cores; i++) {
			if ( (f = fopen(f_name, "w")) == NULL)  {
				rootsim_error(true, "Cannot open %s\n", f_name);
			}
			thread_files[i][index] = f;
		}
	}
}




/**
* This is an helper-function to allow the statistics subsystem create a new directory
*
* @author Alessandro Pellegrini
*
* @param path The path of the new directory to create
*/
void _mkdir(const char *path) {

	char opath[512];
	char *p;
	size_t len;

	strncpy(opath, path, sizeof(opath));
	len = strlen(opath);
	if(opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// opath plus 1 is a hack to allow also absolute path
	for(p = opath + 1; *p; p++) {
		if(*p == '/') {
			*p = '\0';
			if(access(opath, F_OK))
				if (mkdir(opath, S_IRWXU))
					if (errno != EEXIST) {
						rootsim_error(true, "Could not create output directory", opath);
					}
			*p = '/';
		}
	}

	// Path does not terminate with a slash
	if(access(opath, F_OK)) {
		if (mkdir(opath, S_IRWXU)) {
			if (errno != EEXIST) {
				if (errno != EEXIST) {
					rootsim_error(true, "Could not create output directory", opath);
				}
			}
		}
	}
}

/**
* This function manage the simulation time used to in the statistics, and print to the output file the starting simulation header
*
* @author Francesco Quaglia 
* @author Roberto Vitali
* 
*/
static void statistics_start(void) {
	register unsigned int i;
	FILE *f = get_file(STAT_UNIQUE, GLOBAL_STAT);

	// Print the statistics output header
	fprintf(f, "TOTAL SIMULATION KERNELS: %d \n", n_ker);
	fprintf(f, "KERNEL ID: %d\n", kid);
	fprintf(f, "TOTAL LOGICAL PROCESSES %d\n", n_prc_tot);
	fprintf(f, "LOCAL LOGICAL PROCESSES %d\n\n", n_prc);
	fprintf(f, "CHECKPOINTING: type %d - period %d\n\n", rootsim_config.checkpointing, rootsim_config.ckpt_period);
	fprintf(f, "\n-------------  STARTING SIMULATION ---------------\n");
	fflush(f);

	if (rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF) {
		for(i = 0; i < n_cores; i++) {
			fprintf(thread_files[i][GVT_STAT], "#\"EXEC TIME\"\t\"GVT VALUE\"\t\"COMM EVENTS\"\t\"CUMULATED COMM EVENTS\"\t\n");
			fflush(thread_files[i][GVT_STAT]);
		}
	}

	timer_start(simulation_timer);
}




/**
* This function print in the output file all the statistics associated with the simulation
*
* @author Francesco Quaglia 
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
*/
static void statistics_flush(void){
/*
	FILE *file_stat;

	register unsigned int t;

	double 	tot_time;
	struct 	tm *ptr;
	float	event_rate = 0;
	int	t_lid;

	// FIXME: this is no longer working, why?
	// Statistics Local to a kernel
	if (rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_LOCAL) {


		f_local_stat = fopen(local_stat_filename, "w");
		fprintf(f_local_stat, "#\"lid\"\t\"LID\"\t\"TOTAL EVENTS\"\t\"COMMITTED EVENTS\"\t\"ROLLBACKS\"\t\"ANTIMESSAGES\"");
		fprintf(f_local_stat, "\t\"AVERAGE EVENT COST (us)\"\t\"AVERAGE CKPT COST (us)\"\n");

		for (t = 0; t < n_prc; t++) {
			t_lid = LidTolid(t);
			fprintf( f_local_stat, "%d", t_lid);		
			fprintf( f_local_stat, "\t%d", t);		
			fprintf( f_local_stat, "\t%lu", LPS[t]->total_events );
			fprintf( f_local_stat, "\t\t\t%ld", LPS[t]->count_rollbacks);
			fprintf( f_local_stat, "\t\t%lu", LPS[t]->total_antimessages);		

			// expressed in microseconds
			fprintf( f_local_stat, "\t\t%.2f", LPS[t]->event_total_time  / LPS[t]->total_events );		
			fprintf( f_local_stat, "\n");

		}
		fflush(f_local_stat);
		fclose(f_local_stat);
	}

	kernel_stats.kid = kid;	
	
	for (t = 0; t < n_prc; t++) {
		// Local kernel statistics
		kernel_stats.tot_events		+= LPS[t]->total_events;
//		kernel_stats.committed_events 	+= LPS[t]->last_snapshot.committed_events;
//		kernel_stats.committed_eventsRP	+= (LPS[t]->last_snapshot.committed_events - LPS[t]->first_snapshot.committed_events);

		kernel_stats.tot_rollbacks	+= LPS[t]->count_rollbacks;
		kernel_stats.tot_antimessages	+= LPS[t]->total_antimessages;
		kernel_stats.tot_ckpts		+= LPS[t]->saved_states_counter;

		kernel_stats.event_time 	+= LPS[t]->event_total_time;
//		kernel_stats.ckpt_time		+= LPS[t]->ckpt_total_time;
		
	}
	// End Statistics Local to a kernel


	// FIXME: this must be reintegrated in the trunk version
//	kernel_stats.ckpt_cost = checkpoint_cost_per_byte;
//	kernel_stats.recovery_cost = recovery_cost_per_byte;
//	kernel_stats.tot_checkpoints = total_checkpoints;
//	kernel_stats.tot_recoveries = total_recoveries;
	// In kernel_stats only data for the regime permanente


	kernel_stats.Fr = (kernel_stats.tot_rollbacks / kernel_stats.tot_events);
	kernel_stats.Lr = (kernel_stats.tot_events - kernel_stats.committed_events) / kernel_stats.tot_rollbacks ;	
	kernel_stats.Ef = (1 - kernel_stats.Fr * kernel_stats.Lr) * 100;



	fprintf(fout, "\nTOTAL EXECUTED EVENTS ..... : %.0f \n", 		kernel_stats.tot_events);
	fprintf(fout,   "TOTAL COMMITTED EVENTS (RP) : %.0f \n", 		kernel_stats.committed_eventsRP);		
	fprintf(fout,   "TOTAL COMMITTED EVENTS..... : %.0f \n", 		kernel_stats.committed_events);		
	fprintf(fout,   "TOTAL ROLLBACKS EXECUTED... : %.0f \n", 		kernel_stats.tot_rollbacks);		
	fprintf(fout,   "TOTAL ANTIMESSAGES......... : %.0f \n", 		kernel_stats.tot_antimessages);		
	fprintf(fout,   "ROLLBACK FREQUENCY......... : %.2f %%\n",	 	kernel_stats.Fr * 100);		
	fprintf(fout,   "ROLLBACK LENGTH............ : %.2f events\n",		kernel_stats.Lr);
	fprintf(fout,   "EFFICIENCY................. : %.2f %%\n",		kernel_stats.Ef);
	fprintf(fout,   "AVERAGE EVENT COST......... : %.2f us\n",		kernel_stats.event_time / kernel_stats.tot_events);
	fprintf(fout,   "AVERAGE CKPT COST.......... : %.2f us\n",		kernel_stats.ckpt_time / kernel_stats.tot_ckpts);
	fprintf(fout,   "AVERAGE CHECKPOINT COST.... : %.3f us\n",		kernel_stats.ckpt_cost / kernel_stats.tot_checkpoints);
	fprintf(fout,   "AVERAGE RECOVERY COST...... : %.3f us\n",		kernel_stats.recovery_cost / kernel_stats.tot_recoveries);
//	fprintf(fout,   "AVERAGE LOG SIZE........... : %.2f bytes\n",		checkpoint_bytes_total / kernel_stats.tot_ckpts);
	fflush(fout);

	// Calculate an average on the number of threads
	if (master_thread()) {

		file_stat = files[new_file(STAT_FILE, UNIQUE)];
		system_wide_stats.Fr /= n_ker;
		system_wide_stats.Lr /= n_ker;
		system_wide_stats.Ef /= n_ker;

		fprintf(file_stat, "------------------------------------------------------------\n");
		fprintf(file_stat, "-------------------- GLOBAL STATISTICS ---------------------\n");
		fprintf(file_stat, "------------------------------------------------------------\n");

		fprintf(file_stat, "TOTAL KERNELS: %d\nTOTAL PROCESSES: %d\n\n", 	n_ker, n_prc_tot);
		fprintf(file_stat, "TOTAL EVENTS EXECUTED......: %.0f\n", 		system_wide_stats.tot_events);
		fprintf(file_stat, "TOTAL COMMITTED EVENTS (RP): %.0f\n", 		system_wide_stats.committed_eventsRP);
		fprintf(file_stat, "TOTAL COMMITTED EVENTS.....: %.0f\n", 		system_wide_stats.committed_events);
		fprintf(file_stat, "TOTAL ROLLBACKS EXECUTED...: %.0f\n", 		system_wide_stats.tot_rollbacks);
		fprintf(file_stat, "TOTAL ANTIMESSAGES.........: %.0f\n\n", 	system_wide_stats.tot_antimessages);
		fprintf(file_stat, "AVERAGE ROLLBACK FREQUENCY.: %.2f %%\n", 	system_wide_stats.Fr * 100);
		fprintf(file_stat, "AVERAGE ROLLBACK LENGTH....: %.2f events\n", 	system_wide_stats.Lr);
		fprintf(file_stat, "EFFICIENCY.................: %.2f %%\n\n",	system_wide_stats.Ef);	
		fprintf(file_stat, "AVERAGE EVENT COST.........: %.2f us\n",	system_wide_stats.event_time / system_wide_stats.tot_events);
		fprintf(file_stat, "AVERAGE CHECKPOINT COST....: %.2f us\n\n",	system_wide_stats.ckpt_time / system_wide_stats.tot_ckpts);
		fprintf(file_stat, "AVERAGE CHECKPOINT COST....: %.3f us\n",	system_wide_stats.ckpt_cost / system_wide_stats.tot_checkpoints);
		fprintf(file_stat, "AVERAGE RECOVERY COST......: %.3f us\n\n",	system_wide_stats.recovery_cost / system_wide_stats.tot_recoveries);
				
		fprintf(file_stat, "Simulation started at: ");

		ptr = localtime(&start_time);
		fprintf(file_stat, asctime(ptr));

		fprintf(file_stat, "Simulation finished at: ");
		ptr = localtime(&end_time);
		fprintf(file_stat, asctime(ptr));
		fprintf(file_stat,"\n");

		tot_time = difftime( end_time, start_time);
		fprintf(file_stat, "Computation time....................: %.0f seconds\n", (tot_time));
		fprintf(file_stat, "Computation time (steady state only): %.0f seconds\n", ((tot_time - STARTUP_TIME / 1000 )));
//		fprintf(file_stat, "\nLast GVT value: %f\n\n", prev_gvt);
		
		event_rate = system_wide_stats.committed_eventsRP/(tot_time - (STARTUP_TIME / 1000));
		fprintf(file_stat, "Average Committed Steady state Event-Rate: %.2f events/second\n\n", event_rate);

		event_rate = system_wide_stats.committed_events / tot_time;
		fprintf(file_stat, "Average Committed Event-Rate.............: %.2f events/second\n", event_rate);
		fflush(file_stat);
			
		fclose(file_stat);
	} 
*/
}


static void statistics_flush_gvt(double gvt) {
	
	FILE *f = get_file(STAT_PER_THREAD, GVT_STAT);
	register unsigned int i;
	unsigned int committed = 0;
	static __thread unsigned int cumulated;
	double exec_time;
	
	// Dump on file only if required
	if( !rootsim_config.stats == STATS_ALL && !rootsim_config.stats == STATS_PERF) {
		return;
	}
	
	exec_time = timer_value_seconds(simulation_timer);
	
	// Reduce the committed events from all LPs
	for(i = 0; i < n_prc_per_thread; i++) {
		committed += lp_stats_gvt[LPS_bound[i]->lid].committed_events;
	}
	cumulated += committed;
	
	fprintf(f, "%f\t%f\t%d\t%d\n", exec_time, gvt, committed, cumulated);
	fflush(f);
}


/**
* This function initialize the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_init(void) {
	register unsigned int i;
	
	if(rootsim_config.serial) {
		printf("Sequential simulation is not yet integrated with the new statistics system.\n"
		       "Basic information will be displayed on screen at simulation completion\n");
		return;
	}
	

	// Master thread directories to keep
	// statistics from all threads
	if(master_thread()) {
		for(i = 0; i < n_cores; i++) {
			sprintf(thread_dir, "%s/thread_%d_%d/", rootsim_config.output_dir, kid, i);
			_mkdir(thread_dir);
		}
	}
	sprintf(thread_dir, "%s/thread_%d_%d/", rootsim_config.output_dir, kid, tid);
	
	// Allocate entries for file arrays (and set them to NULL, for later closing)
	thread_files = rsalloc(sizeof(FILE **) * n_cores);
	bzero(thread_files, sizeof(FILE **) * n_cores);
	
	for(i = 0; i < n_cores; i++) {
		thread_files[i] = rsalloc(sizeof(FILE *) * NUM_FILES);
		bzero(thread_files[i], sizeof(FILE *) * NUM_FILES);
	}
	unique_files = rsalloc(sizeof(FILE *) * NUM_FILES);
	bzero(unique_files, sizeof(FILE *) * NUM_FILES);
	
	// Create files depending on the actual level of verbosity
	new_file(GLOBAL_STAT_NAME, STAT_UNIQUE, GLOBAL_STAT);
	
	new_file(THREAD_STAT_NAME, STAT_PER_THREAD, THREAD_STAT);
	
	if(rootsim_config.stats == STATS_PERF || rootsim_config.stats == STATS_ALL)
		new_file(GVT_STAT_NAME, STAT_PER_THREAD, GVT_STAT);
		
	if(rootsim_config.stats == STATS_ALL)
		new_file(LP_STATS_NAME, STAT_PER_THREAD, LP_STATS);
		
	// Initialize data structures to keep information
	lp_stats = rsalloc(n_prc_tot * sizeof(struct stat_t));
	bzero(lp_stats, n_prc_tot * sizeof(struct stat_t));
	lp_stats_gvt = rsalloc(n_prc_tot * sizeof(struct stat_t));
	bzero(lp_stats_gvt, n_prc_tot * sizeof(struct stat_t));
	thread_stats = rsalloc(n_cores * sizeof(struct stat_t));
	bzero(thread_stats, n_cores * sizeof(struct stat_t));
}


/**
* This function finalize the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_fini(void) {
	register unsigned int i, j;
	
	for(i = 0; i < NUM_FILES; i++) {
		if(unique_files[i] != NULL)
			fclose(unique_files[i]);
	}
	rsfree(unique_files);
	
	for(i = 0; i < n_cores; i++) {
		for(j = 0; j < NUM_FILES; j++) {
			if(thread_files[i][j] != NULL)
				fclose(thread_files[i][j]);
		}
		rsfree(thread_files[i]);
	}
	rsfree(thread_files);
	
	rsfree(thread_stats);
	rsfree(lp_stats);
}


/**
* When invoked, this function computes the total simulation time, and flushes 
* all statistics files, as per simulation configuration
* 
* @param exit_code The exit code of the simulator, used to display a happy
* 		   or sad termination message
*
* @author Francesco Quaglia 
* @author Roberto Vitali
* @author Alessandro Pellegrini
*/
void statistics_stop(int exit_code) {
	
	FILE *f = get_file(STAT_UNIQUE, GLOBAL_STAT);

	if(rootsim_config.serial) {
		printf("Sequential simulation is not yet integrated with the new statistics system.\n"
		       "Basic information will be displayed on screen at simulation completion\n");
		return;
	}

	statistics_flush();

	if (master_kernel() && master_thread()) {
		fprintf(f, "TOTAL TIME: %.03f seconds", timer_value_seconds(simulation_timer));

		if(exit_code == EXIT_FAILURE) {
			fprintf(f, "\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
			printf("\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
		}
		if(exit_code == EXIT_SUCCESS) {
			fprintf(f, "\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
			printf("\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
		}	
	}
	
	fflush(f);
}

inline void statistics_post_lp_data(unsigned int lid, unsigned int type, double data) {
	
	switch(type) {
		
		case STAT_ANTIMESSAGE:
			lp_stats_gvt[lid].tot_antimessages++;
			break;
			
		case STAT_EVENT:
			lp_stats_gvt[lid].tot_events++;
			break;
			
		case STAT_EVENT_TIME:
			lp_stats_gvt[lid].event_time += data;
			break;
			
		case STAT_COMMITTED:
			lp_stats_gvt[lid].committed_events += data;
			break;
			
		case STAT_ROLLBACK:
			lp_stats_gvt[lid].tot_rollbacks++;
			break;
			
		case STAT_CKPT:
			lp_stats_gvt[lid].tot_ckpts++;
			break;
		
		case STAT_CKPT_MEM:
			lp_stats_gvt[lid].ckpt_mem += data;
			break;
		
		case STAT_CKPT_TIME:
			lp_stats_gvt[lid].ckpt_time += data;
			break;
			
		case STAT_RECOVERY:
			lp_stats_gvt[lid].tot_recoveries++;
			break;
			
		case STAT_RECOVERY_TIME:
			lp_stats_gvt[lid].recovery_time += data;
			break;
			
		case STAT_IDLE_CYCLES:
			lp_stats_gvt[lid].idle_cycles++;
			break;
		
		default:
			rootsim_error(true, "Wrong LP statistics post type: %d. Aborting...\n", type);
	}
}


inline void statistics_post_other_data(unsigned int type, double data) {
	register unsigned int i;
	
	switch(type) {
		
		case STAT_SIM_START:
			statistics_start();
			break;
		
		// Sum up all that happened in the last GVT phase, in case it is required,
		// dump a line on the corresponding statistics file
		case STAT_GVT:
			for(i = 0; i < n_prc_per_thread; i++) {
				unsigned int lid = LPS_bound[i]->lid;
				
				lp_stats[lid].tot_antimessages += lp_stats_gvt[lid].tot_antimessages;
				lp_stats[lid].tot_events += lp_stats_gvt[lid].tot_events;
				lp_stats[lid].event_time += lp_stats_gvt[lid].event_time;
				lp_stats[lid].committed_events += lp_stats_gvt[lid].committed_events;
				lp_stats[lid].tot_ckpts += lp_stats_gvt[lid].tot_ckpts;
				lp_stats[lid].ckpt_mem += lp_stats_gvt[lid].ckpt_mem;
				lp_stats[lid].ckpt_time += lp_stats_gvt[lid].ckpt_time;
				lp_stats[lid].tot_recoveries += lp_stats_gvt[lid].tot_recoveries;
				lp_stats[lid].recovery_time += lp_stats_gvt[lid].recovery_time;
				lp_stats[lid].idle_cycles += lp_stats_gvt[lid].idle_cycles;
				
				statistics_flush_gvt(data);
				
				bzero(&lp_stats_gvt[LPS_bound[i]->lid], sizeof(struct stat_t));
			}
			break;
		
		default:
			rootsim_error(true, "Wrong statistics post type: %d. Aborting...\n", type);
	}
}

