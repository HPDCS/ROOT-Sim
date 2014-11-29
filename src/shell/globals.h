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
* @file globals.h
* @brief Global definitions for the interactive shell
* @date March 02, 2011
* @author Alessandro Pellegrini
*/


#pragma once
#ifndef SHELLGLOBALS_H
#define SHELLGLOBALS_H

#ifdef bool
#undef bool
#endif
typedef enum {false, true} bool;


// These are the supported main commands
#define CONFIG_C	1
#define SET_C		2
#define HELP_C		3
#define RUN_C		4
#define EXIT_C		5
#define UNSET_C		6
#define RESET_C		7
#define CMDLINE_C	8
#define WATERMARK_C	9


// This is the maximum number of parameters acceptable by a command
#define MAX_ARGV 2

// This is the maximum number of runtime variables the user can specify
#define MAX_VARIABLES 256

// This is the maximum length of a parameter
#define MAX_PARAMETER 256


// This struct is the result of command parsing
typedef struct cmd {
	int cmd_code;
	char argv[MAX_ARGV][MAX_PARAMETER];
	int nargs;
} cmd_t;


// This struct represents the couple <variable, value>, used to store the runtime variables
// interactively defined by the user
typedef struct runtimevars {
	int nvars;
	char variable[MAX_VARIABLES][MAX_PARAMETER];
	char value[MAX_VARIABLES][MAX_PARAMETER];
	int is_internal[MAX_VARIABLES];
	char description[MAX_VARIABLES][1024];
} runtimevars_t;

// This is the core function to execute commands
extern void execute_cmd(cmd_t *cmd);
bool execute_run(cmd_t *cmd);
cmd_t *new_cmd(int cmd_code);
cmd_t *add_arg(cmd_t *cmd, char *arg);

// Application-wide global variables
extern runtimevars_t runtime_variables;

#endif /* SHELLGLOBALS_H */

