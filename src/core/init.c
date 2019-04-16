/**
* @file core/init.c
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
#include <arch/thread.h>
#include <communication/communication.h>
#include <core/core.h>
#include <core/init.h>
#include <datatypes/bitmap.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <mm/state.h>
#include <mm/ecs.h>
#include <mm/mm.h>
#include <statistics/statistics.h>
#include <lib/numerical.h>
#include <lib/topology.h>
#include <lib/abm_layer.h>
#include <serial/serial.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif


/// This is the list of mnemonics for arguments
enum _opt_codes{
	OPT_FIRST = 128, /**< this is used as an offset to the enum values, so that argp doesn't assign short options */

	// make sure these ones are mapped correctly to the external enum param_codes,
	OPT_SCHEDULER = 	OPT_FIRST + PARAM_SCHEDULER,
	OPT_CKTRM_MODE = 	OPT_FIRST + PARAM_CKTRM_MODE,
	OPT_LPS_DISTRIBUTION =	OPT_FIRST + PARAM_LPS_DISTRIBUTION,
	OPT_VERBOSE = 		OPT_FIRST + PARAM_VERBOSE,
	OPT_STATS = 		OPT_FIRST + PARAM_STATS,
	OPT_STATE_SAVING = 	OPT_FIRST + PARAM_STATE_SAVING,
	OPT_SNAPSHOT = 		OPT_FIRST + PARAM_SNAPSHOT,

	OPT_NP,
	OPT_NPRC,
	OPT_OUTPUT_DIR,
	OPT_NPWD,
	OPT_P,
	OPT_FULL,
	OPT_INC,
	OPT_A,
	OPT_GVT,
	OPT_GVT_SNAPSHOT_CYCLES,
	OPT_SIMULATION_TIME,
	OPT_DETERMINISTIC_SEED,
	OPT_SEED,
	OPT_SERIAL,
	OPT_NO_CORE_BINDING,

#ifdef HAVE_PREEMPTION
	OPT_PREEMPTION,
#endif
	OPT_LAST
};

// XXX we offset the first level with OPT_FIRST so remember about it when you index it!
const char * const param_to_text[][5] = {
	[OPT_SCHEDULER - OPT_FIRST] = {
			[SCHEDULER_INVALID] = "invalid scheduler",
			[SCHEDULER_STF] = "stf",
	},
	[OPT_CKTRM_MODE - OPT_FIRST] = {
			[CKTRM_INVALID] = "invalid termination checking",
			[CKTRM_NORMAL] = "normal",
			[CKTRM_INCREMENTAL] = "incremental",
			[CKTRM_ACCURATE] = "accurate"
	},
	[OPT_LPS_DISTRIBUTION - OPT_FIRST] = {
			[LP_DISTRIBUTION_INVALID] = "invalid LPs distribution",
			[LP_DISTRIBUTION_BLOCK] = "block",
			[LP_DISTRIBUTION_CIRCULAR] = "circular"
	},
	[OPT_VERBOSE - OPT_FIRST] = {
			[VERBOSE_INVALID] = "invalid verbose specification",
			[VERBOSE_INFO] = "info",
			[VERBOSE_DEBUG] = "debug",
			[VERBOSE_NO] = "no"
	},
	[OPT_STATS - OPT_FIRST] = {
			[STATS_INVALID] = "invalid statistics specification",
			[STATS_GLOBAL] = "global",
			[STATS_PERF] = "performance",
			[STATS_LP] = "lp",
			[STATS_ALL] = "all"
	},
	[OPT_STATE_SAVING - OPT_FIRST] = {
			[STATE_SAVING_INVALID] = "invalid checkpointing specification",
			[STATE_SAVING_COPY] = "copy",
			[STATE_SAVING_PERIODIC] = "periodic",
	},
	[OPT_SNAPSHOT - OPT_FIRST] = {
			[SNAPSHOT_INVALID] = "invalid snapshot specification",
			[SNAPSHOT_FULL] = "full",
	}
};

const char *argp_program_version 	= PACKAGE_STRING "\nCopyright (C) 2008-2019 HPDCS Group";
const char *argp_program_bug_address 	= PACKAGE_BUGREPORT;

// Directly from argp documentation:
// If non-zero, a string containing extra text to be printed before and after the options in a long help message,
// with the two sections separated by a vertical tab ('\v', '\013') character.
// By convention, the documentation before the options is just a short string explaining what the program does.
// Documentation printed after the options describe behavior in more detail.
static char doc[] = "ROOT-Sim - a fast distributed multithreaded Parallel Discrete Event Simulator \v For more information check the official wiki at https://github.com/HPDCS/ROOT-Sim/wiki";

// this isn't needed (we haven't got non option arguments to document)
static char args_doc[] = "";
// the options recognized by argp
static const struct argp_option argp_options[] = {
	{"wt",			OPT_NP,			"VALUE",	0,		"Number of total cores being used by the simulation", 0},
	{"lp",			OPT_NPRC,		"VALUE",	0,		"Total number of Logical Processes being launched at simulation startup", 0},
	{"output-dir",		OPT_OUTPUT_DIR,		"PATH",		0,		"Path to a folder where execution statistics are stored. If not present, it is created", 0},
	{"scheduler",		OPT_SCHEDULER,		"TYPE",		0,		"LP Scheduling algorithm. Supported values are: stf", 0},
	{"npwd",		OPT_NPWD,		0,		0,		"Non Piece-Wise-Deterministic simulation model. See manpage for accurate description", 0},
	{"p",			OPT_P,			"VALUE",	0,		"Checkpointing interval", 0},
	{"full",		OPT_FULL,		0,		0,		"Take only full logs", 0},
	{"inc",			OPT_INC,		0,		0,		"Take only incremental logs (still to be released)", 0},
	{"A",			OPT_A,			0,		0,		"Autonomic subsystem: set checkpointing interval and log mode automatically at runtime (still to be released)", 0},
	{"gvt",			OPT_GVT,		"VALUE",	0,		"Time between two GVT reductions (in milliseconds)", 0},
	{"cktrm-mode",		OPT_CKTRM_MODE,		"TYPE",		0,		"Termination Detection mode. Supported values: normal, incremental, accurate", 0},
	{"gvt-snapshot-cycles",	OPT_GVT_SNAPSHOT_CYCLES, "VALUE",	0,		"Termination detection is invoked after this number of GVT reductions", 0},
	{"simulation-time",	OPT_SIMULATION_TIME, 	"VALUE",	0,		"Halt the simulation when all LPs reach this logical time. 0 means infinite", 0},
	{"lps-distribution",	OPT_LPS_DISTRIBUTION, 	"TYPE",		0,		"LPs distributions over simulation kernels policies. Supported values: block, circular", 0},
	{"deterministic-seed",	OPT_DETERMINISTIC_SEED,	0,		0, 		"Do not change the initial random seed for LPs. Enforces different deterministic simulation runs", 0},
	{"verbose",		OPT_VERBOSE,		"TYPE",		0,		"Verbose execution", 0},
	{"stats",		OPT_STATS,		"TYPE",		0,		"Level of detail in the output statistics", 0},
	{"seed",		OPT_SEED,		"VALUE",	0,		"Manually specify the initial random seed", 0},
	{"serial",		OPT_SERIAL,		0,		0,		"Run a serial simulation (using Calendar Queues)", 0},
	{"sequential",		OPT_SERIAL,		0,		OPTION_ALIAS,	NULL, 0},
	{"no-core-binding",	OPT_NO_CORE_BINDING,	0,		0,		"Disable the binding of threads to specific physical processing cores", 0},

#ifdef HAVE_PREEMPTION
	{"no-preemption",	OPT_PREEMPTION,		0,		0,		"Disable Preemptive Time Warp", 0},
#endif
	{0}
};

#define malformed_option_failure()	argp_error(state, "invalid value \"%s\" in %s option.\nAborting!", arg, state->argv[state->next -1 -(arg != NULL)])

#define conflicting_option_failure(msg)	argp_error(state, "the requested option %s with value \"%s\" is conflicting: " msg "\nAborting!", state->argv[state->next -1 -(arg != NULL)], arg)

// this parses an string option leveraging the 2d array of strings specified earlier
// the weird iteration style skips the element 0, which we know is associated with an invalid value description
#define handle_string_option(label, var)						\
	case label:									\
	({										\
		unsigned __i = 1;							\
		while(1) {								\
			if(strcmp(arg, param_to_text[key - OPT_FIRST][__i]) == 0) {	\
				var = __i;						\
				break;							\
			}								\
			if(!param_to_text[key - OPT_FIRST][++__i])			\
				malformed_option_failure();				\
		}									\
	});										\
	break


// the compound expression equivalent to __value >= low is needed in order to suppress a warning when low == 0
#define parse_ullong_limits(low, high) 	\
	({														\
		unsigned long long int __value;										\
		char *__endptr;												\
		__value = strtoull(arg, &__endptr, 10);									\
		if(!(*arg != '\0' && *__endptr == '\0' && (__value > low || __value == low) && __value <= high)) {	\
			malformed_option_failure();									\
		}													\
		__value;												\
	})

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	// this is used in order to ensure that the user doesn't use duplicate options
	static rootsim_bitmap scanned[bitmap_required_size(OPT_LAST - OPT_FIRST)];

	if(key >= OPT_FIRST && key < OPT_LAST){
		if(bitmap_check(scanned, key - OPT_FIRST))
			conflicting_option_failure("this option has already been specified");

		bitmap_set(scanned, key - OPT_FIRST);
	}

	switch (key) {
		case OPT_NP:
			if(strcmp(arg, "auto") == 0){
				n_cores = get_cores();
			}else{
				n_cores = parse_ullong_limits(1, UINT_MAX);
			}
			break;

		case OPT_NPRC:
			n_prc_tot = parse_ullong_limits(1, UINT_MAX);
			break;

		case OPT_OUTPUT_DIR:
			rootsim_config.output_dir = arg;
			break;

		handle_string_option(OPT_SCHEDULER, rootsim_config.scheduler);
		handle_string_option(OPT_FULL, rootsim_config.snapshot);
		handle_string_option(OPT_CKTRM_MODE, rootsim_config.check_termination_mode);
		handle_string_option(OPT_VERBOSE, rootsim_config.verbose);
		handle_string_option(OPT_STATS, rootsim_config.stats);
		handle_string_option(OPT_LPS_DISTRIBUTION, rootsim_config.lps_distribution);

		case OPT_NPWD:
			if (bitmap_check(scanned, OPT_P-OPT_FIRST)) {
				conflicting_option_failure("I'm requested to run non piece-wise deterministically, but a checkpointing interval is set already.");
			} else {
				rootsim_config.checkpointing = STATE_SAVING_COPY;
			}
			break;

		case OPT_P:
			if(bitmap_check(scanned, OPT_NPWD-OPT_FIRST)) {
				conflicting_option_failure("Copy State Saving is selected, but I'm requested to set a checkpointing interval.");
			} else {
				rootsim_config.checkpointing = STATE_SAVING_PERIODIC;
				rootsim_config.ckpt_period = parse_ullong_limits(1, 40);
				// This is a micro optimization that makes the LogState function to avoid checking the checkpointing interval and keeping track of the logs taken
				if(rootsim_config.ckpt_period == 1)
					rootsim_config.checkpointing = STATE_SAVING_COPY;
			}
			break;

		case OPT_INC:
			argp_failure(state, EXIT_FAILURE, ENOSYS, "incremental state saving is not supported in stable version yet...\nAborting");
			break;

		case OPT_A:
			argp_failure(state, EXIT_FAILURE, ENOSYS, "autonomic state saving is not supported in stable version yet...\nAborting");
			break;

		case OPT_GVT:
			rootsim_config.gvt_time_period = parse_ullong_limits(1, 10000);
			break;

		case OPT_GVT_SNAPSHOT_CYCLES:
			rootsim_config.gvt_snapshot_cycles = parse_ullong_limits(1, INT_MAX);
			break;

		case OPT_SIMULATION_TIME:
			rootsim_config.simulation_time = parse_ullong_limits(0, INT_MAX);
			break;

		case OPT_DETERMINISTIC_SEED:
			rootsim_config.deterministic_seed = true;
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

		case ARGP_KEY_INIT:

			memset(&rootsim_config, 0, sizeof(rootsim_config));
			memset(scanned, 0, sizeof(scanned));
			// Store the predefined values, before reading any overriding one
			rootsim_config.output_dir = DEFAULT_OUTPUT_DIR;
			rootsim_config.scheduler = SCHEDULER_STF;
			rootsim_config.lps_distribution = LP_DISTRIBUTION_BLOCK;
			rootsim_config.check_termination_mode = CKTRM_NORMAL;
			rootsim_config.stats = STATS_ALL;
			rootsim_config.verbose = VERBOSE_INFO;
			rootsim_config.snapshot = SNAPSHOT_FULL; // TODO: in the future, default to AUTONOMIC_
			rootsim_config.checkpointing = STATE_SAVING_PERIODIC;
			rootsim_config.gvt_time_period = 1000;
			rootsim_config.gvt_snapshot_cycles = 2;
			rootsim_config.ckpt_period = 10;
			rootsim_config.simulation_time = 0;
			rootsim_config.deterministic_seed = false;
			rootsim_config.set_seed = 0;
			rootsim_config.serial = false;
			rootsim_config.core_binding = true;

#ifdef HAVE_PREEMPTION
			rootsim_config.disable_preemption = false;
#endif
			break;

		case ARGP_KEY_SUCCESS:

			// sanity checks
			if(!rootsim_config.serial && !bitmap_check(scanned, OPT_NP - OPT_FIRST))
				rootsim_error(true, "Number of cores was not provided \"--wt\"\n");

			if(!bitmap_check(scanned, OPT_NPRC - OPT_FIRST))
				rootsim_error(true, "Number of LPs was not provided \"--lp\"\n");

			if(n_cores > get_cores())
				rootsim_error(true, "Demanding %u cores, which are more than available (%d)\n", n_cores, get_cores());

			if(n_cores > MAX_THREADS_PER_KERNEL)
				rootsim_error(true, "Too many threads, maximum supported number is %u\n", MAX_THREADS_PER_KERNEL);

			if(n_prc_tot > MAX_LPs)
				rootsim_error(true, "Too many LPs, maximum supported number is %u\n", MAX_LPs);

			if(!rootsim_config.serial && n_prc_tot < n_cores)
				rootsim_error(true, "Requested a simulation run with %u LPs and %u worker threads: the mapping is not possible\n", n_prc_tot, n_cores);

			print_config();

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

#undef parse_ullong_limits
#undef handle_string_option
#undef conflicting_option_failure
#undef malformed_option_failure

static struct argp_child argp_child[2] = {
		{0, 0, "Model specific options", 0},
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
void SystemInit(int argc, char **argv)
{
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
		ScheduleNewEvent = SerialScheduleNewEvent;
		initialize_lps();
		numerical_init();
		statistics_init();
		serial_init();
		topology_init();
		abm_layer_init();
		return;
	} else {
		ScheduleNewEvent = ParallelScheduleNewEvent;
	}

	// Initialize ROOT-Sim subsystems.
	// All init routines are executed serially (there is no notion of threads in there)
	// and the order of invocation can matter!
	base_init();
	segment_init();
	initialize_lps();
	remote_memory_init();
	statistics_init();
	scheduler_init();
	communication_init();
	gvt_init();
	numerical_init();
	topology_init();
	abm_layer_init();

	// This call tells the simulation engine that the sequential initial simulation is complete
	initialization_complete();
}


