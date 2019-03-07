#pragma once
#include <stdbool.h>
#include <ROOT-Sim.h>

enum _bug_events_t{
	PRODUCE_FOOD = INIT + 1,
	SPAWN_BUG,
	BUG_LEAVING,
	BUG_DELAYED_VISIT,
	BUG_VISIT,
	BUG_TRAVERSE
};

#define TIME_STEP 1.0
#define MAX_FOOD_PRODUCTION_RATE 2
#define MAX_CONSUMPTION_RATE 5
#define REPRODUCTION_SIZE 20
#define CHILD_COUNT 5
#define SURVIVAL_PROBABILITY 95
#define NUM_OCCUPIED_CELLS 1  //default number of occupied cells (i.e.: number of bugs)
#define BUG_PER_CELL 1 //at most one bug per cell at a time


