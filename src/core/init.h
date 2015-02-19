#pragma once
#ifndef _INIT_H

#ifndef _INIT_FROM_MAIN

#include <getopt.h>

/// The initial number of application-level argument the simulator reserves space for. If a greater number is found, the array is realloc'd
#define APPLICATION_ARGUMENTS 32



// This is the list of mnemonics for arguments
#define OPT_NP			1
#define OPT_NPRC		2
#define OPT_OUTPUT_DIR		3
#define OPT_SCHEDULER		4
#define OPT_NPWD		5
#define OPT_P			6
#define OPT_FULL		7
#define OPT_INC			8
#define OPT_A			9
#define OPT_GVT			10
#define OPT_CKTRM_MODE		11
#define OPT_BLOCKING_GVT	12
#define OPT_GVT_SNAPSHOT_CYCLES	13
#define OPT_SIMULATION_TIME	14
#define OPT_LPS_DISTRIBUTION	15
#define OPT_BACKTRACE		16
#define OPT_DETERMINISTIC_SEED	17
#define OPT_VERBOSE		18
#define OPT_STATS		19
#define OPT_SEED		20
#define OPT_SERIAL		21

// TODO: a vector of vector with text name of numerical options, which should be used for parsing options and for displaying names
// static char *opt_opt[][] = { ... }


static char *opt_desc[] = {
	"",
	"Number of total cores being used by the simulation",
	"Total number of Logical Processes being lunched at simulation startup",
	"Path to a folder where execution statistics are stored. If not present, it is created",
	"LP Scheduling algorithm. Supported values are: stf",
	"Non Piece-Wise-Deterministic simulation model. See manpage for accurate description",
	"Checkpointing interval",
	"Take only full logs",
	"Take only incremental logs (still to be released)",
	"Autonomic subsystem: set checkpointing interval and log mode automatically at runtime (still to be released)",
	"Time between two GVT reductions (in milliseconds)",
	"Termination Detection mode. Supported values: standard, incremental",
	"Blocking GVT. All distributed nodes block until a consensus is agreed",
	"Termination detection is invoked after this number of GVT reductions",
	"Halt the simulation when all LPs reach this logical time. 0 means infinite",
	"LPs distributions over simulation kernels policies. Supported values: block, circular",
	"If the simulation crashes, print a backtrace",
	"Do not change the initial random seed for LPs. Enforces different deterministic simulation runs",
	"Verbose execution",
	"Level of detail in the output statistics",
	"Manually specify the initial random seed",
	"Run a serial simulation (using Calendar Queues)"
};


static struct option long_options[] = {
	{"np",			required_argument,	0, OPT_NP},
	{"output-dir",		required_argument,	0, OPT_OUTPUT_DIR},
	{"scheduler",		required_argument,	0, OPT_SCHEDULER},
	{"npwd",		no_argument,		0, OPT_NPWD},
	{"p",			required_argument,	0, OPT_P},
	{"full",		no_argument,		0, OPT_FULL},
	{"inc",			no_argument,		0, OPT_INC},
	{"A",			no_argument,		0, OPT_A},
	{"gvt",			required_argument,	0, OPT_GVT},
	{"cktrm_mode",		required_argument,	0, OPT_CKTRM_MODE},
	{"nprc",		required_argument,	0, OPT_NPRC},
	{"blocking_gvt",	no_argument,		0, OPT_BLOCKING_GVT},
	{"gvt_snapshot_cycles",	required_argument,	0, OPT_GVT_SNAPSHOT_CYCLES},
	{"simulation_time",	required_argument,	0, OPT_SIMULATION_TIME},
	{"lps_distribution",	required_argument,	0, OPT_LPS_DISTRIBUTION},
	{"backtrace",		no_argument,		0, OPT_BACKTRACE},
	{"deterministic_seed",	no_argument,		0, OPT_DETERMINISTIC_SEED},
	{"verbose",		required_argument,	0, OPT_VERBOSE},
	{"stats",		required_argument,	0, OPT_STATS},
	{"seed",		required_argument,	0, OPT_SEED},
	{"serial",		no_argument,		0, OPT_SERIAL},
	{"sequential",		no_argument,		0, OPT_SERIAL},
	{0,			0,			0, 0}
};

#else /* _INIT_FROM_MAIN */

extern void SystemInit(int argc, char **argv);

#endif

struct app_arguments {
	char **arguments;
	int size;
};
extern struct app_arguments model_parameters;

#endif /* _INIT_H */
