#pragma once
#ifndef _TCAR_H
#define _TCAR_H


#include <ROOT-Sim.h>

#define TIME_STEP 5.0

enum _event_type{
	SUGAR_VISIT = INIT + 1,
	SUGAR_INIT,
	SUGAR_LEAVE,
	SUGAR_REFILL,
	_TRAVERSE
};

#define SOURCEBASERADIUS 5

#define INIT_EATERS 100;

#define MAX_INITIAL_WEALTH 25
#define MIN_INITIAL_WEALTH 5

#define MAX_EAT_RATE 4
#define MIN_EAT_RATE 1

#define MAX_MAX_AGE 90
#define MIN_MAX_AGE 60

typedef struct _sugar_eater_t{
	unsigned remaining_steps;
	unsigned eat_rate;
	unsigned wealth;
}sugar_eater_t;

typedef struct _region_t{
	unsigned capacity;
	struct{
		unsigned sugar;
		unsigned eaters;
	}n;
} region_t;


#endif /* _ANT_ROBOT_H */
