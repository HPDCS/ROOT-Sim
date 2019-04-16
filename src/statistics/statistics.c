/**
 * @file statistics/statistics.c
 *
 * @brief Statistics module
 *
 * All facitilies to collect, gather, and dump statistics are implemented
 * in this module. The statistics subsystem relies on the struct @ref stat_t
 * type to keep the relevant fields. Every statistic variable *must* be
 * a @c double, because the aggregation functions are type-agnostic and
 * consider every value to be a @c double. This allows to speedup some
 * aggregations by relying on vectorized instructions.
 *
 * There are two main entry points in this module:
 *
 * * statistics_post_data() can be called anywhere in the runtime library,
 *   allowing to specify a numerical code which identifies some statistic
 *   information. A value can be also passed, which is handled depending on
 *   the type of statistics managed. This function allows to update statistics
 *   values for each LP of the system.
 *
 * * statistics_get_lp_data() can be called to retrieve current per-LP
 *   statistical values. This is useful to implement autonomic policies
 *   which self-tune the behaviour of the runtime depending on, e.g.,
 *   workload factors.
 *
 * At the end of the simulation (or if the simulation is stopped), this
 * module implements a coordination protocol to reduce all values, both
 * at a machine level, and among distributed processes (using MPI).
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
 * @author Andrea Piccione
 * @author Alessandro Pellegrini
 * @author Tommaso Tocci
 * @author Roberto Vitali
 */

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
#include <mm/mm.h>
#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

#define GVT_BUFF_ROWS   50

struct _gvt_buffer {
	unsigned int pos;
	struct _gvt_buffer_row_t{
		double exec_time;
		double gvt;
		unsigned committed;
		unsigned cumulated;
	}rows[GVT_BUFF_ROWS];
};

/// This structure is used to buffer statistics gathered on GVT computation
static __thread struct _gvt_buffer gvt_buf = {0};

/// Pointers to the files used as binary buffer for the GVT statistics
static FILE **thread_blob_files = {0};
/// Pointers to unique files
static FILE *unique_files[NUM_STAT_FILE_U] = {0};
/// Pointers to per-thread files
static FILE **thread_files[NUM_STAT_FILE_T] = {0};

/// This is a timer that start during the initialization of statistics subsystem and can be used to know the total simulation time
static timer simulation_timer;

/// Keeps statistics on a per-LP basis
static struct stat_t *lp_stats;

/// Keeps statistics on a per-LP basis in a GVT phase
static struct stat_t *lp_stats_gvt;

/// Keeps statistics on a per-thread basis
static struct stat_t *thread_stats;

/// Keeps global statistics
static struct stat_t system_wide_stats = {.gvt_round_time_min = INFTY};

#ifdef HAVE_MPI
/// Keep statistics reduced globally across MPI ranks
struct stat_t global_stats = {.gvt_round_time_min = INFTY};
#endif

/**
 * This is a pseudo asprintf() implementation needed in order to stop GCC 8 from complaining
 *
 * @param ret_addr A char ** pointer where the function stores the
 *                 address of a large-enough string to hold the output
 * @param format The format string as in the real asprintf()
 * @param ... The arguments list as in the real asprintf()
 *
 * @return a malloc'd string containing the snprintf()-processed string
 *
 * @todo tranform into a function for safety.
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
static void _rmdir(const char *path)
{
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
void _mkdir(const char *path)
{
	char opath[MAX_PATHLEN];
	char *p;
	size_t len;

	strncpy(opath, path, sizeof(opath));
	opath[MAX_PATHLEN - 1] = '\0';
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

static void print_config_to_file(FILE *f)
{
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
		rootsim_config.set_seed);
}


void print_config(void)	{
	if(master_kernel())
		print_config_to_file(stdout);
}


/**
* This function registers when the actual simulation is starting.
*/
void statistics_start(void)
{
	timer_start(simulation_timer);
}


/**
 * Nicely print a size
 *
 * @param size The size in byte
 * @return A pointer to a per-thread buffer keeping the size.
 */
static char *format_size(double size)
{
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

#define HEADER_STR "------------------------------------------------------------"
static void print_header(FILE *f, const char *title)
{
	char buf[sizeof(HEADER_STR)] = HEADER_STR;
	unsigned title_len = strlen(title);
	assert(title_len < sizeof(HEADER_STR));
	memcpy(buf + ((sizeof(HEADER_STR) - title_len) / 2), title, title_len);
	fprintf(f, HEADER_STR "\n%s\n" HEADER_STR "\n\n", buf);
}


static void print_timer_stats(FILE *f, timer *start_timer, timer *stop_timer, double total_time)
{
	char timer_string[TIMER_BUFFER_LEN];
	timer_tostring(*start_timer, timer_string);
	fprintf(f, "SIMULATION STARTED AT ..... : %s \n", 		timer_string);
	timer_tostring(*stop_timer, timer_string);
	fprintf(f, "SIMULATION FINISHED AT .... : %s \n", 		timer_string);
	fprintf(f, "TOTAL SIMULATION TIME ..... : %.03f seconds \n\n",	total_time);
}


static void print_common_stats(FILE *f, struct stat_t *stats_p, bool want_thread_stats, bool want_local_stats)
{
	double rollback_frequency = (stats_p->tot_rollbacks / stats_p->tot_events);
	double rollback_length = (stats_p->tot_rollbacks > 0 ? (stats_p->tot_events - stats_p->committed_events) / stats_p->tot_rollbacks : 0);
	double efficiency = (1 - rollback_frequency * rollback_length) * 100;

	fprintf(f, "TOTAL KERNELS ............. : %d \n",		n_ker);
	if(want_local_stats) {
		fprintf(f, "KERNEL ID ................. : %d \n",	kid);
		fprintf(f, "LPs HOSTED BY KERNEL....... : %d \n",	n_prc);
		fprintf(f, "TOTAL_THREADS ............. : %d \n",	n_cores);
	} else {
		fprintf(f, "TOTAL_THREADS ............. : %d \n",	n_cores * n_ker);
		fprintf(f, "TOTAL LPs.................. : %d \n",	n_prc_tot);
	}
	if(want_thread_stats) {
		fprintf(f, "THREAD ID ................. : %d \n",	local_tid);
		fprintf(f, "LPs HOSTED BY THREAD ...... : %d \n",	n_prc_per_thread);
	}
	fprintf(f, "\n");
	fprintf(f, "TOTAL EXECUTED EVENTS ..... : %.0f \n",		stats_p->tot_events);
	fprintf(f, "TOTAL COMMITTED EVENTS..... : %.0f \n",		stats_p->committed_events);
	fprintf(f, "TOTAL REPROCESSED EVENTS... : %.0f \n",		stats_p->reprocessed_events);
	fprintf(f, "TOTAL ROLLBACKS EXECUTED... : %.0f \n",		stats_p->tot_rollbacks);
	fprintf(f, "TOTAL ANTIMESSAGES......... : %.0f \n",		stats_p->tot_antimessages);
	fprintf(f, "ROLLBACK FREQUENCY......... : %.2f %%\n",		rollback_frequency * 100);
	fprintf(f, "ROLLBACK LENGTH............ : %.2f events\n",	rollback_length);
	fprintf(f, "EFFICIENCY................. : %.2f %%\n",		efficiency);
	fprintf(f, "AVERAGE EVENT COST......... : %.2f us\n",		stats_p->event_time / stats_p->tot_events);
	fprintf(f, "AVERAGE EVENT COST (EMA)... : %.2f us\n",		stats_p->exponential_event_time);
	fprintf(f, "AVERAGE CHECKPOINT COST.... : %.2f us\n",		stats_p->ckpt_time / stats_p->tot_ckpts);
	fprintf(f, "AVERAGE RECOVERY COST...... : %.2f us\n",		(stats_p->tot_recoveries > 0 ? stats_p->recovery_time / stats_p->tot_recoveries : 0));
	fprintf(f, "AVERAGE LOG SIZE........... : %s\n",		format_size(stats_p->ckpt_mem / stats_p->tot_ckpts));
	fprintf(f, "\n");
	fprintf(f, "IDLE CYCLES................ : %.0f\n",		stats_p->idle_cycles);
	if(!want_thread_stats){
		fprintf(f, "LAST COMMITTED GVT ........ : %f\n",	get_last_gvt());
	}
	fprintf(f, "NUMBER OF GVT REDUCTIONS... : %.0f\n",		stats_p->gvt_computations);
	if(!want_thread_stats && n_ker > 1){
		fprintf(f, "MIN GVT ROUND TIME......... : %.2f us\n",	stats_p->gvt_round_time_min);
		fprintf(f, "MAX GVT ROUND TIME......... : %.2f us\n",	stats_p->gvt_round_time_max);
		fprintf(f, "AVERAGE GVT ROUND TIME..... : %.2f us\n",	stats_p->gvt_round_time / stats_p->gvt_computations);
	}
	fprintf(f, "SIMULATION TIME SPEED...... : %.2f units per GVT\n",stats_p->simtime_advancement);
	fprintf(f, "AVERAGE MEMORY USAGE....... : %s\n",		format_size(stats_p->memory_usage / stats_p->gvt_computations));
	if(!want_thread_stats)
		fprintf(f, "PEAK MEMORY USAGE.......... : %s\n",	format_size(stats_p->max_resident_set));
}


static void print_termination_status(FILE *f, int exit_code)
{
	switch(exit_code){
		case EXIT_SUCCESS:
			fprintf(f, "\n--------- SIMULATION CORRECTLY COMPLETED ----------\n");
			break;
		case EXIT_FAILURE:
		default:
			fprintf(f, "\n--------- SIMULATION ABNORMALLY TERMINATED ----------\n");
	}
}

void print_gvt_stats_file(void)
{
	size_t elems, i;
	FILE *f_blob = thread_blob_files[local_tid];
	FILE *f_final = thread_files[STAT_FILE_T_GVT][local_tid];
	// last flush of our buffer in the blob file
	fwrite(gvt_buf.rows, sizeof(struct _gvt_buffer_row_t), gvt_buf.pos, f_blob);
	// print the header
	fprintf(f_final, "#%15.15s    %15.15s    ", "\"WCT\"", "\"GVT VALUE\"");
	fprintf(f_final, "%15.15s    %15.15s\n", "\"EVENTS\"", "\"CUMUL EVENTS\"");
	// prepare to read back the blob
	rewind(f_blob);
	do {
		// read the blob file back to the buffer piece to piece
		elems = fread(gvt_buf.rows, sizeof(struct _gvt_buffer_row_t), GVT_BUFF_ROWS, f_blob);
		for(i = 0; i < elems; ++i) {
			// write line per line
			fprintf(f_final, " %15lf    %15lf    ", gvt_buf.rows[i].exec_time, gvt_buf.rows[i].gvt);
			fprintf(f_final, "%15u    %15u\n", gvt_buf.rows[i].committed, gvt_buf.rows[i].cumulated);
		}
	} while(!feof(f_blob));
	fflush(f_final);
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
void statistics_stop(int exit_code)
{
	register unsigned int i;
	FILE *f;
	double total_time;
	timer simulation_finished;

	if(rootsim_config.serial) {
		f = unique_files[STAT_FILE_U_GLOBAL];

		// Stop timers
		timer_start(simulation_finished);
		total_time = timer_value_seconds(simulation_timer);

		print_header(f, "SERIAL STATISTICS");
		print_timer_stats(f, &simulation_timer, &simulation_finished, total_time);
		fprintf(f, "TOTAL LPs.................. : %d \n", 		n_prc_tot);
		fprintf(f, "TOTAL EXECUTED EVENTS ..... : %.0f \n", 		system_wide_stats.tot_events);
		fprintf(f, "AVERAGE EVENT COST......... : %.3f us\n",		total_time / system_wide_stats.tot_events * 1000 * 1000);
		fprintf(f, "AVERAGE EVENT COST (EMA)... : %.2f us\n",		system_wide_stats.exponential_event_time);
		fprintf(f, "\n");
		fprintf(f, "LAST COMMITTED GVT ........ : %f\n",		system_wide_stats.gvt_time);
		fprintf(f, "SIMULATION TIME SPEED...... : %.2f units per GVT\n",system_wide_stats.simtime_advancement);
		fprintf(f, "AVERAGE MEMORY USAGE....... : %s\n",		format_size(system_wide_stats.memory_usage / system_wide_stats.gvt_computations));
		fprintf(f, "PEAK MEMORY USAGE.......... : %s\n",		format_size(getPeakRSS()));
		print_termination_status(f, exit_code);
		fflush(f);

		print_termination_status(stdout, exit_code);

	} else { /* Parallel simulation */

		// Stop the simulation timer immediately to avoid considering the statistics reduction time
		if (master_thread()) {
			timer_start(simulation_finished);
			total_time = timer_value_seconds(simulation_timer);
		}

		if((rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF))
			print_gvt_stats_file();

		/* dump per-LP statistics if required. They are already reduced during the GVT phase */
		if(rootsim_config.stats == STATS_LP || rootsim_config.stats == STATS_ALL) {
			f = thread_files[STAT_FILE_T_LP][local_tid];

			fprintf(f, "#%15.15s   %15.15s   %15.15s   %15.15s", "\"GID\"", "\"LID\"", "\"TOTAL EVENTS\"", "\"COMM EVENTS\"");
			fprintf(f, "   %15.15s   %15.15s   %15.15s   %15.15s", "\"REPROC EVENTS\"", "\"ROLLBACKS\"", "\"ANTIMSG\"", "\"AVG EVT COST\"");
			fprintf(f, "   %15.15s   %15.15s   %15.15s\n", "\"AVG CKPT COST\"", "\"AVG REC COST\"", "\"IDLE CYCLES\"");

			foreach_bound_lp(lp) {
				unsigned int lp_id = lp->lid.to_int;

				fprintf(f, " %15u   ", 		lp->gid.to_int);
				fprintf(f, "%15u   ", 		lp_id);
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].tot_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].committed_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].reprocessed_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].tot_rollbacks);
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].tot_antimessages);
				fprintf(f, "%15lf   ", 		lp_stats[lp_id].event_time / lp_stats[lp_id].tot_events);
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].ckpt_time / lp_stats[lp_id].tot_ckpts);
				fprintf(f, "%15.0lf   ", 	(lp_stats[lp_id].tot_rollbacks > 0 ? lp_stats[lp_id].recovery_time / lp_stats[lp_id].tot_recoveries : 0));
				fprintf(f, "%15.0lf   ", 	lp_stats[lp_id].idle_cycles);
				fprintf(f, "\n");
			}
		}

		/* Reduce and dump per-thread statistics */

		// Sum up all LPs statistics
		foreach_bound_lp(lp) {
			thread_stats[local_tid].vec += lp_stats[lp->lid.to_int].vec;
		}
		thread_stats[local_tid].exponential_event_time /= n_prc_per_thread;

		// Compute derived statistics and dump everything
		f = thread_files[STAT_FILE_T_THREAD][local_tid];
		print_header(f, "THREAD STATISTICS");
		print_common_stats(f, &thread_stats[local_tid], true, true);
		print_termination_status(f, exit_code);
		fflush(f);

		thread_barrier(&all_thread_barrier);

		if(master_thread()) {
			// Sum up all threads statistics
			for(i = 0; i < n_cores; i++) {
				system_wide_stats.vec += thread_stats[i].vec;
			}
			system_wide_stats.exponential_event_time /= n_cores;
			system_wide_stats.max_resident_set = getPeakRSS();
			// GVT computations are the same for all threads
			system_wide_stats.gvt_computations /= n_cores;

			// Compute derived statistics and dump everything
			f = unique_files[STAT_FILE_U_NODE];
			print_config_to_file(f);
			fprintf(f, "\n");
			print_header(f, "NODE STATISTICS");
			print_timer_stats(f, &simulation_timer, &simulation_finished, total_time);
			print_common_stats(f, &system_wide_stats, false, true);
			print_termination_status(f, exit_code);
			fflush(f);

			#ifdef HAVE_MPI
			mpi_reduce_statistics(&global_stats, &system_wide_stats);
			if(master_kernel() && n_ker > 1){
				global_stats.exponential_event_time /= n_ker;
				// GVT computations are the same for all kernels
				global_stats.gvt_computations /= n_ker;
				global_stats.simtime_advancement /= n_ker;

				f = unique_files[STAT_FILE_U_GLOBAL];
				print_config_to_file(f);
				fprintf(f, "\n");
				print_header(f, "GLOBAL STATISTICS");
				print_timer_stats(f, &simulation_timer, &simulation_finished, total_time);
				print_common_stats(f, &global_stats, false, false);
				print_termination_status(f, exit_code);
				fflush(f);
			}
			#endif
			if(master_kernel())
				print_termination_status(stdout, exit_code);
		}
	}
}


// Sum up all that happened in the last GVT phase, in case it is required,
// dump a line on the corresponding statistics file
inline void statistics_on_gvt(double gvt)
{
	unsigned int lid;
	unsigned int committed = 0;
	static __thread unsigned int cumulated = 0;
	double exec_time, simtime_advancement, keep_exponential_event_time;

	// Dump on file only if required
	if(rootsim_config.stats == STATS_ALL || rootsim_config.stats == STATS_PERF) {
		exec_time = timer_value_seconds(simulation_timer);

		// Reduce the committed events from all LPs
		foreach_bound_lp(lp) {
			committed += lp_stats_gvt[lp->lid.to_int].committed_events;
		}
		cumulated += committed;

		// fill the row
		gvt_buf.rows[gvt_buf.pos++] = (struct _gvt_buffer_row_t){exec_time, gvt, committed, cumulated};
		// check if buffer is full
		if(gvt_buf.pos >= GVT_BUFF_ROWS){
			// flush our buffer in the blob file
			fwrite(gvt_buf.rows, sizeof(struct _gvt_buffer_row_t), gvt_buf.pos, thread_blob_files[local_tid]);
			gvt_buf.pos = 0;
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

	foreach_bound_lp(lp) {
		lid = lp->lid.to_int;

		lp_stats[lid].vec += lp_stats_gvt[lid].vec;

		lp_stats[lid].exponential_event_time = lp_stats_gvt[lid].exponential_event_time;

		keep_exponential_event_time = lp_stats_gvt[lid].exponential_event_time;
		bzero(&lp_stats_gvt[lid], sizeof(struct stat_t));
		lp_stats_gvt[lid].exponential_event_time = keep_exponential_event_time;
	}
}


inline void statistics_on_gvt_serial(double gvt)
{
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
	do {\
		safe_asprintf(&name_buf, "%s/" format, rootsim_config.output_dir, ##__VA_ARGS__);\
		if (((destination) = fopen(name_buf, "w+")) == NULL)\
			rootsim_error(true, "Cannot open %s\n", name_buf);\
		rsfree(name_buf);\
	} while(0)

/**
* This function initializes the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_init(void)
{
	unsigned int i;
	char *name_buf = NULL;

	// Make output dir if non existant
	_mkdir(rootsim_config.output_dir);

	// The whole reduction for the sequential simulation is simply done at the end
	if(rootsim_config.serial){
		assign_new_file(unique_files[STAT_FILE_U_GLOBAL], "sequential_stats");
		return;
	}

	for(i = 0; i < n_cores; i++) {
		safe_asprintf(&name_buf, "%s/thread_%u_%u/", rootsim_config.output_dir, kid, i);
		_mkdir(name_buf);
		rsfree(name_buf);
	}
	// create the unique file
#ifdef HAVE_MPI
	if(n_ker > 1) {
		assign_new_file(unique_files[STAT_FILE_U_NODE], STAT_FILE_NAME_NODE"_%u", kid);
		if(master_kernel())
			assign_new_file(unique_files[STAT_FILE_U_GLOBAL], STAT_FILE_NAME_GLOBAL);
	} else
#endif
	{
		assign_new_file(unique_files[STAT_FILE_U_NODE], STAT_FILE_NAME_NODE);
	}
	// Create files depending on the actual level of verbosity
	switch(rootsim_config.stats) {
		case STATS_ALL:
		case STATS_LP:
			thread_files[STAT_FILE_T_LP] = rsalloc(sizeof(FILE *) * n_cores);
			for(i = 0; i < n_cores; ++i) {
				assign_new_file(thread_files[STAT_FILE_T_LP][i], "thread_%u_%u/%s", kid, i, STAT_FILE_NAME_LP);
			}
			if(rootsim_config.stats == STATS_LP) goto stats_lp_jump;
			/* fall through */
		case STATS_PERF:
			thread_files[STAT_FILE_T_GVT] = rsalloc(sizeof(FILE *) * n_cores);
			thread_blob_files = rsalloc(sizeof(FILE *) * n_cores);
			for(i = 0; i < n_cores; ++i) {
				assign_new_file(thread_files[STAT_FILE_T_GVT][i], "thread_%u_%u/%s", kid, i, STAT_FILE_NAME_GVT);
				assign_new_file(thread_blob_files[i], "thread_%u_%u/.%s_blob", kid, i, STAT_FILE_NAME_GVT);
			}
			/* fall through */
		case STATS_GLOBAL:
			stats_lp_jump:
			thread_files[STAT_FILE_T_THREAD] = rsalloc(sizeof(FILE *) * n_cores);
			for(i = 0; i < n_cores; ++i) {
				assign_new_file(thread_files[STAT_FILE_T_THREAD][i], "thread_%u_%u/%s", kid, i, STAT_FILE_NAME_THREAD);
			}
		break;
		default:
			rootsim_error(true, "unrecognized statistics option '%d'!", rootsim_config.stats);
	}

	// Initialize data structures to keep information
	lp_stats = rsalloc(n_prc * sizeof(struct stat_t));
	bzero(lp_stats, n_prc * sizeof(struct stat_t));
	lp_stats_gvt = rsalloc(n_prc * sizeof(struct stat_t));
	bzero(lp_stats_gvt, n_prc * sizeof(struct stat_t));
	thread_stats = rsalloc(n_cores * sizeof(struct stat_t));
	bzero(thread_stats, n_cores * sizeof(struct stat_t));
}

#undef assign_new_file


/**
* This function finalize the Statistics subsystem
*
* @author Alessandro Pellegrini
*/
void statistics_fini(void)
{
	register unsigned int i, j;

	for(i = 0; i < NUM_STAT_FILE_U; i++) {
		if(unique_files[i] != NULL)
			fclose(unique_files[i]);
	}

	for(i = 0; i < NUM_STAT_FILE_T; i++) {
		if(!thread_files[i]) continue;
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


void statistics_post_data_serial(enum stat_msg_t type, double data)
{
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


void statistics_post_data(struct lp_struct *lp, enum stat_msg_t type, double data)
{
	// TODO: this is only required to avoid a nasty segfault if we
	// pass NULL to lp, as we do for the case of STAT_IDLE_CYCLES.
	// We should move stats arrays in struct lp_struct to make the
	// whole code less ugly.
	unsigned int lid = lp ? lp->lid.to_int : UINT_MAX;

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
			system_wide_stats.gvt_round_time_min = fmin(data, system_wide_stats.gvt_round_time_min);
			system_wide_stats.gvt_round_time_max = fmax(data, system_wide_stats.gvt_round_time_max);
			system_wide_stats.gvt_round_time += data;
			break;

		default:
			rootsim_error(true, "Wrong LP statistics post type: %d. Aborting...\n", type);
	}
}


double statistics_get_lp_data(struct lp_struct *lp, unsigned int type)
{
	switch(type) {

		case STAT_GET_EVENT_TIME_LP:
			return lp_stats[lp->lid.to_int].exponential_event_time;

		default:
			rootsim_error(true, "Wrong statistics get type: %d. Aborting...\n", type);
	}
	return 0.0;
}
