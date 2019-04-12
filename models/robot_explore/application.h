#pragma once
#ifndef _TCAR_H
#define _TCAR_H


#include <ROOT-Sim.h>
#include <math.h>

#define TIME_STEP 5

#define ROBOTS 2

enum distribution_code{
	UNIFORM,
	EXPONENTIAL
};

#define DISTRIBUZIONE EXPONENTIAL

enum event_code{
	KEEP_ALIVE = INIT + 1,
	REGION_IN,
	REGION_OUT,
	NEW_ROBOT,
	_TRAVERSE
};

typedef struct _cell_state_t{
	unsigned int neighbours[6];
	unsigned int present_agents;
	bool has_obstacles, started;
	double max_ratio;
} cell_state_t;


typedef struct _map_t {
	bool visited, a_star_f;
	unsigned int neighbours[6];
} map_t;


typedef struct _agent_state_type {
	unsigned int current_cell;
	unsigned int current_direction;
	unsigned int target_cell;
	unsigned int direction;
	unsigned int met_robots;
	unsigned int visited_cells;
	map_t visit_map[];
} agent_state_type;


#endif /* _ANT_ROBOT_H */
