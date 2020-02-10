#pragma once
#include <stdbool.h>
#include <ROOT-Sim.h>

enum _bug_events_t {
	PRODUCE_FOOD = INIT + 1,
	SPAWN_BUG,
	BUG_LEAVING,
	BUG_DELAYED_VISIT,
	BUG_VISIT,
	BUG_TRAVERSE
};

typedef struct _region_t {
	simtime_t lvt;
	double food_available; 	// cell's amount of food
	double last_bug_size;	// last bug size
	size_t bugs;		// XXX here we count bugs in the cell. Notice that CountAgentsABM() already provides us this information
	 	 	 	// but since we need to track it we have to replicate it here: if basic values of regions are frequently requested
	 	 	 	// for event modeling we can render them available by default without the need of these tricks
	unsigned is_explored;
	unsigned violation;
} region_t;

typedef struct _bug_t {
	double size;
	bool first; // to render the runs consistent in time we render the first spawned bug immortal
} bug_t;


#define TIME_STEP 1.0
#define MAX_FOOD_PRODUCTION_RATE 2
#define MAX_CONSUMPTION_RATE 5
#define REPRODUCTION_SIZE 20
#define CHILD_COUNT 5
#define SURVIVAL_PROBABILITY 95
#define NUM_OCCUPIED_CELLS 1  //default number of occupied cells (i.e.: number of bugs)
#define BUG_PER_CELL 1 //at most one bug per cell at a time


