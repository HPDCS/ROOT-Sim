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
* @file init.c
* @brief This module implements the simulator initialization routines
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <sysexits.h>
#include <argp.h>
#include <errno.h>

#include <ROOT-Sim.h>
#include <arch/os.h>
#include <arch/thread.h>
#include <communication/communication.h>
#include <core/core.h>
#include <core/init.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <statistics/statistics.h>
#include <lib/numerical.h>
#include <serial/serial.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

/// This is the list of mnemonics for arguments
enum _opt_codes{
 OPT_FIRST = 127, /// this is used as an offset to the enum values, so that argp doesn't assign short options

 OPT_NP,
 OPT_NPRC,
 OPT_OUTPUT_DIR,
 OPT_SCHEDULER,
 OPT_NPWD,
 OPT_P,
 OPT_FULL,
 OPT_INC,
 OPT_A,
 OPT_GVT,
 OPT_CKTRM_MODE,
 OPT_BLOCKING_GVT,
 OPT_GVT_SNAPSHOT_CYCLES,
 OPT_SIMULATION_TIME,
 OPT_LPS_DISTRIBUTION,
 OPT_DETERMINISTIC_SEED,
 OPT_VERBOSE,
 OPT_STATS,
 OPT_SEED,
 OPT_SERIAL,
 OPT_NO_CORE_BINDING,

#ifdef HAVE_PREEMPTION
 OPT_PREEMPTION,
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
 OPT_ALLOCATOR,
#endif
};

const char *argp_program_version 		= PACKAGE_STRING;
const char *argp_program_bug_address 	= PACKAGE_BUGREPORT;
// TODO!!!!!
static char doc[] = "todo";
// TODO!!!!!
static char args_doc[] = "todo";

static const struct argp_option argp_options[] = {
	{"np", 			OPT_NP,		"VALUE", 	0, "Number of total cores being used by the simulation", 0},
	{"nprc", 		OPT_NPRC, 	"VALUE", 	0, "Total number of Logical Processes being lunched at simulation startup", 0},
	{"output-dir",	OPT_OUTPUT_DIR, "PATH", 0, "Path to a folder where execution statistics are stored. If not present, it is created", 0},
	{"scheduler",	OPT_SCHEDULER, "TYPE", 	0, "LP Scheduling algorithm. Supported values are: stf", 0},
	{"npwd",	  	OPT_NPWD,	0,			0, "Non Piece-Wise-Deterministic simulation model. See manpage for accurate description", 0},
	{"p",			OPT_P,		"VALUE",	0, "Checkpointing interval", 0},
	{"full",		OPT_FULL,	0,			0, "Take only full logs", 0},
	{"inc",			OPT_INC,	0,			0, "Take only incremental logs (still to be released)", 0},
	{"A",		  	OPT_A,		0,			0, "Autonomic subsystem: set checkpointing interval and log mode automatically at runtime (still to be released)", 0},
	{"gvt",			OPT_GVT,	"VALUE",	0, "Time between two GVT reductions (in milliseconds)", 0},
	{"cktrm-mode", 	OPT_CKTRM_MODE, "TYPE",	0, "Termination Detection mode. Supported values: standard, incremental", 0},
	{"blocking-gvt", 		OPT_BLOCKING_GVT, 		0,		0, 	"Blocking GVT. All distributed nodes block until a consensus is agreed", 0},
	{"gvt-snapshot-cycles",	OPT_GVT_SNAPSHOT_CYCLES, "VALUE", 0, "Termination detection is invoked after this number of GVT reductions", 0},
	{"simulation-time", 	OPT_SIMULATION_TIME, 	"VALUE", 0,	"Halt the simulation when all LPs reach this logical time. 0 means infinite", 0},
	{"lps-distribution", 	OPT_LPS_DISTRIBUTION, 	"TYPE", 0, 	"LPs distributions over simulation kernels policies. Supported values: block, circular", 0},
	{"deterministic-seed",	OPT_DETERMINISTIC_SEED, 0, 0, 		"Do not change the initial random seed for LPs. Enforces different deterministic simulation runs", 0},
	{"verbose",		OPT_VERBOSE, "TYPE",	0, "Verbose execution", 0},
	{"stats",		OPT_STATS, 	"TYPE",		0, "Level of detail in the output statistics", 0},
	{"seed",		OPT_SEED,	"VALUE",	0, "Manually specify the initial random seed", 0},
	{"serial",		OPT_SERIAL,	0,			0, "Run a serial simulation (using Calendar Queues)", 0},
	{"sequential",	OPT_SERIAL,	0,			OPTION_ALIAS, NULL, 0},
	{"no-core-binding", 	OPT_NO_CORE_BINDING, 	0,		0, 	"Disable the binding of threads to specific physical processing cores", 0},

	#ifdef HAVE_PREEMPTION
	{"no-preemption", OPT_PREEMPTION, 0,	0, "Disable Preemptive Time Warp", 0},
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
	{"no-allocator", OPT_ALLOCATOR, 0,		0, "Disable parallel allocator", 0},
#endif

	{0}
};

#define malformed_option_failure() rootsim_error(true, "invalid value \"%s\" in --%s option", arg, argp_options[key].arg)

#define conflicting_option_failure(msg) argp_failure(state, EXIT_FAILURE, EINVAL, "some options are conflicting: " msg "\nAborting");

// the compound expression equivalent to __value >= low is needed in order to suppress a warning when low == 0
#define parse_ullong_limits(low, high) ({\
					unsigned long long int __value;\
					char *__endptr;\
					__value = strtoull(arg, &__endptr, 10);\
					if(!(*arg != '\0' && *__endptr == '\0' && (__value > low || __value == low) && __value <= high)) {\
						malformed_option_failure();\
					}\
					__value;\
				     })

static error_t parse_opt (int key, char *arg, struct argp_state *state){

	switch (key) {
		// TODO since we check for some conflicting options we may as well
		// restrict the user from entering twice the same option.
		// For example, right now, executing ./model --nprc 6--np 3 --np 4 leads to a run
		// with option np set to the latest value inserted (in this case 4) instead of throwing an error.
		case OPT_NP:
			n_cores = parse_ullong_limits(1, UINT_MAX);
			break;

		case OPT_NPRC:
			n_prc_tot = parse_ullong_limits(1, UINT_MAX);
			break;

		case OPT_OUTPUT_DIR:
			rootsim_config.output_dir = arg;
			break;

		case OPT_SCHEDULER:
			if(strcmp(arg, "stf") == 0) {
				rootsim_config.scheduler = SMALLEST_TIMESTAMP_FIRST;
			} else {
				malformed_option_failure();
			}
			break;

		case OPT_NPWD:
			if (rootsim_config.checkpointing == INVALID_STATE_SAVING) {
				rootsim_config.checkpointing = COPY_STATE_SAVING;
			} else {
				conflicting_option_failure("I'm requested to run non piece-wise deterministically, but a checkpointing interval is set already.");
			}
			break;

		case OPT_P:
			if(rootsim_config.checkpointing == COPY_STATE_SAVING) {
				conflicting_option_failure("Copy State Saving is selected, but I'm requested to set a checkpointing interval.");
			} else {
				rootsim_config.checkpointing = PERIODIC_STATE_SAVING;
				rootsim_config.ckpt_period = parse_ullong_limits(1, 40);
				// This is a micro optimization that makes the LogState function to avoid checking the checkpointing interval and keeping track of the logs taken
				if(rootsim_config.ckpt_period == 1)
					rootsim_config.checkpointing = COPY_STATE_SAVING;
			}
			break;

		case OPT_FULL:
			if (rootsim_config.snapshot == INVALID_SNAPSHOT) {
				rootsim_config.snapshot = FULL_SNAPSHOT;
			} else {
				conflicting_option_failure("a state saving option has already been set.");
			}
			break;

		case OPT_INC:
			argp_failure(state, EXIT_FAILURE, ENOSYS, "incremental state saving is not supported in stable version yet...\nAborting");
			break;

		case OPT_A:
			argp_failure(state, EXIT_FAILURE, ENOSYS, "autonomic state saving is not supported in stable version yet...\nAborting");
			break;

		case OPT_GVT:
			rootsim_config.gvt_time_period = parse_ullong_limits(1, INT_MAX);
			break;

		case OPT_CKTRM_MODE:
			if(strcmp(arg, "standard") == 0) {
				rootsim_config.check_termination_mode = NORM_CKTRM;
			} else if(strcmp(arg, "incremental") == 0) {
				rootsim_config.check_termination_mode = INCR_CKTRM;
			} else {
				malformed_option_failure();
			}
			break;

		case OPT_BLOCKING_GVT:
			rootsim_config.blocking_gvt = true;
			break;

		case OPT_GVT_SNAPSHOT_CYCLES:
			rootsim_config.gvt_snapshot_cycles = parse_ullong_limits(1, INT_MAX);
			break;

		case OPT_SIMULATION_TIME:
			rootsim_config.simulation_time = parse_ullong_limits(0, INT_MAX);
			break;

		case OPT_LPS_DISTRIBUTION:
			if(strcmp(arg, "block") == 0) {
				rootsim_config.lps_distribution = LP_DISTRIBUTION_BLOCK;
			} else if(strcmp(arg, "circular") == 0) {
				rootsim_config.lps_distribution = LP_DISTRIBUTION_CIRCULAR;
			} else {
				malformed_option_failure();
			}
			break;

		case OPT_DETERMINISTIC_SEED:
			rootsim_config.deterministic_seed = true;
			break;

		case OPT_VERBOSE:
			if(strcmp(arg, "info") == 0) {
				rootsim_config.verbose = VERBOSE_INFO;
			} else if(strcmp(arg, "debug") == 0) {
				rootsim_config.verbose = VERBOSE_DEBUG;
			} else if(strcmp(arg, "no") == 0) {
				rootsim_config.verbose = VERBOSE_NO;
			} else {
				malformed_option_failure();
			}
			break;

		case OPT_STATS:
			if(strcmp(arg, "all") == 0) {
				rootsim_config.stats = STATS_ALL;
			} else if(strcmp(arg, "performance") == 0) {
				rootsim_config.stats = STATS_PERF;
			} else if(strcmp(arg, "lp") == 0) {
				rootsim_config.stats = STATS_LP;
			} else if(strcmp(arg, "global") == 0) {
				rootsim_config.stats = STATS_GLOBAL;
			} else {
				malformed_option_failure();
			}
			break;

		case OPT_SEED:
			rootsim_config.set_seed = parse_ullong_limits(0, UINT64_MAX);
			break;

		case OPT_SERIAL:
			rootsim_config.serial = true;
			break;

		case OPT_NO_CORE_BINDING:
			rootsim_config.core_binding = false;
			break;

		#ifdef HAVE_PREEMPTION
		case OPT_PREEMPTION:
			rootsim_config.disable_preemption = true;
			break;
		#endif

		#ifdef HAVE_PARALLEL_ALLOCATOR
		case OPT_ALLOCATOR:
			rootsim_config.disable_allocator = true;
			break;
		#endif

		case ARGP_KEY_INIT:
			// Store the predefined values, before reading any overriding one
			rootsim_config.output_dir = DEFAULT_OUTPUT_DIR;
			rootsim_config.gvt_time_period = 1000;
			rootsim_config.scheduler = SMALLEST_TIMESTAMP_FIRST;
			rootsim_config.checkpointing = INVALID_STATE_SAVING;
			rootsim_config.ckpt_period = 10;
			rootsim_config.gvt_snapshot_cycles = 2;
			rootsim_config.simulation_time = 0;
			rootsim_config.lps_distribution = LP_DISTRIBUTION_BLOCK;
			rootsim_config.check_termination_mode = NORM_CKTRM;
			rootsim_config.blocking_gvt = false;
			rootsim_config.snapshot = INVALID_SNAPSHOT;
			rootsim_config.deterministic_seed = false;
			rootsim_config.set_seed = 0;
			rootsim_config.verbose = VERBOSE_INFO;
			rootsim_config.stats = STATS_ALL;
			rootsim_config.serial = false;
			rootsim_config.core_binding = true;

			#ifdef HAVE_PREEMPTION
			rootsim_config.disable_preemption = false;
			#endif

			#ifdef HAVE_PARALLEL_ALLOCATOR
			rootsim_config.disable_allocator = false;
			#endif

			break;

		case ARGP_KEY_SUCCESS:

			// sanity checks
			if(n_cores > get_cores())
				rootsim_error(true, "Demanding %u cores, which are more than available (%d)\n", n_cores, get_cores());

			if(n_cores > MAX_THREADS_PER_KERNEL)
				rootsim_error(true, "Too many threads, maximum supported number is %u\n", MAX_THREADS_PER_KERNEL);

			if(n_prc_tot > MAX_LPs)
				rootsim_error(true, "Too many LPs, maximum supported number is %u\n", MAX_LPs);

			if(!rootsim_config.serial && n_cores == 0)
				rootsim_error(true, "Number of cores was not provided \"--np\"\n");

			if(n_prc_tot == 0)
				rootsim_error(true, "Number of LPs was not provided \"--nprc\"\n");

			if(!rootsim_config.serial && n_prc_tot < n_cores)
				rootsim_error(true, "Requested a simulation run with %u LPs and %u worker threads: the mapping is not possible\n", n_prc_tot, n_cores);

			// setting default options
			if (!rootsim_config.serial && rootsim_config.snapshot == INVALID_SNAPSHOT)
				rootsim_config.snapshot = FULL_SNAPSHOT; // TODO: in the future, default to AUTONOMIC_

			if (!rootsim_config.serial && rootsim_config.checkpointing == INVALID_STATE_SAVING)
				rootsim_config.checkpointing = PERIODIC_STATE_SAVING;

			break;
			/* these functionalities are not needed
		case ARGP_KEY_ARGS:
		case ARGP_KEY_NO_ARGS:
		case ARGP_KEY_SUCCESS:
		case ARGP_KEY_END:
		case ARGP_KEY_ARG:
		case ARGP_KEY_ERROR:
			*/
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef parse_long_limits
#undef conflicting_option_failure
#undef malformed_option_failure

static struct argp_child argp_child[2] = {
		{0, 0, "Model specific options", 1},
		{0}
};

static struct argp argp = { argp_options, parse_opt, args_doc, doc, argp_child, 0, 0 };

/**
* This function initializes the simulator
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param argc number of parameters passed at command line
* @param argv array of parameters passed at command line
*/
void SystemInit(int argc, char **argv) {

	#ifdef HAVE_MPI
	mpi_init(&argc, &argv);

	if(n_ker > MAX_KERNELS){
		rootsim_error(true, "Too many kernels, maximum supported number is %u\n", MAX_KERNELS);
	}
	#else
	n_ker = 1;
	#endif

	// Early initialization of ECS subsystem if needed
	#ifdef HAVE_CROSS_STATE
	ecs_init();
	#endif

	// this retrieves the model's argp parser if declared by the developer
	argp_child[0].argp = &model_argp;

	argp_parse (&argp, argc, argv, 0, NULL, NULL);

	// If we're going to run a serial simulation, configure the simulation to support it
	if(rootsim_config.serial) {
		SetState = SerialSetState;
		ScheduleNewEvent = SerialScheduleNewEvent;
		numerical_init();
		//dymelor_init();
		statistics_init();
		serial_init();
		return;
	} else {
		SetState = ParallelSetState;
		ScheduleNewEvent = ParallelScheduleNewEvent;
	}


	if (master_kernel()) {

		 printf("****************************\n"
			"*  ROOT-Sim Configuration  *\n"
			"****************************\n"
			"Kernels: %u\n"
			"Cores: %ld available, %d used\n"
			"Number of Logical Processes: %u\n"
			"Output Statistics Directory: %s\n"
			"Scheduler: %d\n"
			#ifdef HAVE_MPI
			"MPI multithread support: %s\n"
			#endif
			"GVT Time Period: %.2f seconds\n"
			"Checkpointing Type: %d\n"
			"Checkpointing Period: %d\n"
			"Snapshot Reconstruction Type: %d\n"
			"Halt Simulation After: %d\n"
			"LPs Distribution Mode across Kernels: %d\n"
			"Check Termination Mode: %d\n"
			"Blocking GVT: %d\n"
			"Set Seed: %ld\n",
			n_ker,
			get_cores(),
			n_cores,
			n_prc_tot,
			rootsim_config.output_dir,
			rootsim_config.scheduler,
			#ifdef HAVE_MPI
			((mpi_support_multithread)? "yes":"no"),
			#endif
			rootsim_config.gvt_time_period / 1000.0,
			rootsim_config.checkpointing,
			rootsim_config.ckpt_period,
			rootsim_config.snapshot,
			rootsim_config.simulation_time,
			rootsim_config.lps_distribution,
			rootsim_config.check_termination_mode,
			rootsim_config.blocking_gvt,
			rootsim_config.set_seed);
	}

	distribute_lps_on_kernels();

	// Initialize ROOT-Sim subsystems.
	// All init routines are executed serially (there is no notion of threads in there)
	// and the order of invocation can matter!

	base_init();
	scheduler_init();
	statistics_init();
	dymelor_init();
	communication_init();
	gvt_init();
	numerical_init();

	// This call tells the simulation engine that the sequential initial simulation is complete
	initialization_complete();
}


