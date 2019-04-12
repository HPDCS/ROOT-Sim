#include <ROOT-Sim.h>

#define ECS

#define TX_OP_ARRIVAL	250
#define MIN_OP_COUNT	500
#define MAX_OP_COUNT	1000
#define MAX_RS_SIZE		1000
#define MAX_WS_SIZE		1000
#define TOTAL_COMMITTED_TX 3

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
	unsigned int conflicted_tx;
	unsigned int residual_tx_ops;
	int tx_ops_displacement;
	int read_set[MAX_RS_SIZE];
	int write_set[MAX_WS_SIZE];
	int read_set_size;
	int write_set_size;
	simtime_t lvt;
} lp_state_type;




