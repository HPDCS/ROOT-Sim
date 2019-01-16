/**
 * @file statistics/statistics.h
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

#pragma once

#include <scheduler/process.h>

/// This macro specified the default output directory, if nothing is passed as an option
#define DEFAULT_OUTPUT_DIR "outputs"

/// Longest length of a path
#define MAX_PATHLEN 512

enum stat_file_unique {
	STAT_FILE_U_NODE = 0,
	STAT_FILE_U_GLOBAL,
	NUM_STAT_FILE_U
};

/* Definition of statistics file names/indices for unique files */
#define STAT_FILE_NAME_NODE		"execution_stats"
#define STAT_FILE_NAME_GLOBAL		"global_execution_stats"

enum stat_file_per_thread {
	STAT_FILE_T_THREAD = 0,
	STAT_FILE_T_GVT,
	STAT_FILE_T_LP,
	NUM_STAT_FILE_T
};

/* Definition of statistics file names/indices for per-thread files */
#define STAT_FILE_NAME_THREAD	"local_stats"
#define STAT_FILE_NAME_GVT		"gvt"
#define STAT_FILE_NAME_LP		"lps"

/* Definition of LP Statistics Post Messages */
enum stat_msg_t {
	STAT_ANTIMESSAGE = 1001,
	STAT_EVENT,
	STAT_COMMITTED,
	STAT_ROLLBACK,
	STAT_CKPT,
	STAT_CKPT_TIME,
	STAT_CKPT_MEM,
	STAT_RECOVERY,
	STAT_RECOVERY_TIME,
	STAT_EVENT_TIME,
	STAT_IDLE_CYCLES,
	STAT_SILENT,
	STAT_GVT_ROUND_TIME,
	STAT_GET_SIMTIME_ADVANCEMENT,	//xxx totally unused
	STAT_GET_EVENT_TIME_LP
};

enum stats_levels {
	STATS_INVALID = 0,	/**< By convention 0 is the invalid field */
	STATS_GLOBAL,		/**< xxx documentation */
	STATS_PERF,		/**< xxx documentation */
	STATS_LP,		/**< xxx documentation */
	STATS_ALL		/**< xxx documentation */
};

// this is used in order to have more efficient stats additions during gvt reductions
typedef double vec_double __attribute__((vector_size(16 * sizeof(double))));

// Structure to keep track of (incremental) statistics
struct stat_t {
	union {
		struct {
			double tot_antimessages,
			    tot_events,
			    committed_events,
			    reprocessed_events,
			    tot_rollbacks,
			    tot_ckpts,
			    ckpt_time,
			    ckpt_mem,
			    tot_recoveries,
			    recovery_time,
			    event_time,
			    idle_cycles,
			    memory_usage,
			    simtime_advancement,
			    gvt_computations, exponential_event_time;
		};
		vec_double vec;
	};
	double gvt_time,
	    gvt_round_time,
	    gvt_round_time_min, gvt_round_time_max, max_resident_set;
};

extern void _mkdir(const char *path);

extern void print_config(void);

extern void statistics_init(void);
extern void statistics_fini(void);

extern void statistics_start(void);
extern void statistics_stop(int exit_code);

extern inline void statistics_on_gvt(double gvt);
extern inline void statistics_on_gvt_serial(double gvt);

extern inline void statistics_post_data(struct lp_struct *, enum stat_msg_t type, double data);
extern inline void statistics_post_data_serial(enum stat_msg_t type, double data);

extern double statistics_get_lp_data(struct lp_struct *, unsigned int type);

