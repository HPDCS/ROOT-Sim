/*
 * topology_utils.h
 *
 *  Created on: 24 gen 2019
 *      Author: andrea
 */

#ifndef MODELS_ROBOT_EXPLORE_ABM_TOPOLOGY_UTILS_H_
#define MODELS_ROBOT_EXPLORE_ABM_TOPOLOGY_UTILS_H_

#include "application.h"

unsigned int compute_my_direction(agent_state_type *state);
unsigned int closest_frontier(agent_state_type *state, unsigned int curr_cell, unsigned int exclude);

#endif /* MODELS_ROBOT_EXPLORE_ABM_TOPOLOGY_UTILS_H_ */
