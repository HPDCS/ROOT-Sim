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

typedef struct _region_t{
	unsigned healthy;
	simtime_t now;
} region_t;

enum _event_t{
	MIDNIGHT = INIT + 1,
	RECEIVE_HEALTHY,
	INFECTION,
	GUY_VISIT,
	GUY_LEAVE,
	GUY_INIT,
	_TRAVERSE
};
// this samples a random number with binomial distribution TODO could be useful in the numeric library
unsigned random_binomial(unsigned trials, double p);

#endif /* MODELS_TUBERCOLOSIS_APPLICATION_H_ */
