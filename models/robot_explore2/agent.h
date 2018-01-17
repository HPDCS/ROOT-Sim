#ifndef _AGENT_H
#define _AGENT_H

#include <math.h>
#include <limits.h>

#include "application.h"

#define AGENT_TIME_STEP	5.0

unsigned int opposite_direction_of(unsigned int direction);

char *direction_name(unsigned int direction);

double a_star(agent_state_type *astate, cell_state_type *rstate, unsigned int current_cell, unsigned int *good_direction);

unsigned int compute_my_direction(agent_state_type *state);

unsigned int closest_frontier(agent_state_type *astate, cell_state_type *rstate, unsigned int exclude);

#endif				// _AGENT_H
