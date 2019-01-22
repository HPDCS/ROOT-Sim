#pragma once
#ifndef _TCAR_H
#define _TCAR_H


#include <ROOT-Sim.h>

/* DISTRIBUZIONI TIMESTAMP */

enum _distribution_type{
	UNIFORM,
	EXPONENTIAL
};

#define DISTRIBUTION EXPONENTIAL



/* Per simulare il time stepping del modello da cui abbiamo preso spunto dove si diceva che:
 *"ogni movimento di un robot impiega 1 time-step e che ogni robot si muove 1 volta per ogni time-step"
 */
#define TIME_STEP 5.0


enum _event_type{
	REGION_IN = INIT + 1,
	REGION_OUT,
	_TRAVERSE
};

#ifndef OCCUPIED_CELLS
	#define OCCUPIED_CELLS 8
#endif
#ifndef ROBOTS_PER_CELL
	#define ROBOTS_PER_CELL 2
#endif

#ifndef MINIMUM_VISITS
	#define MINIMUM_VISITS	1000
#endif

typedef struct _lp_state_type{
	unsigned trails;
} lp_state_type;


#endif /* _ANT_ROBOT_H */
