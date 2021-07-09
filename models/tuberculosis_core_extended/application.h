/*
 * application.h
 *
 *  Created on: 20 lug 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_APPLICATION_H_
#define MODELS_TUBERCOLOSIS_APPLICATION_H_

#include <ROOT-Sim.h>
#include "guy.h"

#define END_TIME 50

enum _event_t {
	RECEIVE_HEALTHY = INIT + 1,
	INFECTION,
	GUY_INIT,
	GUY_MOVE,
	GUY_RECV,
	STATS_COMPUTE,
};

// this samples a random number with binomial distribution TODO could be useful in the numeric library
unsigned random_binomial(unsigned trials, double p);

void reinitialize_missing_agents(unsigned, enum agent_state, simtime_t);

struct guy_t *init_guy(region_t *, enum agent_state);
#endif /* MODELS_TUBERCOLOSIS_APPLICATION_H_ */
