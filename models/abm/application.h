#pragma once

#include <ROOT-Sim.h>
#include <stdbool.h>

// Configuration file
#define CONFIG_FILE "config.json"

// Event Types
#define AGENT_IN			1
#define AGENT_OUT			2
#define	AGENT_CHANGE_DEST	3
#define HEARTBEAT			4
#define	UPDATE_NEIGHBOURS	5

// Define the type of grid: either TOPOLOGY_HEXAGON or TOPOLOGY_SQUARE
#define TOPOLOGY TOPOLOGY_HEXAGON

// How much time does an agent spend in a cell?
#define RESIDENCE_TIME		10

#define error(critical, message, ...) {\
	printf("%s:%d: " message, __FILE__, __LINE__, ##__VA_ARGS__);\
	if(critical) exit(EXIT_FAILURE); \
}

#define get_agent_current_region(agent) ((agent)->visit_list[(agent)->visited - 1].region)


typedef struct _neighbour_state_t {
	size_t num_agents;
} neighbour_state_t;


typedef enum _agent_actions_t {
	START_POSITION, //! This is the initial region where the agent is set
	TRAVERSE,		//! This is a region which the agent only crosses: nothing to do there.
	ACTION_A,
	ACTION_B,
	ACTION_C
} agent_actions_t;


typedef struct _visit_list_t {
	simtime_t time;
	unsigned int region;
	agent_actions_t action;
} visit_t;


typedef struct _agent_t {
	unsigned long long uuid;		//! UUID that uniquely identifies the agent
	char name[64];					//! The name of the agent
	size_t visited;					//! The number of the cells visited by the agent in the current path exploration to destination
	size_t visit_list_size;			//! The number of the visist path size
	visit_t visit_list[];			//! The path of the cells to visit in order to get the target
} agent_t;


typedef struct _agent_node_t {
	struct _agent_node_t *next;
	agent_t *agent;
} agent_node_t;


typedef struct _lp_state_t {
	char name[64];
	size_t num_agents;
	agent_node_t *agents;
	neighbour_state_t *neighbours;
} lp_state_t;


/* Function prototypes from all modules */

// From application.h
int add_agent(lp_state_t *state, agent_t *agent);
agent_t *remove_agent(lp_state_t *state, unsigned long long uuid);
agent_t *find_agent(lp_state_t *state, unsigned long long uuid);
agent_t *create_agent(void);
unsigned int compute_agent_path(agent_t **agent, obstacles_t *obstacles);
unsigned int get_agent_current_destination(agent_t *agent);
void print_agent_list(lp_state_t *state);
void send_update_neighbours(simtime_t now, neighbour_state_t *new_event_content);

// From config.c
void region_config(lp_state_t *state, unsigned int me);
void load_config(void);
agent_t *get_next_agent(unsigned int me);
void initialize_obstacles(obstacles_t **obstacles);

