#ifndef USER_H_
#define USER_H_

#include <stdio.h>

#include <ROOT-Sim.h>
#include "jsmn.h"
#include "agent.h"

/**
 * In order to successfully run a meaningful Agent Based Simulation
 * you are required to fill in this header and the corresponding source user.c
 *
 */

#define NAME_LENGTH 64
#define MAX_USER_ACTIONS 128

/**
 * This struct will be the mean to keep track of your region's state;
 */
struct _region_data_t {
	char name[NAME_LENGTH];			//! A glorious example of the flexibility offered by this interface
	/* FILL IN! */
};

/**
 * This struct will be the data your regions will share with their neighbours.
 * THIS CAN'T HAVE NON NULL POINTERS, this stuff will travel around machines!
 * You will be periodically be asked to fill this struct (callback user_compile_neighbour_state()),
 * the system will automagically update neighbours.
 */
struct _neighbour_state_t {
	/* FILL IN! */
};

/**
 * This struct will be the data your agents will carry with them
 * THIS CAN'T HAVE NON NULL POINTERS, this stuff will travel around machines!
 */
struct _agent_data_t {
	char name[NAME_LENGTH];			//! A glorious example of the flexibility offered by this interface
	/* FILL IN! */
};

int user_init_agent(const agent_t *agent, const jsmntok_t *t_d, const char *base);
int user_init_region(const region_t *region, const jsmntok_t *t_d, const char *base);

bool user_done_region(const region_t *region);

int user_on_visit(const region_t *region, const agent_t *agent, simtime_t now);

simtime_t user_residence_time(const region_t *region, const agent_t *agent, simtime_t now);

int user_on_leave(const region_t *region, const agent_t *agent, simtime_t now);

int	user_compile_neighbour_state(const region_t *region, neighbour_state_t *neighbour_state, simtime_t now);

void user_print_region(const region_t* region, FILE *out_stream);
void user_print_agent(const agent_t* agent, FILE *out_stream);

#endif /* USER_H_ */
