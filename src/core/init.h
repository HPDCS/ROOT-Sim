/**
* @file core/init.h
*
* @brief Initialization routines
*
* This module implements the simulator initialization routines
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
* @author Francesco Quaglia
* @author Andrea Piccione
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/
#pragma once

#include <statistics/statistics.h>

/*! \public
 * These are used to index the first level of the param_to_text array.
 */
enum param_codes {
	PARAM_SCHEDULER = 0,
	PARAM_CKTRM_MODE,
	PARAM_LPS_DISTRIBUTION,
	PARAM_VERBOSE,
	PARAM_STATS,
	PARAM_STATE_SAVING,
	PARAM_SNAPSHOT,
};

/*!
 * This array contains the stringified version of the parameter values ROOT-Sim accepts.
 * You can index the first level of the array using the enum param_codes while
 * for the second level you have to refer to the enumerations
 * listed in relevant modules headers.
 */
extern const char *const param_to_text[][5];

/// Configuration of the execution of the simulator
typedef struct _simulation_configuration {
	char *output_dir;		///< Destination Directory of output files
	int scheduler;			///< Which scheduler to be used
	int gvt_time_period;		///< Wall-Clock time to wait before executiong GVT operations
	int gvt_snapshot_cycles;	///< GVT operations to be executed before rebuilding the state
	int simulation_time;		///< Wall-clock-time based termination predicate
	int lps_distribution;		///< Policy for the LP to Kernel mapping
	int ckpt_mode;			///< Type of checkpointing mode (Synchronous, Semi-Asyncronous, ...)
	int checkpointing;		///< Type of checkpointing scheme (e.g., PSS, CSS, ...)
	int ckpt_period;		///< Number of events to execute before taking a snapshot in PSS (ignored otherwise)
	int snapshot;			///< Type of snapshot (e.g., full, incremental, autonomic, ...)
	int check_termination_mode;	///< Check termination strategy: standard or incremental
	bool deterministic_seed;	///< Does not change the seed value config file that will be read during the next runs
	int verbose;			///< Kernel verbose
	enum stats_levels stats;	///< Produce performance statistic file (default STATS_ALL)
	bool serial;			///< If the simulation must be run serially
	seed_type set_seed;		///< The master seed to be used in this run
	bool core_binding;		///< Bind threads to specific core (reduce context switches and cache misses)

#ifdef HAVE_PREEMPTION
	bool disable_preemption;	///< If compiled for preemptive Time Warp, it can be disabled at runtime
#endif

} simulation_configuration;

extern simulation_configuration rootsim_config;

extern void SystemInit(int argc, char **argv);
