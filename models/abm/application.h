#pragma once

#include <ROOT-Sim.h>
#include <stdbool.h>

// Event Types
#define AGENT_IN		1
#define AGENT_OUT		2
#define HEARTBEAT		3
#define	UPDATE_NEIGHBOURS	4


// This determines the percentage of cells which cannot be accessed
#define OBSTACLES_PERCENT 0.1

// Define the type of grid: either TOPOLOGY_HEXAGON or TOPOLOGY_SQUARE
#define TOPOLOGY TOPOLOGY_HEXAGON

// Initial number of cells where there are agents. IDs are defined in application.c
#define NUM_AGENT_CELLS		10

// How many agents in each cell, initially
#define NUM_AGENTS_PER_CELL	2

// How much time does an agent spend in a cell?
#define RESIDENCE_TIME		10


typedef struct _neighbour_state_t {
	size_t num_agents;
} neighbour_state_t;


typedef struct _agent_t {
	unsigned long long uuid;
	size_t visited;
	size_t visit_list_size;
	unsigned int visit_list[]; // TODO: add also visit simulation time
} agent_t;


typedef struct _agent_node_t {
	struct _agent_node_t *next;
	agent_t *agent;
} agent_node_t;


typedef struct _lp_state_type {
	size_t num_agents;
	agent_node_t *agents;
	neighbour_state_t *neighbours;
} lp_state_type;

