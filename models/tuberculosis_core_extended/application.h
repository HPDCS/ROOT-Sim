/*
 * application.h
 *
 *  Created on: 20 lug 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_APPLICATION_H_
#define MODELS_TUBERCOLOSIS_APPLICATION_H_

#include <ROOT-Sim.h>

#define END_TIME 10

enum agent_state {
	HEALTHY,
	SICK,
	INFECTED,
	TREATED,
	TREATMENT,
	END_STATES, // A dummy state to track the size of the enum
};

struct guy_t;

typedef struct _region_t {
	unsigned healthy;
	unsigned infected;
	unsigned sick;
	unsigned treatment;
	unsigned treated;
	simtime_t now;
	unsigned int me;
	unsigned long long counter;
	struct guy_t *agents[END_STATES];
} region_t;

enum _event_t {
	RECEIVE_HEALTHY = INIT + 1,
	INFECTION,
	GUY_VISIT,
	GUY_LEAVE,
	GUY_INIT,
	STATS_COMPUTE,
};

// this samples a random number with binomial distribution TODO could be useful in the numeric library
unsigned random_binomial(unsigned trials, double p);

void reinitialize_missing_agents(unsigned, enum agent_state, simtime_t);

#endif /* MODELS_TUBERCOLOSIS_APPLICATION_H_ */
