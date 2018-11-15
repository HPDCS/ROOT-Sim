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
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>
#include <queues/queues.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

#define GVT_BUFF_ROWS   50
// Format string length + the width of a field * fields count + 20 for good measure
#define GVT_LINE_BUFF_LEN ((sizeof(GVT_FORMAT_STR) + 15 * 4 + 20) * GVT_BUFF_ROWS)

/// This is the format string used to print periodically the gvt statistics
#define GVT_FORMAT_STR 	" %15lf    %15lf    %15u    %15u\n"

struct _gvt_buffer {
	unsigned int pos;
	struct _gvt_buffer_row_t{
		double exec_time;
		double gvt;
		unsigned committed;
		unsigned cumulated;
	}row[GVT_BUFF_ROWS];
	char line_buffer[GVT_LINE_BUFF_LEN];
};

/// This structure is used to buffer statistics gathered on GVT computation
__thread struct _gvt_buffer gvt_buf;

/// This is a timer that start during the initialization of statistics subsystem and can be used to know the total simulation time
static timer simulation_timer;

/// Pointers to unique files
static FILE *unique_files[NUM_STAT_FILE_U];
/// Pointers to per-thread files
static FILE **thread_files[NUM_STAT_FILE_T];

/// Keeps statistics on a per-LP basis
static struct stat_t *lp_stats;

/// Keeps statistics on a per-LP basis in a GVT phase
static struct stat_t *lp_stats_gvt;

/// Keeps statistics on a per-thread basis
static struct stat_t *thread_stats;

/// Keeps global statistics
static struct stat_t system_wide_stats;

/*!
 * @brief This is a pseudo asprintf() implementation needed in order to stop GCC 8 from complaining
 * @param format the format string as in the real asprintf()
 * @param ... the arguments list as in the real asprintf()
 * @returns a mallocated string containing the snprintf()-processed string
 *
 *	TODO: transform into a function for safety
 * GCC 8 cares a lot for our security so we have to be sure snprintf() doesn't truncate.
 */
#define safe_asprintf(ret_addr, format, ...) ({					\
		char *__pstr = NULL;						\
		int __ret = snprintf(0, 0, format, ##__VA_ARGS__);		\
		if(__ret < 0)							\
			rootsim_error(true, "Error in snprintf()!");		\
		__pstr = rsalloc(__ret + 1);					\
		__ret = snprintf(__pstr, __ret + 1, format, ##__VA_ARGS__);	\
		if(__ret < 0)							\
			rootsim_error(true, "Error in snprintf()!");		\
		*(ret_addr) = __pstr;						\
})


/**
* This is an helper-function to recursively delete the whole content of a folder
*
* @param path The path of the directory to purge
*/
static void _rmdir(const char *path) {
	char *buf;
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

		safe_asprintf(&buf, "%s/%s", path, dirt->d_name);

		if (stat(buf, &st) == -1) {
			rootsim_error(false, "stat() file \"%s\" failed, %s\n", buf, strerror(errno));
		}else if (S_ISLNK(st.st_mode) || S_ISREG(st.st_mode)) {
			if (unlink(buf) == -1)
				rootsim_error(false, "unlink() file \"%s\" failed, %s\n", buf, strerror(errno));
		}
		else if (S_ISDIR(st.st_mode)) {
			_rmdir(buf);
		}

		rsfree(buf);
	}
	closedir(dir);

	if (rmdir(path) == -1)
		rootsim_error(false, "rmdir() directory \"%s\" failed, %s\n", path, strerror(errno));
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

static void print_config_to_file(FILE *f){
	 fprintf(f,
		"****************************\n"
		"*  ROOT-Sim Configuration  *\n"
		"****************************\n"
		"Kernels: %u\n"
		"Cores: %ld available, %d used\n"
		"Number of Logical Processes: %u\n"
		"Output Statistics Directory: %s\n"
		"Scheduler: %s\n"
		#ifdef HAVE_MPI
		"MPI multithread support: %s\n"
		#endif
		"GVT Time Period: %.2f seconds\n"
		"Checkpointing Type: %s\n"
		"Checkpointing Period: %d\n"
		"Snapshot Reconstruction Type: %s\n"
		"Halt Simulation After: %d\n"
		"LPs Distribution Mode across Kernels: %s\n"
		"Check Termination Mode: %s\n"
		"Blocking GVT: %s\n"
		"Set Seed: %ld\n",
		n_ker,
		get_cores(),
		n_cores,
		n_prc_tot,
		rootsim_config.output_dir,
		param_to_text[PARAM_SCHEDULER][rootsim_config.scheduler],
		#ifdef HAVE_MPI
		((mpi_support_multithread)? "yes":"no"),
		#endif
		rootsim_config.gvt_time_period / 1000.0,
		param_to_text[PARAM_STATE_SAVING][rootsim_config.checkpointing],
		rootsim_config.ckpt_period,
		param_to_text[PARAM_SNAPSHOT][rootsim_config.snapshot],
		rootsim_config.simulation_time,
		param_to_text[PARAM_LPS_DISTRIBUTION][rootsim_config.lps_distribution],
		param_to_text[PARAM_CKTRM_MODE][rootsim_config.check_termination_mode],
		((rootsim_config.blocking_gvt)? "yes":"no"),
		rootsim_config.set_seed);
}


void print_config(void)	{if(master_kernel()) print_config_to_file(stdout);}


/**
* This function manage the simulation time used to in the statistics, and print to the output file the starting simulation header
*
* @author Francesco Quaglia
* @author Roberto Vitali
*
*/
void statistics_start(void) {
	register unsigned int i;

	// Print the header of GVT statistics files
	if (!rootsim_config.serial && (rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF)) {
		for(i = 0; i < n_cores; i++) {
			fprintf(thread_files[STAT_FILE_T_GVT][i], "#%15.15s    %15.15s    %15.15s    %15.15s\n",
					"\"WCT\"", "\"GVT VALUE\"", "\"EVENTS\"", "\"CUMUL EVENTS\"");
			fflush(thread_files[STAT_FILE_T_GVT][i]);
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

	snprintf(size_str, 32, fmt, size / divisor);

	return size_str;
}


static inline void statistics_flush_gvt_buffer(void) {
	unsigned i, len = gvt_buf.pos;
	size_t used = 0;
	char *buf = gvt_buf.line_buffer;
	// fill the "line" buffer with content
	// xxx this clearly isn't exactly 100% safe:
	// printf() width specifiers are MINIMUM bounds
	for(i = 0; i < len; ++i){
		used += snprintf(buf + used, GVT_LINE_BUFF_LEN - used, GVT_FORMAT_STR,
				gvt_buf.row[i].exec_time, gvt_buf.row[i].gvt,gvt_buf.row[i].committed, gvt_buf.row[i].cumulated);

		if(used >= GVT_LINE_BUFF_LEN){
			buf[GVT_LINE_BUFF_LEN - 1] = '\0';
			break;
		}
	}
	fprintf(thread_files[STAT_FILE_T_GVT][local_tid], "%s", buf);
	gvt_buf.pos = 0;
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


	if(rootsim_config.serial) {
		f = unique_files[STAT_FILE_U_GLOBAL];

		// Stop timers
		timer_start(simulation_finished);
		total_time = timer_value_seconds(simulation_timer);

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
		if (master_thread()) {
			timer_start(simulation_finished);
			total_time = timer_value_seconds(simulation_timer);
		}

		/* Finish flushing GVT statistics */
		statistics_flush_gvt_buffer();
		fflush(thread_files[STAT_FILE_T_GVT][local_tid]);

		/* dump per-LP statistics if required. They are already reduced during the GVT phase */
		if(rootsim_config.stats == STATS_LP || rootsim_config.stats == STATS_ALL) {
			f = thread_files[STAT_FILE_T_LP][local_tid];

			fprintf(f, "#%15.15s   %15.15s   %15.15s   %15.15s   %15.15s   %15.15s   %15.15s   %15.15s   %15.15s   %15.15s   %15.15s\n",
					"\"GID\"", "\"LID\"", "\"TOTAL EVENTS\"", "\"COMM EVENTS\"", "\"REPROC EVENTS\"", "\"ROLLBACKS\"", "\"ANTIMSG\"",
					"\"AVG EVT COST\"", "\"AVG CKPT COST\"", "\"AVG REC COST\"", "\"IDLE CYCLES\"");

			int __helper_show_lp_stats(LID_t the_lid, GID_t the_gid, unsigned lid, void *unused){
				(void)unused, (void)the_lid;
				fprintf(f, " %15u   ", 		gid_to_int(the_gid));
				fprintf(f, "%15u   ", 		lid);
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].tot_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].committed_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].reprocessed_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].tot_rollbacks);
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].tot_antimessages);
				fprintf(f, "%15lf   ", 		lp_stats[lid].event_time / lp_stats[lid].tot_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].ckpt_time / lp_stats[lid].tot_ckpts);
				fprintf(f, "%15.0lf   ", 	(lp_stats[lid].tot_rollbacks > 0 ? lp_stats[lid].recovery_time / lp_stats[lid].tot_recoveries : 0));
				fprintf(f, "%15.0lf   ", 	lp_stats[lid].idle_cycles);
				fprintf(f, "\n");
				return 0; // process each lp
			}
			LPS_bound_foreach(__helper_show_lp_stats, NULL);
		}


		/* Reduce and dump per-thread statistics */

		// Sum up all LPs statistics
		int __helper_sum_lp_stats(LID_t the_lid, GID_t the_gid, unsigned lid, void *unused){
			(void)unused, (void)the_lid, (void)the_gid;
			thread_stats[local_tid].vec += lp_stats[lid].vec;
			return 0;
		}
		LPS_bound_foreach(__helper_sum_lp_stats, NULL);
		thread_stats[local_tid].exponential_event_time /= n_prc_per_thread;

		// Compute derived statistics and dump everything
		f = thread_files[STAT_FILE_T_THREAD][local_tid];
		rollback_frequency = (thread_stats[local_tid].tot_rollbacks / thread_stats[local_tid].tot_events);
		rollback_length = (thread_stats[local_tid].tot_rollbacks > 0
				   ? (thread_stats[local_tid].tot_events - thread_stats[local_tid].committed_events) / thread_stats[local_tid].tot_rollbacks
				   : 0);
		efficiency = (1 - rollback_frequency * rollback_length) * 100;

		fprintf(f, "------------------------------------------------------------\n");
		fprintf(f, "-------------------- THREAD STATISTICS ---------------------\n");
		fprintf(f, "------------------------------------------------------------\n\n");

		fprintf(f, "KERNEL ID ................. : %d \n", 		kid);
		fprintf(f, "LPs HOSTED BY KERNEL....... : %d \n", 		n_prc);
		fprintf(f, "TOTAL_THREADS ............. : %d \n", 		n_cores);
		fprintf(f, "THREAD ID ................. : %d \n", 		local_tid);
		fprintf(f, "LPs HOSTED BY THREAD ...... : %d \n", 		n_prc_per_thread);
		fprintf(f, "\n");
		fprintf(f, "TOTAL EXECUTED EVENTS ..... : %.0f \n", 		thread_stats[local_tid].tot_events);
		fprintf(f, "TOTAL COMMITTED EVENTS..... : %.0f \n", 		thread_stats[local_tid].committed_events);
		fprintf(f, "TOTAL REPROCESSED EVENTS... : %.0f \n", 		thread_stats[local_tid].reprocessed_events);
		fprintf(f, "TOTAL ROLLBACKS EXECUTED... : %.0f \n", 		thread_stats[local_tid].tot_rollbacks);
		fprintf(f, "TOTAL ANTIMESSAGES......... : %.0f \n", 		thread_stats[local_tid].tot_antimessages);
		fprintf(f, "ROLLBACK FREQUENCY......... : %.2f %%\n",		rollback_frequency * 100);
		fprintf(f, "ROLLBACK LENGTH............ : %.2f events\n",	rollback_length);
		fprintf(f, "EFFICIENCY................. : %.2f %%\n",		efficiency);
		fprintf(f, "AVERAGE EVENT COST......... : %.2f us\n",		thread_stats[local_tid].event_time / thread_stats[local_tid].tot_events);
		fprintf(f, "AVERAGE EVENT COST (EMA)... : %.2f us\n",		thread_stats[local_tid].exponential_event_time);
		fprintf(f, "AVERAGE CHECKPOINT COST.... : %.2f us\n",		thread_stats[local_tid].ckpt_time / thread_stats[local_tid].tot_ckpts);
		fprintf(f, "AVERAGE RECOVERY COST...... : %.2f us\n",		(thread_stats[local_tid].tot_recoveries > 0 ? thread_stats[local_tid].recovery_time / thread_stats[local_tid].tot_recoveries : 0));
		fprintf(f, "AVERAGE LOG SIZE........... : %s\n",		format_size(thread_stats[local_tid].ckpt_mem / thread_stats[local_tid].tot_ckpts));
		fprintf(f, "IDLE CYCLES................ : %.0f\n",		thread_stats[local_tid].idle_cycles);
		fprintf(f, "NUMBER OF GVT REDUCTIONS... : %.0f\n",		thread_stats[local_tid].gvt_computations);
		fprintf(f, "SIMULATION TIME SPEED...... : %.2f units per GVT\n",thread_stats[local_tid].simtime_advancement);
		fprintf(f, "AVERAGE MEMORY USAGE....... : %s\n",		format_size(thread_stats[local_tid].memory_usage / thread_stats[local_tid].gvt_computations));

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

		if(master_thread()) {

			// Sum up all threads statistics
			for(i = 0; i < n_cores; i++) {
				system_wide_stats.vec += thread_stats[i].vec;
			}
			system_wide_stats.exponential_event_time /= n_cores;

			// GVT computations are the same for all threads
			system_wide_stats.gvt_computations /= n_cores;

			// Compute derived statistics and dump everything
			f = unique_files[STAT_FILE_U_GLOBAL]; // TODO: quando reintegriamo il distribuito, la selezione del file qui deve cambiare
			rollback_frequency = (system_wide_stats.tot_rollbacks / system_wide_stats.tot_events);
			rollback_length = (system_wide_stats.tot_rollbacks > 0
					   ? (system_wide_stats.tot_events - system_wide_stats.committed_events) / system_wide_stats.tot_rollbacks
					   : 0);
			efficiency = (1 - rollback_frequency * rollback_length) * 100;

			print_config_to_file(f);

			fprintf(f, "\n");
			fprintf(f, "------------------------------------------------------------\n");
			fprintf(f, "-------------------- GLOBAL STATISTICS ---------------------\n");
			fprintf(f, "------------------------------------------------------------\n\n");

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
			fprintf(f, "IDLE CYCLES................ : %.0f\n",		system_wide_stats.idle_cycles);
			fprintf(f, "LAST COMMITTED GVT ........ : %f\n",		get_last_gvt());
			fprintf(f, "NUMBER OF GVT REDUCTIONS... : %.0f\n",		system_wide_stats.gvt_computations);
			if(n_ker > 1){
				fprintf(f, "MIN GVT ROUND TIME......... : %.2f us\n",	system_wide_stats.gvt_round_time_min);
				fprintf(f, "MAX GVT ROUND TIME......... : %.2f us\n",	system_wide_stats.gvt_round_time_max);
				fprintf(f, "AVERAGE GVT ROUND TIME..... : %.2f us\n",	system_wide_stats.gvt_round_time / system_wide_stats.gvt_computations);
			}
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

			if(master_kernel()){
				// TODO: do the statistics reduction between all the MPI kernels in another file
				// Write the final outcome of the simulation on screen
				if(exit_code == EXIT_FAILURE) {
					printf("\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
				}
				if(exit_code == EXIT_SUCCESS) {
					printf("\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
				}
			}
		}
	}
}


// Sum up all that happened in the last GVT phase, in case it is required,
// dump a line on the corresponding statistics file
inline void statistics_on_gvt(double gvt) {
	unsigned int i, lid;
	unsigned int committed = 0;
	static __thread unsigned int cumulated = 0;
	double exec_time, simtime_advancement, keep_exponential_event_time;

	// Dump on file only if required
	if(rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF) {
		exec_time = timer_value_seconds(simulation_timer);

		// Reduce the committed events from all LPs
		for(i = 0; i < n_prc_per_thread; i++) {
			committed += lp_stats_gvt[lid_to_int(LPS_bound(i)->lid)].committed_events;
		}
		cumulated += committed;

		// fill the row
		gvt_buf.row[gvt_buf.pos] = (struct _gvt_buffer_row_t){exec_time, gvt, committed, cumulated};
		gvt_buf.pos++;
		// check if buffer is full
		if(gvt_buf.pos >= GVT_BUFF_ROWS){
			statistics_flush_gvt_buffer();
		}
	}

	thread_stats[local_tid].memory_usage += (double)getCurrentRSS();
	thread_stats[local_tid].gvt_computations += 1.0;

	simtime_advancement = gvt - thread_stats[local_tid].gvt_time;
	if(D_DIFFER_ZERO(thread_stats[local_tid].simtime_advancement)) {
		// Exponential moving average
		thread_stats[local_tid].simtime_advancement =
			0.1 * simtime_advancement +
			0.9 * thread_stats[local_tid].simtime_advancement;
	} else {
		thread_stats[local_tid].simtime_advancement = simtime_advancement;
	}
	thread_stats[local_tid].gvt_time = gvt;

	for(i = 0; i < n_prc_per_thread; i++) {
		lid = lid_to_int(LPS_bound(i)->lid);

		lp_stats[lid].vec += lp_stats_gvt[lid].vec;

		lp_stats[lid].exponential_event_time = lp_stats_gvt[lid].exponential_event_time;

		keep_exponential_event_time = lp_stats_gvt[lid].exponential_event_time;
		bzero(&lp_stats_gvt[lid_to_int(LPS_bound(i)->lid)], sizeof(struct stat_t));
		lp_stats_gvt[lid].exponential_event_time = keep_exponential_event_time;
	}
}


inline void statistics_on_gvt_serial(double gvt){
	system_wide_stats.gvt_computations += 1.0;
	system_wide_stats.memory_usage += (double)getCurrentRSS();

	double simtime_advancement = gvt - system_wide_stats.gvt_time;
	if(D_DIFFER_ZERO(system_wide_stats.simtime_advancement)) {
		// Exponential moving average
		system_wide_stats.simtime_advancement =
			0.1 * simtime_advancement +
			0.9 * system_wide_stats.simtime_advancement;
	} else {
		system_wide_stats.simtime_advancement = simtime_advancement;
	}

	system_wide_stats.gvt_time = gvt;
}


#define assign_new_file(destination, format, ...) \
	do{\
		safe_asprintf(&name_buf, "%s/" format, base_path, ##__VA_ARGS__);\
		if (((destination) = fopen(name_buf, "w")) == NULL)\
			rootsim_error(true, "Cannot open %s\n", name_buf);\
		rsfree(name_buf);\
	}while(0)

/**
* This function initializes the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_init(void) {
	unsigned int i;
	char *name_buf = NULL;
	char *base_path = NULL;

	// this is needed in order to suppress a race condition when multiple kernels
	// share the same filesystem
	if(n_ker > 1){
		safe_asprintf(&base_path, "%s_%u", rootsim_config.output_dir, kid);
	}
	else{
		safe_asprintf(&base_path, "%s", rootsim_config.output_dir);
	}

	// Purge old output dir if present
	_rmdir(base_path);
	_mkdir(base_path);

	// The whole reduction for the sequential simulation is simply done at the end
	if(rootsim_config.serial){
		assign_new_file(unique_files[STAT_FILE_U_GLOBAL], "sequential_stats");
		rsfree(base_path);
		return;
	}

	for(i = 0; i < n_cores; i++) {
		safe_asprintf(&name_buf, "%s/thread_%u/", base_path, i);
		_rmdir(name_buf);
		_mkdir(name_buf);
		rsfree(name_buf);
	}

	// Allocate entries for file arrays (and set them to NULL, for later closing)
	bzero(unique_files, sizeof(FILE *) * NUM_STAT_FILE_U);
	for(i = 0; i < NUM_STAT_FILE_T; i++) {
		thread_files[i] = rsalloc(sizeof(FILE *) * n_cores);
		bzero(thread_files[i], sizeof(FILE *) * n_cores);
	}

	// create the unique file
	assign_new_file(unique_files[STAT_FILE_U_GLOBAL], STAT_FILE_NAME_GLOBAL);

	// Create files depending on the actual level of verbosity
	switch(rootsim_config.stats){
		case STATS_ALL:
		case STATS_LP:
			for(i = 0; i < n_cores; ++i) {
				assign_new_file(thread_files[STAT_FILE_T_LP][i], "thread_%u/%s", i, STAT_FILE_NAME_LP);
			}
			if(rootsim_config.stats == STATS_LP) goto stats_lp_jump;
			/* fall through */
		case STATS_PERF:
			for(i = 0; i < n_cores; ++i) {
				assign_new_file(thread_files[STAT_FILE_T_GVT][i], "thread_%u/%s", i, STAT_FILE_NAME_GVT);
			}
			/* fall through */
		case STATS_GLOBAL:
			stats_lp_jump:
			for(i = 0; i < n_cores; ++i) {
				assign_new_file(thread_files[STAT_FILE_T_THREAD][i], "thread_%u/%s", i, STAT_FILE_NAME_THREAD);
			}
		break;
		default:
			rootsim_error(true, "unrecognized statistics option '%d'!", rootsim_config.stats);
	}

	rsfree(base_path);

	// Initialize data structures to keep information
	lp_stats = rsalloc(n_prc * sizeof(struct stat_t));
	bzero(lp_stats, n_prc * sizeof(struct stat_t));
	lp_stats_gvt = rsalloc(n_prc * sizeof(struct stat_t));
	bzero(lp_stats_gvt, n_prc * sizeof(struct stat_t));
	thread_stats = rsalloc(n_cores * sizeof(struct stat_t));
	bzero(thread_stats, n_cores * sizeof(struct stat_t));

	memset(&system_wide_stats, 0, sizeof(struct stat_t));

	system_wide_stats.gvt_round_time_min = INFTY;
}

#undef assign_new_file


/**
* This function finalize the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_fini(void) {
	register unsigned int i, j;

	for(i = 0; i < NUM_STAT_FILE_U; i++) {
		if(unique_files[i] != NULL)
			fclose(unique_files[i]);
	}

	for(i = 0; i < NUM_STAT_FILE_T; i++) {
		for(j = 0; j < n_cores; j++) {
			if(thread_files[i][j] != NULL)
				fclose(thread_files[i][j]);
		}
		rsfree(thread_files[i]);
	}

	rsfree(thread_stats);
	rsfree(lp_stats);
	rsfree(lp_stats_gvt);
}


void statistics_post_data_serial(enum stat_msg_t type, double data) {
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
}


void statistics_post_data(LID_t the_lid, enum stat_msg_t type, double data) {
	unsigned int lid = lid_to_int(the_lid);
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
			thread_stats[local_tid].idle_cycles++;
			break;

		case STAT_SILENT:
			lp_stats_gvt[lid].reprocessed_events += data;
			break;

		case STAT_GVT_ROUND_TIME:
			if(data < system_wide_stats.gvt_round_time_min)
				system_wide_stats.gvt_round_time_min = data;
			if(data > system_wide_stats.gvt_round_time_max)
				system_wide_stats.gvt_round_time_max = data;
			system_wide_stats.gvt_round_time += data;
			break;

		default:
			rootsim_error(true, "Wrong LP statistics post type: %d. Aborting...\n", type);
	}
}


double statistics_get_lp_data(LID_t lid, unsigned int type) {

	switch(type) {

		case STAT_GET_EVENT_TIME_LP:
			return lp_stats[lid_to_int(lid)].exponential_event_time;

		default:
			rootsim_error(true, "Wrong statistics get type: %d. Aborting...\n", type);
	}

	return 0.0;
}
