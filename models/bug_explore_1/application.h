#pragma once
#include <stdbool.h>
#include <ROOT-Sim.h>

#define CELL_IN 1
#define CELL_OUT 2
#define UPDATE_NEIGHBOURS 3
#define PRODUCE_FOOD 4
#define REPRODUCE 5
#define TIME_STEP 5.0
#define MAX_FOOD_PRODUCTION_RATE 10
#define MAX_FOOD_CONSUMPTION_RATE 10
#define SURVIVAL_PROBABILITY 95 
#define EXECUTION_TIME 10000000 //dummy: it can be modified with a specific parameter 
#define TOT_CELLS (int)n_prc_tot
#ifndef NUM__OCCUPIED_CELLS
    #define NUM_OCCUPIED_CELLS 1  //default number of occupied cells (i.e.: number of bugs)
#endif
#ifndef BUG_PER_CELL
    #define BUG_PER_CELL 1 //at most one bug per cell at a time
#endif


typedef struct _bug_t{
	int uuid;
	size_t visited;
	int cells[];
} bug_t;

typedef struct _bug_node_t{
	struct _bug_node_t *next;
	struct _bug_t *bug;
} bug_node_t;

typedef struct _cell_state_t{
	size_t bugs;
	int neighbours[4];
	bug_node_t *bug_list;
} cell_state_t;
