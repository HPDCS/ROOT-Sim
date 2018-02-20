#pragma once

#include <ROOT-Sim.h>
#include <stdbool.h>

// Configuration file
#define CONFIG_FILE "config.json"

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

typedef struct _visit_list_t {
	simtime_t time;
	unsigned int cell;
} visit_t;

typedef struct _agent_t {
	unsigned long long uuid;
	char name[64];
	size_t visited;
	size_t visit_list_size;
	visit_t visit_list[];
} agent_t;


typedef struct _agent_node_t {
	struct _agent_node_t *next;
	agent_t *agent;
} agent_node_t;


typedef struct _lp_state_type {
	char name[64];
	size_t num_agents;
	agent_node_t *agents;
	neighbour_state_t *neighbours;
} lp_state_type;

