/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file statistics.h
* @brief State Management subsystem's header
* @author Francesco Quaglia
* @author Roberto Vitali
*/

#pragma once
#ifndef _STATISTICS_H
#define _STATISTICS_H

#include <scheduler/process.h>

/// This macro pre-allocates space for statistics files.
#define NUM_FILES	3

/// This macro specified the default output directory, if nothing is passed as an option
#define DEFAULT_OUTPUT_DIR "outputs"

/// Longest length of a path
#define MAX_PATHLEN 512

/* Definition of Statistics file levels */
#define STAT_PER_THREAD	0
#define STAT_UNIQUE	1


/* Definition of statistics file names/indices for unique files */
#define GLOBAL_STAT_NAME "execution_stats"
#define GLOBAL_STAT	 0


/* Definition of statistics file names/indices for per-thread files */
#define THREAD_STAT_NAME "local_stats"
#define THREAD_STAT	 0
#define GVT_STAT_NAME	 "gvt"
#define GVT_STAT	 1
#define LP_STATS_NAME	 "lps"
#define LP_STATS	 2


/* Definition of LP Statistics Post Messages */
#define STAT_ANTIMESSAGE	1
#define STAT_EVENT		2
#define STAT_COMMITTED		3
#define STAT_ROLLBACK		4
#define STAT_CKPT		5
#define STAT_CKPT_TIME		6
#define STAT_CKPT_MEM		7
#define STAT_RECOVERY		8
#define STAT_RECOVERY_TIME	9
#define STAT_EVENT_TIME		10
#define STAT_IDLE_CYCLES	11
#define STAT_SILENT		12
#define STAT_ECS		13
#define STAT_ECS_FAULT		14
#define STAT_ECS_CLUSTERED		15
#define STAT_ECS_SCATTERED		16

/* Definition of Global Statistics Post Messages */
#define STAT_SIM_START		1001
#define STAT_GVT		1002
#define STAT_GVT_ROUND_TIME 1003


/* Definition of Thread Statistics Get Messages */
#define STAT_GET_SIMTIME_ADVANCEMENT	15001
#define STAT_GET_EVENT_TIME_LP		15002
#define STAT_GET_TOT_ECS			15003



enum stat_levels {STATS_GLOBAL, STATS_PERF, STATS_LP, STATS_ALL};




// Structure to keep track of (incremental) statistics
struct stat_t {
	double 	tot_antimessages,
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
		exponential_event_time,
		idle_cycles,
		memory_usage,
		gvt_computations,
		gvt_time,
		gvt_round_time_min,
		gvt_round_time_max,
		gvt_round_time,
		tot_ecs,
		ecs_page_faults,
		ecs_clustered_faults,
		ecs_scattered_faults,
		simtime_advancement;
};

extern void _mkdir(const char *path);
extern void statistics_init(void);
extern void statistics_fini(void);
extern void statistics_stop(int exit_code);
extern inline void statistics_post_lp_data(LID_t lid, unsigned int type, double data);
extern inline void statistics_post_other_data(unsigned int type, double data);
extern double statistics_get_lp_data(unsigned int type, LID_t lid);
extern double statistics_get_system_wide_data(unsigned int type);


#endif /* _STATISTICS_H */
