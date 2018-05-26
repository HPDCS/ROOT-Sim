/*
 * argp_example.c
 *
 *  Created on: 23 mag 2018
 *      Author: andrea
 */

#include <argp.h>
#include <stdlib.h>
#include "logger.h"

error_t my_parse(int key, char* arg, struct argp_state *state) {
	switch (key) {
		case 'l':
			abm_log_level_prog = atoi(arg);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

struct argp_option my_options[2] = { { "log-level", 'l', "LEVEL", 0, "sets the logging level", 0 }, { 0 } };

struct argp model_argp = { my_options, my_parse, "", "", 0, 0, 0 };

