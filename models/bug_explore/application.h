#pragma once
#include <stdbool.h>
#include <ROOT-Sim.h>

enum _bug_events_t{
	PRODUCE_FOOD = INIT + 1,
	SPAWN_BUG,
	BUG_LEAVING,
	BUG_VISIT,
	BUG_TRAVERSE
};

#define REGION_IN 1
#define REGION_OUT 2
#define UPDATE_NEIGHBOURS 3
#define REPRODUCE 5
#define TIME_STEP 5.0
#define MAX_FOOD_PRODUCTION_RATE 2
#define MAX_CONSUMPTION_RATE 5
#define REPRODUCTION_SIZE 20
#define CHILD_COUNT 5
#define SURVIVAL_PROBABILITY 95 
#define EXECUTION_TIME 10000000 //dummy: it can be modified with a specific parameter 
#define TOT_REG (int)n_prc_tot
#ifndef NUM__OCCUPIED_CELLS
    #define NUM_OCCUPIED_CELLS 1  //default number of occupied cells (i.e.: number of bugs)
#endif
#ifndef BUG_PER_CELL
    #define BUG_PER_CELL 1 //at most one bug per cell at a time
#endif



