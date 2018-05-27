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
* @file parseparam.c
* @brief This module gives the user the possibility to process command line parameters passed to the application
* @author Alessandro Pellegrini
* @date 2/21/2013
*/

#include <ROOT-Sim.h>
#include <core/core.h>
#include <mm/dymelor.h>

/// This is used to retrieve a command line parameter within INIT event
#define getPar(args, i) (((char **)args)[(i)])



static int seek_param(void *args, char *name) {
	int i = 0;
// SANITY CHECK !!!
	if(args == NULL)
		return -1;

	while(getPar(args, i) != NULL) {

		if(strcmp(getPar(args, i), name) == 0) {
			return i;
		}

		i++;
	}

	return -1;
}


int GetParameterInt(void *args, char *name) {

	int i = seek_param(args, name);

	if(i == -1) {
		rootsim_error(false, "Parameter %s not set, returning -1...\n", name);
		return -1;
	}

	return parseInt(getPar(args, i+1));
}


float GetParameterFloat(void *args, char *name) {
	int i = seek_param(args, name);

	if(i == -1) {
		rootsim_error(false, "Parameter %s not set, returning -1...\n", name);
		return -1.0;
	}

	return parseFloat(getPar(args, i+1));
}


double GetParameterDouble(void *args, char *name) {
	int i = seek_param(args, name);

	if(i == -1) {
		rootsim_error(false, "Parameter %s not set, returning -1...\n", name);
		return -1.0;
	}

	return parseDouble(getPar(args, i+1));
}


bool GetParameterBool(void *args, char *name) {
	int i = seek_param(args, name);

	if(i == -1) {
		rootsim_error(false, "Parameter %s not set, returning false...\n", name);
		return false;
	}

	return parseBoolean(getPar(args, i+1));
}



char *GetParameterString(void *args, char *name) {
	int i = seek_param(args, name);

	if(i == -1) {
		rootsim_error(false, "Parameter %s not set, returning NULL...\n", name);
		return NULL;
	}

	return getPar(args, i+1);
}


bool IsParameterPresent(void *args, char *name) {
	// XXX: the "legacy" argument API must need to distinguish between the case in which we
	// are interested in only knowing if a certain argument is specified and the case in
	// which instead we are interested in retrieving the argument following one with a certain
	// name. In the latter case we need to ensure the following argument actually exists.
	// It seems this was overlooked so I added an (ugly) check to make sure of this.
	// Notice this breaks perfectly legal invocation where we only specify a single model argument!
	int ret = seek_param(args, name);
	return ret != -1  && getPar(args, ret+1);
}

