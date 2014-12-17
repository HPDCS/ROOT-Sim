#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <core/core.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <queues/queues.h>
#include <mm/state.h>
#include <core/timer.h>
#include <mm/dymelor.h>
#include <mm/malloc.h>


/// Output File, overwritten at each eecution
FILE	*fout = NULL;


/// Local LPs statistics File, overwritten at each eecution
FILE	*f_local_stat = NULL;

/// Performance statistics files
FILE 		*f_perf_stat;


static char 	local_stat_filename[256],
		kernel_dir[256];


static time_t start_time;


static time_t end_time;


static struct timeval start_timevalue;


// TODO: ma è elegante conservare questo time qiu?!
/// This is a timer that start during the initialization of statistics subsystem and can be used to know the total simulation time
timer simulation_timer;


/// Pointer to the files handled by the statistics subsystem
FILE **files;


/// Last file index of the files handled by the statistics subsystem
static int last_file;


/// Number of allocable file for the statistics subsystem
static int max_file;



static int new_file(char *, int);



static struct stat_type	kernel_stats,
			 system_wide_stats;

/**
* This function initialize the Statistics subsystem
*
* @author Roberto Vitali
*
*/
void statistics_init(void) {

	sprintf(kernel_dir, "%s/kernel%d/", rootsim_config.output_dir, kid);
	sprintf(local_stat_filename, "%s/%s", kernel_dir, LOCAL_STAT_FILE);

	_mkdir(kernel_dir);

	files = (FILE **)rsalloc(PREALLOC_FILE * sizeof(FILE *));
	bzero(files, PREALLOC_FILE * sizeof(FILE *));
	max_file = PREALLOC_FILE;

	fout = files[new_file(OUTPUT_FILE, PER_KERNEL)];
	
	if (rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF)
		f_perf_stat = files[new_file("performance", PER_KERNEL)];
}



/**
* This function finalize the Statistics subsystem
*
* @author Roberto Vitali
*
*/
void statistics_fini(void) {

	int i;
	for (i = 0; i < last_file; i++)
		if (files[i] != NULL)
			fclose(files[i]);
	rsfree(files);
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

	// opath plus 1 is an hack to allow also absolute path
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

	if(access(opath, F_OK))         /* if path is not terminated with / */
		if (mkdir(opath, S_IRWXU))
			if (errno != EEXIST)
				if (errno != EEXIST) {
					rootsim_error(true, "Could not create output directory", opath);
				}
}



/**
* This function create a new file
*
* @author Roberto Vitali
*
* param file_name The name of the file. If the file is local to each kernel, the kernel id is suffixed at the file name
* param type It indicates if the file is kernel-wide (one file per kernel), or system-wide (a unique file)
* 
*/
static int new_file(char *file_name, int type) {

	FILE	*file_temp;
	char	*f_name;
	int	name_length;
	char 	*destination_dir;

	if (last_file >= max_file - 1) {
		max_file *= 2;
		files = realloc(files, max_file);
		if (files == NULL)
			rootsim_error(true, "Error in allocating new space for files");
	}
	
	switch (type) {
		case PER_KERNEL:
			destination_dir = kernel_dir;
			break;

		case UNIQUE:
			destination_dir = rootsim_config.output_dir;
			break;
	}

	name_length = strlen(destination_dir) + strlen(file_name) + 2;
	f_name = (char *)rsalloc(name_length);
	bzero(f_name, name_length);
	strcpy(f_name, destination_dir);
	f_name = strcat(f_name, "/");
	f_name = strcat(f_name, file_name);
	
	if ( (type == UNIQUE && master_kernel()) || type != UNIQUE) {
		if ( (file_temp = fopen(f_name, "w")) == NULL)  {
			rsfree(f_name);
			rootsim_error(true, "Cannot open the file %s\n", f_name);
		}
		files[last_file++] = file_temp;
	}

	rsfree(f_name);
	return last_file - 1;
}



/**
* This function manage the simulation time used to in the statistics, and print to the output file the starting simulation header
*
* @author Francesco Quaglia 
* @author Roberto Vitali
* 
*/
void start_statistics(void){

	// Print the statistics output header
	fprintf(fout, "(%d) local setup , my pid  %d\n", kid, getpid());		    
	fprintf(fout, "TOTAL SIMULATION KERNELS: %d \n", n_ker);
	fprintf(fout, "LOCAL KERNEL ID: %d\n", kid);
	fprintf(fout, "TOTAL LOGICAL PROCESSES %d\n", n_prc_tot);
	fprintf(fout, "LOCAL LOGICAL PROCESSES %d\n\n", n_prc);
	fprintf(fout, "CHECKPOINTING: type %d - period %d\n\n", rootsim_config.checkpointing, rootsim_config.ckpt_period);
	fflush(fout);

	if (rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF) {
		fprintf(f_perf_stat, "#\"NUM GVT\"\t\"GVT VALUE\"\t\"COMM EVENTS\"\t\"CUMULATED COMM EVENTS\"\t\n");
		fflush(f_perf_stat);
	}

	fprintf( fout, "\n-------------  STARTING SIMULATION ---------------\n");

	start_time = time(NULL);
	gettimeofday(&start_timevalue, NULL);	
	timer_start(simulation_timer);
}



/**
* This function manage the end  of the simulation time used to in the statistics, and print to the output file the ending of the simulation
*
* @author Francesco Quaglia 
* @author Roberto Vitali
*
*/
int stop_statistics(void){
	int total_time = timer_value(simulation_timer);
	if (master_kernel()) {
		fprintf(fout, "TOTAL TIME: ");
		timer_print(total_time);
		fprintf(fout, " milliseconds \n");
		
	}
	
	fprintf(fout, "\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
	fflush(fout);
	return total_time;
}



/**
* This function print in the output file all the statistics associated with the simulation
*
* @author Francesco Quaglia 
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
*/
void flush_statistics(void){

	FILE *file_stat;

	register unsigned int t;

	double 	tot_time;
	struct 	tm *ptr;
	float	event_rate = 0;
	int	t_gid;

	end_time = time(NULL);	


	// FIXME: this is no longer working, why?
	// Statistics Local to a kernel
	if (true || rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_LOCAL) {


		f_local_stat = fopen(local_stat_filename, "w");
		fprintf(f_local_stat, "#\"GID\"\t\"LID\"\t\"TOTAL EVENTS\"\t\"COMMITTED EVENTS\"\t\"ROLLBACKS\"\t\"ANTIMESSAGES\"");
		fprintf(f_local_stat, "\t\"AVERAGE EVENT COST (us)\"\t\"AVERAGE CKPT COST (us)\"\n");

		for (t = 0; t < n_prc; t++) {
			t_gid = LidToGid(t);
			fprintf( f_local_stat, "%d", t_gid);		
			fprintf( f_local_stat, "\t%d", t);		
			fprintf( f_local_stat, "\t%lu", LPS[t]->total_events );
//			fprintf( f_local_stat, "\t\t%lu", LPS[t]->last_snapshot.committed_events - LPS[t]->first_snapshot.committed_events);
			fprintf( f_local_stat, "\t\t\t%ld", LPS[t]->count_rollbacks);
			fprintf( f_local_stat, "\t\t%lu", LPS[t]->total_antimessages);		

			// expressed in microseconds
			fprintf( f_local_stat, "\t\t%.2f", LPS[t]->event_total_time  / LPS[t]->total_events );		
//			fprintf( f_local_stat, "\t\t\t%.2f", LPS[t]->ckpt_total_time / LPS[t]->saved_states_counter );		
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



	fprintf(fout, "\nTOTAL EVENTS EXECUTED...... : %.0f \n", 		kernel_stats.tot_events);
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

	// Global Statistics
/*	MPI_Reduce(&kernel_stats.tot_events, &system_wide_stats.tot_events, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.committed_events, &system_wide_stats.committed_events, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.committed_eventsRP, &system_wide_stats.committed_eventsRP, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.tot_rollbacks, &system_wide_stats.tot_rollbacks, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.tot_antimessages, &system_wide_stats.tot_antimessages, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.tot_ckpts, &system_wide_stats.tot_ckpts,1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.tot_checkpoints, &system_wide_stats.tot_checkpoints, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.tot_recoveries, &system_wide_stats.tot_recoveries, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.ckpt_cost, &system_wide_stats.ckpt_cost, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.recovery_cost, &system_wide_stats.recovery_cost, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.Fr, &system_wide_stats.Fr, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.Lr, &system_wide_stats.Lr, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.Ef, &system_wide_stats.Ef, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
	
	MPI_Reduce(&kernel_stats.event_time, &system_wide_stats.event_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kernel_stats.ckpt_time, &system_wide_stats.ckpt_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
*/

	// Calculate an average on the number of kernel
	if (master_kernel()) {

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
}

