#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <arch/thread.h>
#include <arch/memusage.h>
#include <core/core.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <queues/queues.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <core/timer.h>


#define GVT_BUFF_SIZE	2048

struct _gvt_buffer {
	unsigned int pos;
	char buff[GVT_BUFF_SIZE];
};


/// This structure is used to buffer statistics gathered on GVT computation
__thread struct _gvt_buffer gvt_buffer;

/// This is a timer that start during the initialization of statistics subsystem and can be used to know the total simulation time
timer simulation_timer;

/// Pointers to unique files
static FILE **unique_files;
/// Pointers to per-thread files
static FILE ***thread_files;

/// Keeps statistics on a per-LP basis
static struct stat_t *lp_stats;

/// Keeps statistics on a per-LP basis in a GVT phase
static struct stat_t *lp_stats_gvt;

/// Keeps statistics on a per-thread basis
static struct stat_t *thread_stats;

/// Keeps global statistics
static struct stat_t system_wide_stats;


/**
* This function creates a new file
*
* @author Roberto Vitali
*
* @param file_name The name of the file. If the file is local to each kernel, the kernel id is suffixed at the file name
* @param type It indicates if the file is kernel-wide (one file per kernel), or system-wide (a unique file)
* @param idx The index where to store the file in the corresponding array
*
*/
static void new_file(char *file_name, int type, int idx) {
	register unsigned int i;
	FILE	*f;
	char	f_name[MAX_PATHLEN];

	// Sanity checks
	if(idx >= NUM_FILES) {
		rootsim_error(true, "Asking for too many statistics files. Check the value of NUM_FILES. Aborting...\n");
	}

	if(type != STAT_PER_THREAD && type != STAT_UNIQUE)
		rootsim_error(true, "Wrong type of statistics file specified at %s:%d\n", __FILE__, __LINE__);

	// Create the actual file(s)
	if ( (type == STAT_UNIQUE && master_kernel() && master_thread()) ) {
		snprintf(f_name, MAX_PATHLEN, "%s/%s", rootsim_config.output_dir, file_name);

		if ( (f = fopen(f_name, "w")) == NULL)  {
			rootsim_error(true, "Cannot open %s\n", f_name);
		}
		unique_files[idx] = f;

	} else if (type == STAT_PER_THREAD) {
		for(i = 0; i < n_cores; i++) {
			snprintf(f_name, MAX_PATHLEN, "%s/thread_%d_%d/%s", rootsim_config.output_dir, kid, i, file_name);
			if ( (f = fopen(f_name, "w")) == NULL)  {
				rootsim_error(true, "Cannot open %s\n", f_name);
			}
			thread_files[i][idx] = f;
		}
	}
}


static inline FILE *get_file(unsigned int type, unsigned idx) {
			FILE *f;

			if(type == STAT_PER_THREAD)
				f = thread_files[tid][idx];
			else if(type == STAT_UNIQUE)
				f = unique_files[idx];
			return f;
}


/**
* This is an helper-function to recursively delete the whole content of a folder
*
* @param path The path of the directory to purge
*/
static void _rmdir(const char *path) {
	char buf[MAX_PATHLEN];
        struct stat st;
        struct dirent *dirt;
        DIR *dir;

	// We could be asked to remove a non-existing folder. In that case, do nothing!
        if (!(dir = opendir(path))) {
                return;
        }

        while ( (dirt = readdir(dir))) {
                if ((strcmp(dirt->d_name, ".") == 0) || (strcmp(dirt->d_name, "..") == 0))
                        continue;
                snprintf(buf, MAX_PATHLEN, "%s/%s", path, dirt->d_name);

                if (stat(buf, &st) == -1) {
                        rootsim_error(false, "stat() file \"%s\" failed, %s\n", buf, strerror(errno));
                        continue;
                }

                if (S_ISLNK(st.st_mode) || S_ISREG(st.st_mode)) {
                        if (unlink(buf) == -1)
                                rootsim_error(false, "unlink() file \"%s\" failed, %s\n", buf, strerror(errno));
                }
                else if (S_ISDIR(st.st_mode)) {
                        _rmdir(buf);
                }
        }
        closedir(dir);

        if (rmdir(path) == -1)
                rootsim_error(false, "rmdir() directory \"%s\" failed, %s\n", buf, strerror(errno));
}




/**
* This is an helper-function to allow the statistics subsystem create a new directory
*
* @author Alessandro Pellegrini
*
* @param path The path of the new directory to create
*/
void _mkdir(const char *path) {

	char opath[MAX_PATHLEN];
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

	// Print the header of GVT statistics files
	if (!rootsim_config.serial && (rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF)) {
		for(i = 0; i < n_cores; i++) {
			fprintf(thread_files[i][GVT_STAT], "#\"WCT\"\t\"GVT VALUE\"\t\"COMM EVENTS\"\t\"CUMULATED COMM EVENTS\"\t\n");
			fflush(thread_files[i][GVT_STAT]);
		}
	}

	timer_start(simulation_timer);
}


/**
 * Nicely print a size
 *
 * @param size The size in byte
 */
static char *format_size(double size) {
	static __thread char size_str[32];
	char *fmt;
	int divisor;

	if(size <= 1024) {
		fmt = "%.02f B";
		divisor = 1;
	} else if (size <= 1024 * 1024) {
		fmt = "%.02f KB";
		divisor = 1024;
	} else if (size <= 1024 * 1024 * 1024) {
		fmt = "%.02f MB";
		divisor = 1024 * 1024;
	} else {
		fmt = "%.02f GB";
		divisor = 1024 * 1024 * 1024;
	}

	sprintf(size_str, fmt, size / divisor);

	return size_str;
}


static inline statistics_flush_gvt_buffer(void) {
	FILE *f;

	f = get_file(STAT_PER_THREAD, GVT_STAT);
	fprintf(f, gvt_buffer.buff);
	fflush(f);
	gvt_buffer.pos = 0;
	gvt_buffer.buff[0] = '\0';
}


/**
* This function print in the output file all the statistics associated with the simulation
*
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
* @param exit_code The exit code of the simulator, used to display a happy
* 		   or sad termination message
*/
void statistics_stop(int exit_code) {
	register unsigned int i;
	FILE *f;
	double total_time;
	double rollback_frequency,
	       rollback_length,
	       efficiency;
	timer simulation_finished;
	char timer_string[64]; // 64 chars is the size required by the macro to convert timers to string
	char f_name[MAX_PATHLEN]; // Used only for sequential statistics


	if(rootsim_config.serial) {

		// Stop timers
		timer_start(simulation_finished);
		total_time = timer_value_seconds(simulation_timer);

		sprintf(f_name, "%s/sequential_stats", rootsim_config.output_dir);
		if ( (f = fopen(f_name, "w")) == NULL)  {
			rootsim_error(true, "Cannot open %s\n", f_name);
		}

		fprintf(f, "------------------------------------------------------------\n");
		fprintf(f, "-------------------- SERIAL STATISTICS ---------------------\n");
		fprintf(f, "------------------------------------------------------------\n\n");

		timer_tostring(simulation_timer, timer_string);
		fprintf(f, "SIMULATION STARTED AT ..... : %s \n", 		timer_string);
		timer_tostring(simulation_finished, timer_string);
		fprintf(f, "SIMULATION FINISHED AT .... : %s \n", 		timer_string);
		fprintf(f, "TOTAL SIMULATION TIME ..... : %.03f seconds \n",	total_time);

		fprintf(f, "\n");
		fprintf(f, "TOTAL LPs.................. : %d \n", 		n_prc_tot);
		fprintf(f, "TOTAL EXECUTED EVENTS ..... : %.0f \n", 		system_wide_stats.tot_events);
		fprintf(f, "AVERAGE EVENT COST......... : %.3f us\n",		total_time / system_wide_stats.tot_events * 1000 * 1000);
		fprintf(f, "AVERAGE EVENT COST (EMA)... : %.2f us\n",		system_wide_stats.exponential_event_time);
		fprintf(f, "\n");
		fprintf(f, "LAST COMMITTED GVT ........ : %f\n",		system_wide_stats.gvt_time);
		fprintf(f, "SIMULATION TIME SPEED...... : %.2f units per GVT\n",system_wide_stats.simtime_advancement);
		fprintf(f, "AVERAGE MEMORY USAGE....... : %s\n",		format_size(system_wide_stats.memory_usage / system_wide_stats.gvt_computations));
		fprintf(f, "PEAK MEMORY USAGE.......... : %s\n",		format_size(getPeakRSS()));

		if(exit_code == EXIT_FAILURE) {
			fprintf(f, "\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
			printf("\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
		}
		if(exit_code == EXIT_SUCCESS) {
			fprintf(f, "\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
			printf("\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
		}

		fflush(f);

	} else { /* Parallel simulation */

		// Stop the simulation timer immediately to avoid considering the statistics reduction time
		if (master_kernel() && master_thread()) {
			timer_start(simulation_finished);
			total_time = timer_value_seconds(simulation_timer);
		}
		
		/* Finish flushing GVT statistics */
		statistics_flush_gvt_buffer();

		/* dump per-LP statistics if required. They are already reduced during the GVT phase */

		if(rootsim_config.stats == STATS_LP || rootsim_config.stats == STATS_ALL) {
			f = get_file(STAT_PER_THREAD, LP_STATS);

			fprintf(f, "#\"GID\"\t"
				   "\"LID\"\t"
				   "\"TOTAL EVENTS\"\t"
				   "\"COMMITTED EVENTS\"\t"
				   "\"REPROCESSED EVENTS\"\t"
				   "\"ROLLBACKS\"\t"
				   "\"ANTIMESSAGES\"\t"
				   "\"AVERAGE EVENT COST\"\t"
				   "\"AVERAGE CKPT COST\"\t"
				   "\"AVERAGE RECOVERY COST\"\t"
				   "\"IDLE CYCLES\"\t"
				   "\n");

			for(i = 0; i < n_prc_per_thread; i++) {
				unsigned int lid = LPS_bound[i]->lid;
				fprintf(f, "%d\t", LidToGid(lid));
				fprintf(f, "%d\t", lid);
				fprintf(f, "%d\t", (int)lp_stats[lid].tot_events);
				fprintf(f, "%d\t", (int)lp_stats[lid].committed_events);
				fprintf(f, "%d\t", (int)lp_stats[lid].reprocessed_events);
				fprintf(f, "%d\t", (int)lp_stats[lid].tot_rollbacks);
				fprintf(f, "%d\t", (int)lp_stats[lid].tot_antimessages);
				fprintf(f, "%f\t", lp_stats[lid].event_time / lp_stats[lid].tot_events);
				fprintf(f, "%f\t", lp_stats[lid].ckpt_time / lp_stats[lid].tot_ckpts);
				fprintf(f, "%f\t", ((int)lp_stats[lid].tot_rollbacks > 0 ? lp_stats[lid].recovery_time / lp_stats[lid].tot_recoveries : 0));
				fprintf(f, "%d\t", (int)lp_stats[lid].idle_cycles);
				fprintf(f, "\n");
			}
		}


		/* Reduce and dump per-thread statistics */

		// Sum up all LPs statistics
		for(i = 0; i < n_prc_per_thread; i++) {
			unsigned int lid = LPS_bound[i]->lid;

			thread_stats[tid].tot_antimessages += lp_stats[lid].tot_antimessages;
			thread_stats[tid].tot_events += lp_stats[lid].tot_events;
			thread_stats[tid].committed_events += lp_stats[lid].committed_events;
			thread_stats[tid].reprocessed_events += lp_stats[lid].reprocessed_events;
			thread_stats[tid].tot_rollbacks += lp_stats[lid].tot_rollbacks;
			thread_stats[tid].tot_ckpts += lp_stats[lid].tot_ckpts;
			thread_stats[tid].ckpt_time += lp_stats[lid].ckpt_time;
			thread_stats[tid].ckpt_mem += lp_stats[lid].ckpt_mem;
			thread_stats[tid].tot_recoveries += lp_stats[lid].tot_recoveries;
			thread_stats[tid].recovery_time += lp_stats[lid].recovery_time;
			thread_stats[tid].event_time += lp_stats[lid].event_time;
			thread_stats[tid].tot_reverse_gen += lp_stats[lid].tot_reverse_gen;
			thread_stats[tid].reverse_gen_time += lp_stats[lid].reverse_gen_time;
			thread_stats[tid].tot_reverse_exec += lp_stats[lid].tot_reverse_exec;
			thread_stats[tid].reverse_exec_time += lp_stats[lid].reverse_exec_time;
			thread_stats[tid].exponential_event_time += lp_stats[lid].exponential_event_time;
			thread_stats[tid].idle_cycles += lp_stats[lid].idle_cycles;
		}
		thread_stats[tid].exponential_event_time /= n_prc_per_thread;

		// Compute derived statistics and dump everything
		f = get_file(STAT_PER_THREAD, THREAD_STAT);
		rollback_frequency = (thread_stats[tid].tot_rollbacks / thread_stats[tid].tot_events);
		rollback_length = (thread_stats[tid].tot_rollbacks > 0
				   ? (thread_stats[tid].tot_events - thread_stats[tid].committed_events) / thread_stats[tid].tot_rollbacks
				   : 0);
		efficiency = (1 - rollback_frequency * rollback_length) * 100;

		fprintf(f, "------------------------------------------------------------\n");
		fprintf(f, "-------------------- THREAD STATISTICS ---------------------\n");
		fprintf(f, "------------------------------------------------------------\n\n");
		fprintf(f, "KERNEL ID ................. : %d \n", 		kid);
		fprintf(f, "LPs HOSTED BY KERNEL....... : %d \n", 		n_prc);
		fprintf(f, "TOTAL_THREADS ............. : %d \n", 		n_cores);
		fprintf(f, "THREAD ID ................. : %d \n", 		tid);
		fprintf(f, "LPs HOSTED BY THREAD ...... : %d \n", 		n_prc_per_thread);
		fprintf(f, "\n");
		fprintf(f, "TOTAL EXECUTED EVENTS ..... : %.0f \n", 		thread_stats[tid].tot_events);
		fprintf(f, "TOTAL COMMITTED EVENTS..... : %.0f \n", 		thread_stats[tid].committed_events);
		fprintf(f, "TOTAL REPROCESSED EVENTS... : %.0f \n", 		thread_stats[tid].reprocessed_events);
		fprintf(f, "TOTAL ROLLBACKS EXECUTED... : %.0f \n", 		thread_stats[tid].tot_rollbacks);
		fprintf(f, "TOTAL ANTIMESSAGES......... : %.0f \n", 		thread_stats[tid].tot_antimessages);
		fprintf(f, "ROLLBACK FREQUENCY......... : %.2f %%\n",		rollback_frequency * 100);
		fprintf(f, "ROLLBACK LENGTH............ : %.2f events\n",	rollback_length);
		fprintf(f, "EFFICIENCY................. : %.2f %%\n",		efficiency);
		fprintf(f, "AVERAGE EVENT COST......... : %.2f us\n",		thread_stats[tid].event_time / thread_stats[tid].tot_events);
		fprintf(f, "AVERAGE EVENT COST (EMA)... : %.2f us\n",		thread_stats[tid].exponential_event_time);
		fprintf(f, "AVERAGE CHECKPOINT COST.... : %.2f us\n",		thread_stats[tid].ckpt_time / thread_stats[tid].tot_ckpts);
		fprintf(f, "AVERAGE RECOVERY COST...... : %.2f us\n",		(thread_stats[tid].tot_recoveries > 0 ? thread_stats[tid].recovery_time / thread_stats[tid].tot_recoveries : 0));
		fprintf(f, "AVERAGE LOG SIZE........... : %s\n",		format_size(thread_stats[tid].ckpt_mem / thread_stats[tid].tot_ckpts));
		fprintf(f, "\n");
		fprintf(f, "AVERAGE REVERSE GENERATION. : %.3f us\n",		thread_stats[tid].reverse_gen_time / thread_stats[tid].tot_reverse_gen);
		fprintf(f, "AVERAGE REVERSE EXECUTION.. : %.3f us\n",		thread_stats[tid].reverse_exec_time / thread_stats[tid].tot_reverse_exec);
		fprintf(f, "IDLE CYCLES................ : %.0f\n",		thread_stats[tid].idle_cycles);
		fprintf(f, "NUMBER OF GVT REDUCTIONS... : %.0f\n",		thread_stats[tid].gvt_computations);
		fprintf(f, "SIMULATION TIME SPEED...... : %.2f units per GVT\n",thread_stats[tid].simtime_advancement);
		fprintf(f, "AVERAGE MEMORY USAGE....... : %s\n",		format_size(thread_stats[tid].memory_usage / thread_stats[tid].gvt_computations));

		if(exit_code == EXIT_FAILURE) {
			fprintf(f, "\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
		}
		if(exit_code == EXIT_SUCCESS) {
			fprintf(f, "\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
		}

		fflush(f);



		/* Reduce and dump per-thread statistics */
		/* (only one thread does the reduction, not necessarily the master) */


		thread_barrier(&all_thread_barrier);

		if(master_kernel() && master_thread()) {

			// Sum up all threads statistics
			for(i = 0; i < n_cores; i++) {
				system_wide_stats.tot_antimessages += thread_stats[i].tot_antimessages;
				system_wide_stats.tot_events += thread_stats[i].tot_events;
				system_wide_stats.committed_events += thread_stats[i].committed_events;
				system_wide_stats.reprocessed_events += thread_stats[i].reprocessed_events;
				system_wide_stats.tot_rollbacks += thread_stats[i].tot_rollbacks;
				system_wide_stats.tot_ckpts += thread_stats[i].tot_ckpts;
				system_wide_stats.ckpt_time += thread_stats[i].ckpt_time;
				system_wide_stats.ckpt_mem += thread_stats[i].ckpt_mem;
				system_wide_stats.tot_recoveries += thread_stats[i].tot_recoveries;
				system_wide_stats.recovery_time += thread_stats[i].recovery_time;
				system_wide_stats.event_time += thread_stats[i].event_time;
				system_wide_stats.exponential_event_time += thread_stats[i].exponential_event_time;
				system_wide_stats.tot_reverse_gen += thread_stats[i].tot_reverse_gen;
				system_wide_stats.reverse_gen_time += thread_stats[i].reverse_gen_time;
				system_wide_stats.tot_reverse_exec += thread_stats[i].tot_reverse_exec;
				system_wide_stats.reverse_exec_time += thread_stats[i].reverse_exec_time;
				system_wide_stats.idle_cycles += thread_stats[i].idle_cycles;
				system_wide_stats.memory_usage += thread_stats[i].memory_usage;
				system_wide_stats.simtime_advancement += thread_stats[i].simtime_advancement;
			}
			system_wide_stats.exponential_event_time /= n_cores;

			// GVT computations are the same for all threads
			system_wide_stats.gvt_computations += thread_stats[0].gvt_computations;

			// Compute derived statistics and dump everything
			f = get_file(STAT_UNIQUE, GLOBAL_STAT); // TODO: quando reintegriamo il distribuito, la selezione del file qui deve cambiare
			rollback_frequency = (system_wide_stats.tot_rollbacks / system_wide_stats.tot_events);
			rollback_length = (system_wide_stats.tot_rollbacks > 0
					   ? (system_wide_stats.tot_events - system_wide_stats.committed_events) / system_wide_stats.tot_rollbacks
					   : 0);
			efficiency = (1 - rollback_frequency * rollback_length) * 100;


			fprintf(f, "------------------------------------------------------------\n");
			fprintf(f, "-------------------- GLOBAL STATISTICS ---------------------\n");
			fprintf(f, "------------------------------------------------------------\n\n");

			// TODO: quando reintegriamo il distribuito, queste stampe vanno nel file globale
			timer_tostring(simulation_timer, timer_string);
			fprintf(f, "SIMULATION STARTED AT ..... : %s \n", 		timer_string);
			bzero(timer_string, 64);
			timer_tostring(simulation_finished, timer_string);
			fprintf(f, "SIMULATION FINISHED AT .... : %s \n", 		timer_string);
			fprintf(f, "TOTAL SIMULATION TIME ..... : %.03f seconds \n",	total_time);

			fprintf(f, "\n");
			fprintf(f, "TOTAL KERNELS ............. : %d \n", 		n_ker);
			fprintf(f, "KERNEL ID ................. : %d \n", 		kid);
			fprintf(f, "LPs HOSTED BY KERNEL....... : %d \n", 		n_prc);
			fprintf(f, "TOTAL_THREADS ............. : %d \n", 		n_cores);
			fprintf(f, "\n");
			fprintf(f, "TOTAL EXECUTED EVENTS ..... : %.0f \n", 		system_wide_stats.tot_events);
			fprintf(f, "TOTAL COMMITTED EVENTS..... : %.0f \n", 		system_wide_stats.committed_events);
			fprintf(f, "TOTAL REPROCESSED EVENTS... : %.0f \n", 		system_wide_stats.reprocessed_events);
			fprintf(f, "TOTAL ROLLBACKS EXECUTED... : %.0f \n", 		system_wide_stats.tot_rollbacks);
			fprintf(f, "TOTAL ANTIMESSAGES......... : %.0f \n", 		system_wide_stats.tot_antimessages);
			fprintf(f, "ROLLBACK FREQUENCY......... : %.2f %%\n",		rollback_frequency * 100);
			fprintf(f, "ROLLBACK LENGTH............ : %.2f events\n",	rollback_length);
			fprintf(f, "EFFICIENCY................. : %.2f %%\n",		efficiency);
			fprintf(f, "AVERAGE EVENT COST......... : %.2f us\n",		system_wide_stats.event_time / system_wide_stats.tot_events);
			fprintf(f, "AVERAGE EVENT COST (EMA)... : %.2f us\n",		system_wide_stats.exponential_event_time);
			fprintf(f, "AVERAGE CHECKPOINT COST.... : %.2f us\n",		system_wide_stats.ckpt_time / system_wide_stats.tot_ckpts);
			fprintf(f, "AVERAGE RECOVERY COST...... : %.3f us\n",		(system_wide_stats.tot_recoveries > 0 ? system_wide_stats.recovery_time / system_wide_stats.tot_recoveries : 0 ));
			fprintf(f, "AVERAGE LOG SIZE........... : %s\n",		format_size(system_wide_stats.ckpt_mem / system_wide_stats.tot_ckpts));
			fprintf(f, "\n");
			fprintf(f, "AVERAGE REVERSE GENERATION. : %.3f us\n",		system_wide_stats.reverse_gen_time / system_wide_stats.tot_reverse_gen);
			fprintf(f, "AVERAGE REVERSE EXECUTION.. : %.3f us\n",		system_wide_stats.reverse_exec_time / system_wide_stats.tot_reverse_exec);
			fprintf(f, "\n");
			fprintf(f, "IDLE CYCLES................ : %.0f\n",		system_wide_stats.idle_cycles);
			fprintf(f, "LAST COMMITTED GVT ........ : %f\n",		get_last_gvt());
			fprintf(f, "NUMBER OF GVT REDUCTIONS... : %.0f\n",		system_wide_stats.gvt_computations);
			fprintf(f, "SIMULATION TIME SPEED...... : %.2f units per GVT\n",system_wide_stats.simtime_advancement);
			fprintf(f, "AVERAGE MEMORY USAGE....... : %s\n",		format_size(system_wide_stats.memory_usage / system_wide_stats.gvt_computations));
			fprintf(f, "PEAK MEMORY USAGE.......... : %s\n",		format_size(getPeakRSS()));

			if(exit_code == EXIT_FAILURE) {
				fprintf(f, "\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
			}
			if(exit_code == EXIT_SUCCESS) {
				fprintf(f, "\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
			}

			fflush(f);
		}

		// TODO: quando reintegriamo la parte distribuita, qui si deve fare la riduzione tra tutti i kernel

		// Write the final outcome of the simulation on screen
		if (master_kernel() && master_thread()) {
			if(exit_code == EXIT_FAILURE) {
				printf("\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
			}
			if(exit_code == EXIT_SUCCESS) {
				printf("\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
			}
		}
	}
}


static inline void statistics_flush_gvt(double gvt) {
	register unsigned int i;
	unsigned int committed = 0;
	static __thread unsigned int cumulated = 0;
	double exec_time;
	char gvt_line[512];
	int gvt_line_len;

	// Dump on file only if required
	if( rootsim_config.stats != STATS_ALL && rootsim_config.stats != STATS_PERF) {
		return;
	}

	exec_time = timer_value_seconds(simulation_timer);

	// Reduce the committed events from all LPs
	for(i = 0; i < n_prc_per_thread; i++) {
		committed += lp_stats_gvt[LPS_bound[i]->lid].committed_events;
	}
	cumulated += committed;

	// If we are using a higher level of statistics, buffer data and eventually dump on file
	if(rootsim_config.stats == STATS_PERF || rootsim_config.stats == STATS_LP || rootsim_config.stats ==  STATS_ALL) {

		snprintf(gvt_line, 512, "%f\t%f\t%d\t%d\n\0", exec_time, gvt, committed, cumulated);
		gvt_line_len = strlen(gvt_line);

		if(gvt_buffer.pos + gvt_line_len > GVT_BUFF_SIZE - 2) {
			statistics_flush_gvt_buffer();
		}

		strcat(gvt_buffer.buff, gvt_line);
		gvt_buffer.pos += gvt_line_len;
	}
}


/**
* This function initializes the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_init(void) {
	register unsigned int i;
	char thread_dir[MAX_PATHLEN];

	// The whole reduction for the sequential simulation is simply done at the end
	if(rootsim_config.serial) {
		// Purge old output dir if present and create new one
		_rmdir(rootsim_config.output_dir);
		_mkdir(rootsim_config.output_dir);

		return;
	}


	// Master thread directories to keep
	// statistics from all threads
	if(master_thread()) {
		// Purge old output dir if present
		_rmdir(rootsim_config.output_dir);
		_mkdir(rootsim_config.output_dir);

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
	lp_stats = rsalloc(n_prc * sizeof(struct stat_t));
	bzero(lp_stats, n_prc * sizeof(struct stat_t));
	lp_stats_gvt = rsalloc(n_prc * sizeof(struct stat_t));
	bzero(lp_stats_gvt, n_prc * sizeof(struct stat_t));
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


void statistics_post_lp_data(unsigned int lid, unsigned int type, double data) {

	if(rootsim_config.serial) {

		switch(type) {
			case STAT_EVENT:
				system_wide_stats.tot_events += 1.0;
				break;

			case STAT_EVENT_TIME:
				system_wide_stats.event_time += data;
				system_wide_stats.exponential_event_time = 0.1 * data + 0.9 * system_wide_stats.exponential_event_time;
				break;

			default:
				rootsim_error(true, "Wrong LP statistics post type: %d. Aborting...\n", type);
		}

	} else {

		switch(type) {

			case STAT_ANTIMESSAGE:
				lp_stats_gvt[lid].tot_antimessages += 1.0;
				break;

			case STAT_EVENT:
				lp_stats_gvt[lid].tot_events += 1.0;
				break;

			case STAT_EVENT_TIME:
				lp_stats_gvt[lid].event_time += data;
				lp_stats_gvt[lid].exponential_event_time = 0.1 * data + 0.9 * lp_stats_gvt[lid].exponential_event_time;
				break;

			case STAT_REVERSE_EXECUTE:
				lp_stats_gvt[lid].tot_reverse_exec += 1.0;
				break;

			case STAT_REVERSE_EXECUTE_TIME:
				lp_stats_gvt[lid].reverse_exec_time += data;
				break;

			case STAT_REVERSE_GENERATE:
				lp_stats_gvt[lid].tot_reverse_gen += 1.0;
				break;

			case STAT_REVERSE_GENERATE_TIME:
				lp_stats_gvt[lid].reverse_gen_time += data;
				break;

			case STAT_COMMITTED:
				lp_stats_gvt[lid].committed_events += data;
				break;

			case STAT_ROLLBACK:
				lp_stats_gvt[lid].tot_rollbacks += 1.0;
				break;

			case STAT_CKPT:
				lp_stats_gvt[lid].tot_ckpts += 1.0;
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
				thread_stats[tid].idle_cycles++;
				break;

			case STAT_SILENT:
				lp_stats_gvt[lid].reprocessed_events += data;
				break;

			default:
				rootsim_error(true, "Wrong LP statistics post type: %d. Aborting...\n", type);
		}
	}
}


void statistics_post_other_data(unsigned int type, double data) {
	register unsigned int i;
	double simtime_advancement;

	if(rootsim_config.serial) {
		switch(type) {

			case STAT_SIM_START:
				statistics_start();
				break;

			case STAT_GVT:
				system_wide_stats.gvt_computations += 1.0;
				system_wide_stats.memory_usage += (double)getCurrentRSS();

				simtime_advancement = data - system_wide_stats.gvt_time;
				if(D_DIFFER_ZERO(system_wide_stats.simtime_advancement)) {
					// Exponential moving average
					system_wide_stats.simtime_advancement =
						0.1 * simtime_advancement +
						0.9 * system_wide_stats.simtime_advancement;
				} else {
					system_wide_stats.simtime_advancement = simtime_advancement;
				}

				system_wide_stats.gvt_time = data;
				break;

			default:
				rootsim_error(true, "Wrong statistics post type: %d. Aborting...\n", type);
		}

		return;
	}

	switch(type) {

		case STAT_SIM_START:
			statistics_start();
			break;

		// Sum up all that happened in the last GVT phase, in case it is required,
		// dump a line on the corresponding statistics file
		case STAT_GVT:

			statistics_flush_gvt(data);

			thread_stats[tid].memory_usage += (double)getCurrentRSS();
			thread_stats[tid].gvt_computations += 1.0;
			simtime_advancement = data - thread_stats[tid].gvt_time;

			if(D_DIFFER_ZERO(thread_stats[tid].simtime_advancement)) {
				// Exponential moving average
				thread_stats[tid].simtime_advancement =
					0.1 * simtime_advancement +
					0.9 * thread_stats[tid].simtime_advancement;
			} else {
				thread_stats[tid].simtime_advancement = simtime_advancement;
			}

			thread_stats[tid].gvt_time = data;

			for(i = 0; i < n_prc_per_thread; i++) {
				unsigned int lid = LPS_bound[i]->lid;
				double keep_exponential_event_time;

				lp_stats[lid].tot_antimessages += lp_stats_gvt[lid].tot_antimessages;
				lp_stats[lid].tot_events += lp_stats_gvt[lid].tot_events;
				lp_stats[lid].event_time += lp_stats_gvt[lid].event_time;
				lp_stats[lid].exponential_event_time = lp_stats_gvt[lid].exponential_event_time;
				lp_stats[lid].tot_reverse_exec += lp_stats_gvt[lid].tot_reverse_exec;
				lp_stats[lid].reverse_exec_time += lp_stats_gvt[lid].reverse_exec_time;
				lp_stats[lid].tot_reverse_gen += lp_stats_gvt[lid].tot_reverse_gen;
				lp_stats[lid].reverse_gen_time += lp_stats_gvt[lid].reverse_gen_time;
				lp_stats[lid].committed_events += lp_stats_gvt[lid].committed_events;
				lp_stats[lid].tot_rollbacks += lp_stats_gvt[lid].tot_rollbacks;
				lp_stats[lid].tot_ckpts += lp_stats_gvt[lid].tot_ckpts;
				lp_stats[lid].ckpt_mem += lp_stats_gvt[lid].ckpt_mem;
				lp_stats[lid].ckpt_time += lp_stats_gvt[lid].ckpt_time;
				lp_stats[lid].tot_recoveries += lp_stats_gvt[lid].tot_recoveries;
				lp_stats[lid].recovery_time += lp_stats_gvt[lid].recovery_time;
				lp_stats[lid].reprocessed_events += lp_stats_gvt[lid].reprocessed_events;

				keep_exponential_event_time = lp_stats_gvt[lid].exponential_event_time;
				bzero(&lp_stats_gvt[LPS_bound[i]->lid], sizeof(struct stat_t));
				lp_stats_gvt[lid].exponential_event_time = keep_exponential_event_time;
			}
			break;

		default:
			rootsim_error(true, "Wrong statistics post type: %d. Aborting...\n", type);
	}
}


double statistics_get_data(unsigned int type, double data) {
	double ret;

	switch(type) {

		/* Per thread statistics */
		case STAT_GET_SIMTIME_ADVANCEMENT:
			ret = thread_stats[tid].simtime_advancement;
			break;

		/* per LP statistics */
		case STAT_GET_ROLLBACK_FREQ:
			ret = lp_stats[(int)data].tot_rollbacks / lp_stats[(int)data].tot_events;
			break;

		case STAT_GET_EVENT_TIME_LP:
			ret = lp_stats[(int)data].exponential_event_time;
			break;

		#ifdef HAVE_REVERSE
		case STAT_GET_UNDO_EVENT_COST:
			if(lp_stats[(int)data].tot_reverse_exec > 0)
				ret = lp_stats[(int)data].reverse_exec_time / lp_stats[(int)data].tot_reverse_exec;
			else
				ret = 0; // This could create a bias which is nevertheless usefult to learn the cost
			break;
		#endif

		case STAT_GET_FULL_CKPT_TIME:
			ret = lp_stats[(int)data].ckpt_time / lp_stats[(int)data].tot_ckpts;
			break;

		case STAT_GET_FULL_RECOVERY_TIME:
			ret = lp_stats[(int)data].recovery_time / lp_stats[(int)data].tot_recoveries;
			break;

		default:
			rootsim_error(true, "Wrong statistics get type: %d. Aborting...\n", type);
	}

	return ret;
}
