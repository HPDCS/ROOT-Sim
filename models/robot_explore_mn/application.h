#include <ROOT-Sim.h>

#define DESTINATION 		1		//
#define ENTER 			2
#define EXIT			3
#define PING			4
#define COMPLETE		5

#define AGENT			6
#define REGION			7

#define PERC_REGION		0.50
 
#define DELAY 			120

#define VISITED 		0.95 		// Termination condition

#define MASK 			0x00000001
#define NUM_CHUNKS_PER_BLOCK 	8

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

typedef struct event_content_t {
	unsigned int *visited_regions;
	unsigned int destination;
	unsigned int sender;
} event_t;

typedef struct lp_state_t{
	
	unsigned int type;
	bool complete;	

	//Agent variables
	unsigned int region;
	unsigned int *visited_regions;
	unsigned int visited_counter;	
	
	//Region variables
	void **actual_agent;
	unsigned int agent_counter;
	unsigned int obstacles;	
} lp_state_t;


