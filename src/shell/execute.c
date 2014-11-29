/**
*			Copyright (C) 2008-2014 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file execute.c
* @brief Shell's commands implementation
* @date March 02, 2011
* @author Alessandro Pellegrini
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>
#include <time.h>

#include "globals.h"
#include "../simulator/subsystems/core/init.h"




/// This macro frees the memory used by strings used to compose ROOT-Sim's command line
#define free_command_line(cl) 	do { \
					int __i = 0;\
					while((cl)[__i] != NULL) {\
						free((cl)[__i++]);\
					}\
					free((cl));\
					(cl) = NULL;\
				} while (0)



/// This global variable stores the runtime variables set by the user
runtimevars_t runtime_variables;



/**
* This is a re-implementation of the non ANSI-C function stricmp (case-insentive strcmp).
*
* @author Alessandro Pellegrini
*
* @param cs Source string for comparison
* @param ct Target string for comparison
*
* @return Numerical difference between the strings (halting at the first different character)
*/
int __stricmp(const unsigned char * cs, const unsigned char * ct) {
	register signed char __res;

	while(1) {
		if((__res = toupper(*cs) - toupper(*ct++)) != 0 || !*cs++)
			break;
	}

	return __res;
}



/**
* This is a weak alias declaration, used to avoid name conflicts if stricmp is actually
* defined in some library installed on the system.
*
* @author Alessandro Pellegrini
*
* @param cs Source string for comparison
* @param ct Target string for comparison
*
* @return Numerical difference between the strings (halting at the first different character)
*/
int stricmp(const char *cs, const char *ct) __attribute__ ((weak, alias("__stricmp")));




/**
* This function adds to the global shell's configuration variable a new internal runtime
* variable. This is used at startup time.
*
* @author Alessandro Pellegrini
*
* @param cs Source string for comparison
* @param ct Target string for comparison
*
* @todo After having changed the semantic of ROOT-Sim's command line arguments, internal
* 	variables can now be unset. The 'is_internal' flag is no longer needed and should be removed
*/
static void add_internal_runtime_variable(char *name, char *value, char *description) {
	strcpy(runtime_variables.variable[runtime_variables.nvars], name);
	strcpy(runtime_variables.value[runtime_variables.nvars], value);
	strcpy(runtime_variables.description[runtime_variables.nvars], " - ");
	strcat(runtime_variables.description[runtime_variables.nvars], description);
	runtime_variables.is_internal[runtime_variables.nvars] = 0;
	runtime_variables.nvars++;
}




/**
* This function implements the 'HELP' command
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*/
void execute_help(cmd_t *cmd) {

	if(cmd->argv[0][0] == '\0') {
		puts("List of available commands (alphabetically):\n\n"\
		     "CONFIG:\tLists information about the current ROOT-Sim's configuration\n"\
		     "RUN:\tStarts the simulation of the specified application-level program\n"\
		     "SET:\tSpecifies simulation parameter's values\n"\
		     "UNSET:\tRemoves the specified simulation parameter\n"\
		     "CMDLINE:Print the command line for running a simulation\n"\
		     "\nType \"help\" followed by a command name for full documentation\n\n");
		return;
	}

	if(stricmp(cmd->argv[0], "run") == 0) {
		puts("RUN <what>\n"\
		     "Starts the simulation of the <what> application."\
		     "Absolute or relative path to the model can be specified.\n\n");
		return;
	}

	if(stricmp(cmd->argv[0], "set") == 0) {
		puts("SET <var> <value>\n"\
		     "Gives runtime variable <var> the value <value>. If <var> was already defined, the old value is "\
		     "superseded. Otherwise, it is created and the according value is set. "\
		     "Some runtime variables have pre-defined values. Type CONFIG to show all the "\
		     "runtime variables already set.\n\n");
		return;
	}

	if(stricmp(cmd->argv[0], "unset") == 0) {
		puts("UNSET <var>\n"\
		     "Removes the variable <var> from the pool of runtime variables.\n\n");
		return;
	}

	if(stricmp(cmd->argv[0], "cmdline") == 0) {
		puts("CMDLINE <program>\n"\
		     "Print the command line which can be used to run ROOT-Sim with current configuration outside of the interactive shell, loading the <program> simulation model.\n\n");
		return;
	}

	printf("Unknown command \"%s\". Type HELP for the full list of available commands.\n", cmd->argv[0]);
}





/**
* This function implements the 'CONFIG' command
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*/
void execute_config(void) {

	printf("Configuration variables set:\n");
	int i;
	for(i = 0; i < runtime_variables.nvars; i++) {
		printf("%s = %s%s\n", runtime_variables.variable[i], runtime_variables.value[i], runtime_variables.description[i]);
	}
	puts("\n");
	return;

}



/**
* This function implements the 'RESET' command. Any internal runtime variable must be placed in here,
* as this function is called as well at shell's startup time.
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*/
void execute_reset(void) {

	bzero(&runtime_variables, sizeof(runtime_variables));

	add_internal_runtime_variable("np", "1", "number of Simulation Kernels to be run, range: 1 to max number of available cores");
	add_internal_runtime_variable("machinefile", "", "configuration for deploying the run over different machines");
	add_internal_runtime_variable("nprc", "4", "number of involved simulation objects, range: [1, 8192]");
	add_internal_runtime_variable("gvt", "1000", "time period for a new GVT calculation (msec), range: [500, 5000]");
	add_internal_runtime_variable("gvt_snapshot_cycles", "2", "number of consecutive GVT calculations before rebuilding a state for temination check");
	add_internal_runtime_variable("scheduler", "stf", "scheduling algorithm, valid values: {stf, star}");
	add_internal_runtime_variable("simulation_time", "0", "Wall-clock-time termination predicate. Range: any non negative value. 0 for infinity");
	add_internal_runtime_variable("lps_distribution", "block", "LPs Distribution over the kernels: {block, circular}");
	add_internal_runtime_variable("cktrm_mode", "standard", "termination predicate check: {incremental, standard}");
}




/**
* This function implements the 'SET' command. If a variable to be set is already present, its value
* is overwritten
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*/
void execute_set(cmd_t *cmd) {
	int i;

	// See whether the symbol has been already defined
	for(i = 0; i < runtime_variables.nvars; i++) {
		if(strcmp(runtime_variables.variable[i], cmd->argv[0]) == 0) {
			// Variable found: replace the value
			strcpy(runtime_variables.value[i], cmd->argv[1]);
			bzero(runtime_variables.description[i], 1024);
			return;
		}
	}

	// Store the variable name and its value
	strcpy(runtime_variables.variable[i], cmd->argv[0]);
	strcpy(runtime_variables.value[i], cmd->argv[1]);
	bzero(runtime_variables.description[i], 1024);
	runtime_variables.nvars++;
}




/**
* This function implements the 'UNSET' command
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*
* @todo Remove the check on internal variables, as it is now useless.
*/
void execute_unset(cmd_t *cmd) {
	int i, j;

	// Looks for the specified symbol
	for(i = 0; i < runtime_variables.nvars; i++) {
		if(strcmp(runtime_variables.variable[i], cmd->argv[0]) == 0) {

			// Internal variables cannot be unset
			if(!runtime_variables.is_internal[i]) {

				// Variable found: move subsequent entries up
				for(j = i+1; j <= runtime_variables.nvars; j++) {
					strcpy(runtime_variables.variable[j-1], runtime_variables.variable[j]);
					strcpy(runtime_variables.value[j-1], runtime_variables.value[j]);
					strcpy(runtime_variables.description[j-1], runtime_variables.description[j]);
				}
				runtime_variables.nvars--;
				printf("Variable \"%s\" has been unset correctly.\n", cmd->argv[0]);
				return;
			}

			printf("Error: cannot unset variable \"%s\", because it's a ROOT-Sim's internal parameter\n", cmd->argv[0]);
			return;
		}
	}

	printf("Error: variable \"%s\" is not set.\n", cmd->argv[0]);

}


/**
* This function builds the command line used to launch ROOT-Sim.
* It allocates memory for the actual string, which must be free'd by the caller
* though free_command_line() macro
*
* @author Alessandro Pellegrini
*/
static char **build_command_line(cmd_t *cmd) {
	int i, j;
	char **args;
	const char *name;
	int application_param;
	int skip_param = 2; // np - machinefile

	// Assemble the parameters' vector
	args = (char **)malloc(sizeof(char *) * (runtime_variables.nvars * 2 + 10)); // +6 is for the program name and the NULL terminating pointer and -v and -ckpt 1 1

	args[0] = malloc((strlen("mpirun") + 1) * sizeof(char)); strcpy(args[0], "mpirun");
	args[1] = malloc((strlen("-v") + 1) * sizeof(char)); strcpy(args[1], "-v");
	args[2] = malloc((strlen("-np") + 1) * sizeof(char)); strcpy(args[2], "-np");
	// First variable must be np
	args[3] = malloc((strlen(runtime_variables.value[0]) + 1) * sizeof(char)); strcpy(args[3], runtime_variables.value[0]);

	args[4] = malloc((strlen("-mca") + 1) * sizeof(char)); strcpy(args[4], "-mca");
	args[5] = malloc((strlen("btl_tcp_if_include") + 1) * sizeof(char)); strcpy(args[5], "btl_tcp_if_include");
	args[6] = malloc((strlen("eth0") + 1) * sizeof(char)); strcpy(args[6], "eth0");
	application_param = 7;

	if (strlen(runtime_variables.value[1]) != 0) {
		args[application_param] = malloc((strlen("-machinefile") + 1) * sizeof(char)); strcpy(args[application_param], "-machinefile");
		application_param++;
		args[application_param] = malloc((strlen(runtime_variables.value[1]) + 1) * sizeof(char)); strcpy(args[application_param], runtime_variables.value[1]);
		application_param++;
	}

	args[application_param] = malloc(128 * sizeof(char));
	sprintf(args[application_param], "%s", cmd->argv[0]);

	// Scan the runtime variables and check which of them are platform specific.
	for(i = 0; i < runtime_variables.nvars - 1; i++) {

		args[application_param + (i*2) + 1] = malloc(128 * sizeof(char));
		bzero(args[application_param  + (i*2) + 1], 128 * sizeof(char));

		// Check whether the variable is platform specific (to add the double dash)
		j = 0;
		while((name = long_options[j++].name) != NULL) {
			if(strcmp(name, runtime_variables.variable[i + skip_param]) == 0) {
				strcat(args[application_param + (i*2) + 1], "--");
				break;
			}
		}

		strncat(args[application_param + i*2 + 1], runtime_variables.variable[i + skip_param], 126);
		args[application_param + (i*2) + 2] = malloc(128 * sizeof(char));
		strcpy(args[application_param + (i*2) + 2], runtime_variables.value[i + skip_param]);
	}

	// Last argument is NULL
	args[application_param + i * 2 + 1] = NULL;

	return args;

}



/**
* This function implements the 'CMDLINE' command
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*/
void execute_cmdline(cmd_t *cmd) {
	int i;
	char **args;

	// Get the current command line
	args = build_command_line(cmd);

	puts("Command Line to run ROOT-Sim with current configuration:");

	i = 0;
	while(args[i] != NULL)
		printf("%s ", args[i++]);
	printf("\n");

	free_command_line(args);

}



/**
* This function implements the 'RUN' command. It builds the command line with all the arguments and forks to it.
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
* @return Returns true if execution was successful
*/
bool execute_run(cmd_t *cmd) {
	pid_t rootsim;
	char **args;
	int retval;

	// Get the current command line
	args = build_command_line(cmd);	

	rootsim = fork();
	if(rootsim == 0) {
		execvp("mpiexec", args);
	} else if(rootsim < 0) {
		free_command_line(args);
		return false;
	} else {
		wait(&retval);
	}

	free_command_line(args);

	return true;
}



char a[] = {
	0x33,0x10,0x15,0x15,0x12,0x1a,0x0d,0x1b,0x0b,0x1d,
	0x0a,0x16,0x02,0x06,0x0a,0x0b,0x0f,0x08,0x07,0x05,
	0x03,0x01,0x11,0x07,0x06,0x06,0x02,0x01,0x13,0x07,
	0x06,0x05,0x16,0x06,0x07,0x04,0x17,0x04,0x09,0x04,
	0x17,0x04,0x09,0x03,0x01,0x09,0x06,0x02,0x06,0x04,
	0x0a,0x02,0x03,0x08,0x04,0x05,0x04,0x04,0x0a,0x02,
	0x01,0x09,0x04,0x07,0x04,0x02,0x0f,0x08,0x04,0x01,
	0x01,0x01,0x08,0x01,0x13,0x01,0x01,0x01,0x28,0x01,
	0x26,0x01,0x01,0x01,0x24,0x02,0x25,0x05,0x22,0x09,
	0x1d,0x09,0x01,0x01,0x1d,0x09,0x07,0x01,0x16,0x07,
	0x01,0x03,0x1d,0x08,0x01,0x03,0x1c,0x08,0x21,0x0a,
	0x04,0x01,0x03,0x01,0x16,0x0d,0x02,0x02,0x15,0x01,
	0x01,0x0f,0x12,0x02,0x03,0x02,0x01,0x0d,0x18,0x02,
	0x02,0x0c,0x1a,0x01,0x02,0x0a,0x1a,0x01,0x05,0x06,
	0x1d,0x01,0x03,0x03,0x01,0x02
};
void watermark(void) {
 	int i=0,j,k=0,w=0;
	while(i<154) {
   		j=a[i++];
		w++;
		while(j-->0) {
			if(k++%40==0) putchar('\n');
			putchar((w%2==1?' ':'M'));
		}
	}
	putchar('\n');
}


/**
* This function is called from the parser whenever a grammar's rule is correctly matched.
* The value in cmd->cmd_code is used to demultiplex to the actual function which implements
* the command.
*
* @author Alessandro Pellegrini
*
* @param cmd Information from the typed command
*/
void execute_cmd(cmd_t *cmd) {

	switch(cmd->cmd_code) {

		case CONFIG_C:
			execute_config();
			break;
		case RESET_C:
			execute_reset();
			break;
		case SET_C:
			execute_set(cmd);
			break;
		case UNSET_C:
			execute_unset(cmd);
			break;
		case HELP_C:
			execute_help(cmd);
			break;
		case RUN_C:
			if(execute_run(cmd) == false)
				printf("Error: unable to launch the simulator. Check your installation!\n");
			break;
		case CMDLINE_C:
			execute_cmdline(cmd);
			break;
		case WATERMARK_C:
			watermark();
			break;
		case EXIT_C:
			exit(EXIT_SUCCESS);
	}

	// Free some memory
	if(cmd != NULL)
		free(cmd);
}

