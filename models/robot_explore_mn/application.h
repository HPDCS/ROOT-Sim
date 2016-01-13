#include <ROOT-Sim.h>

#define ECS_TEST
#define TEST_CASE

#define DESTINATION 		1		//Message used by Region to communicate to the Agent the next destination
#define ENTER 			2		//Message used by Agent for communicating its arrival	
#define EXIT			3		//Message useb by Agent for communicating its exit
#define PING			4		//Keep alive of Region
#define COMPLETE		5		//Message sent by the first Agent that covers the required percentage of regions

#define AGENT			6		//Agent opcode
#define REGION			7		//Region opcode

#define PERC_REGION		0.90		//Fraction of LPs that states regions 
 
#define DELAY 			120		//Expeted value for the delay function

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

typedef struct enter_content_t {
	unsigned int agent;			//Sender's Lid
	unsigned char *map; 			//Pointer to the sender's map
} enter_t;

typedef struct exit_content_t {
	unsigned int agent;			//Sender's Lid
	#ifdef ECS_TEST
	unsigned char *map;			//Pointer to the sender's map
	#endif
} exit_t;

typedef struct destination_content_t {
	unsigned int region;			//Id of next region
	#ifndef ECS_TEST
	unsigned char *map;
	#endif
} destination_t;

typedef struct complete_content_t {
	unsigned int agent;			//Id of agent that completes the mission
} complete_t;

typedef struct lp_agent_t{
	unsigned int region;			//Current region
	unsigned char *map;			//Map pointer
	unsigned int count;			//Amount of already visited regions
	bool complete;				//True if it has received the COMPLETE message
}lp_agent_t;

typedef struct lp_region_t{
	
	#ifdef ECS_TEST	
	unsigned char **guests;			//Vector that stores pointers of agent guest map
	#else
	unsigned char *map;
	#endif
	
	unsigned int count;			//Amount of agents inside the region
	unsigned char obstacles;		//Map of obstacles
} lp_region_t;


