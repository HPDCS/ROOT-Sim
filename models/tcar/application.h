#pragma once
#ifndef _TCAR_H
#define _TCAR_H


#include <ROOT-Sim.h>

enum _distribution_type{
	UNIFORM,
	EXPONENTIAL
};

#define DISTRIBUTION EXPONENTIAL

#define TIME_STEP 5.0

#define ROBOTS_PER_CELL 1


enum _event_type{
	REGION_IN = INIT + 1,
	REGION_OUT,
	PING,
	_TRAVERSE
};

#ifndef OCCUPIED_CELLS
	#define OCCUPIED_CELLS 8
#endif

#ifndef MINIMUM_VISITS
	#define MINIMUM_VISITS	10
#endif

typedef struct _lp_state_type{
	unsigned trails;
} lp_state_type;


#endif /* _ANT_ROBOT_H */
