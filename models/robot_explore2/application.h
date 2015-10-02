#pragma once
#ifndef _TCAR_H
#define _TCAR_H

#include <ABM.h>
#include <math.h>

/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORME	0
#define ESPONENZIALE 	1

#define DISTRIBUZIONE ESPONENZIALE

#define is_agent(me) (me >= num_cells)

// Topology
#define CELL_EDGES 4

/*
#define MASK 0x00000001LL			// Mask used to check, set and unset bits
#define NUM_CHUNKS_PER_BLOCK (sizeof(int) * 8)

#define CHECK_BIT(A,I) ( A[(int)((int)(I) / NUM_CHUNKS_PER_BLOCK)] & (MASK << (int)(I) % NUM_CHUNKS_PER_BLOCK) )
#define SET_BIT(A,I) ( A[(int)((int)(I) / NUM_CHUNKS_PER_BLOCK)] |= (MASK << (int)(I) % NUM_CHUNKS_PER_BLOCK) )
#define RESET_BIT(A,I) ( A[(int)((int)(I) / NUM_CHUNKS_PER_BLOCK)] &= ~(MASK << (int)(I) % NUM_CHUNKS_PER_BLOCK) )
*/

#define MASK 0x00000001
#define NUM_CHUNKS_PER_BLOCK 8

#define SET_BIT_AT(target, bit) ((target) |= ((MASK) << (bit)))
#define RESET_BIT_AT(target, bit) ((target) &= ~((MASK) << (bit)))
#define CHECK_BIT_AT(target, bit) ((target) & ((MASK) << (bit)))

#define BITMAP_SET_BIT(map, bit) SET_BIT_AT(((unsigned char*)(map))[(int)((int)(bit) / NUM_CHUNKS_PER_BLOCK)], (int)(bit) % NUM_CHUNKS_PER_BLOCK)
#define BITMAP_RESET_BIT(map, bit) RESET_BIT_AT(((unsigned char*)(map))[(int)((int)(bit) / NUM_CHUNKS_PER_BLOCK)], (int)(bit) % NUM_CHUNKS_PER_BLOCK)
#define BITMAP_CHECK_BIT(map, bit) CHECK_BIT_AT(((unsigned char*)(map))[(int)((int)(bit) / NUM_CHUNKS_PER_BLOCK)], (int)(bit) % NUM_CHUNKS_PER_BLOCK)

#define BITMAP_SIZE(size) (((int)(size / NUM_CHUNKS_PER_BLOCK) + 1))	// TODO: alloca 1 byte di troppo
#define BITMAP_NUMBITS(size) (BITMAP_SIZE(size) * NUM_CHUNKS_PER_BLOCK)
#define ALLOCATE_BITMAP(size) (malloc(BITMAP_SIZE(size)))
#define BITMAP_BZERO(map, size) (bzero(((unsigned char*)(map)), BITMAP_SIZE(size)))

extern unsigned int number_of_regions;
extern unsigned int number_of_agents;

typedef struct _event_content_type {
	unsigned int coming_from;
	unsigned int cell;
} event_content_type;

typedef struct _cell_state_type {
	unsigned int *agents;
	unsigned int present_agents;
	unsigned char neighbours;	/// Neighbour cell for each edge
	unsigned char obstacles;	/// Whether the specific edge has an obstacle
} cell_state_type;

typedef struct _map_t {
	bool visited;
	unsigned int neighbours[CELL_EDGES];
} map_t;

typedef struct _agent_state_type {
	unsigned int current_cell;	/// This is the current region where the agent actually is
//      unsigned int current_direction;                 /// TODO: probably is no more used
	unsigned int target_cell;	/// Region's id of the target the agent is pursuing
	unsigned int direction;	/// TODO: probably is no more used
	unsigned int met_robots;	/// Number of robots the agent met so far
	unsigned int visited_cells;	/// Number of regions the agent visited so far
	unsigned char *visit_map;	/// Bit map to traces visited cells
	unsigned char *a_star_map;	/// Bit map to traces dinamyc A* algorithm research
} agent_state_type;



#define REGION_INTERACTION	1

#endif				/* _ANT_ROBOT_H */
