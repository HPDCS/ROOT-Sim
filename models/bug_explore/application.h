#pragma once
#include <stdbool.h>
#include <ROOT-Sim.h>

#define REGION_IN 1
#define REGION_OUT 2
#define UPDATE_NEIGHBOURS 3
#define PRODUCE_FOOD 4
#define REPRODUCE 5
#define TIME_STEP 5.0
#define MAX_FOOD_PRODUCTION_RATE 10
#define MAX_FOOD_CONSUMPTION_RATE 10
#define SURVIVAL_PROBABILITY 95 
#define EXECUTION_TIME 10000000 //dummy: it can be modified with a specific parameter 
#define TOT_REG (int)n_prc_tot
#ifndef NUM__OCCUPIED_CELLS
    #define NUM_OCCUPIED_CELLS 1  //default number of occupied cells (i.e.: number of bugs)
#endif
#ifndef BUG_PER_CELL
    #define BUG_PER_CELL 1 //at most one bug per cell at a time
#endif

extern int total_num_bugs;

typedef struct _event_content_type { 
	    unsigned int cell; //cell's ID
	    int present; //was a bug inside this cell?
	   	double bug_size;	
		int dying;
} event_content_type; 
 
typedef struct _lp_state_type{ 
	    int present; //number of bugs actually present in this cell 
		int neighbour_bugs[4]; //each entry memorizes the number of bugs of the surrounding cells (i.e.: N,S,O,W cells)
		simtime_t lvt; //the lvt of this cell
		unsigned int explored; //was this cell already explored?	
		double bug_size; //how big is the bug?
		double food_availability; //cell's amount of food
		double food_production; //rate at which a cell produces food
		int food_consumption;
} lp_state_type;

