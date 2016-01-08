#include <ROOT-Sim.h>

#define DESTINATION 	1	//
#define ENTER 		2
#define EXIT		3
#define PING		4

#define AGENT		5
#define REGION		6

#define PERC_REGION	0.80
 
#define DELAY 		120

#define VISITED 	0.95 	// Termination condition

typedef struct event_content_t {
	unsigned char *visited_regions;
	unsigned int destination;
	unsigned int sender;
} event_t;

typedef struct lp_agent_state{
	
	unsigned int type;	

	//Agent variables
	unsigned int region;
	unsigned char *visited_regions;
	unsigned int visited_counter;	
	
	//Region variables
	unsigned char **actual_agent;
	unsigned int agent_counter;
	unsigned char obstacles;	
} region_state;


extern unsigned int get_tot_regions(void);
extern unsigned int random_region(void);
extern unsigned char get_obstacles(void);
extern unsigned int get_region(unsigned int,unsigned char,unsigned int);
