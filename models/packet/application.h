#include <ROOT-Sim.h>

#define PACKET 1 // Event definition
#define DELAY 120
#define PACKETS 1000 // Termination condition

typedef struct event_content_t {
	simtime_t sent_at;
	int *pointer;
	unsigned int sender;
} event_t;

typedef struct lp_state_t{
	int packet_count;
	int *pointer;
} lp_state_t;
