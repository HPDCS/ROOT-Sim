#include <ROOT-Sim.h>

//#define ECS

#define TX_OP_ARRIVAL	250
#define MIN_OP_COUNT	500
#define MAX_OP_COUNT	1000
#define MAX_RS_SIZE	100
#define TOTAL_COMMITTED_TX 10

#define START_TX	10
#define TX_OP		20
#define PREPARE		30
#define COMMIT		40


// struttura rappresentante il contenuto applicativo di un messaggio scambiato tra LP
typedef struct _event_content_type {
	int *read_set_ptr;
	#ifndef ECS
	int read_set[MAX_RS_SIZE];
	#endif
	int size;
	bool second;
	unsigned int from;
} event_content_type;

typedef struct _lp_state_type{
	unsigned int committed_tx;
	unsigned int residual_tx_ops;
	int read_set[MAX_RS_SIZE];
	int read_set_size;
	simtime_t lvt;
} lp_state_type;




