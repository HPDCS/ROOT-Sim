#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>


// Distribution values
#define NO_DISTR	0  // Not actually used, but 0 might mean 'false'
#define UNIFORM		1
#define EXPO	 	2

// Default values
#define WRITE_DISTRIBUTION	0.1
#define READ_DISTRIBUTION	0.1
#define MAX_SIZE		1024
#define MIN_SIZE		32
#define OBJECT_TOTAL_SIZE	16000
#define NUM_BUFFERS		3
#define TAU			5
#define COMPLETE_ALLOC		5000

// Event types
#define ALLOC		1
#define DEALLOC 	2
#define LOOP		3

#define COMPLETE_EVENTS 10000	// for the LOOP traditional case
#define LOOP_COUNT	1000


// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// This structure defines buffers lists
typedef struct _buffers {
	char *buffer;
	struct _buffers *next;
	struct _buffers *prev;
} buffers;


// LP simulation state
typedef struct _lp_state_type {
	bool traditional;
	unsigned int events;
	int loop_counter;


	unsigned int cont_allocation;
	unsigned int num_elementi;
	int actual_size;
	int total_size;
	int next_lp;
	int *taglie;
	int *elementi;
	buffers **head_buffs;
	buffers **tail_buffs;
} lp_state_type;


void read_op(lp_state_type *pointer);
void write_op(lp_state_type *pointer);
void allocation_op(lp_state_type * pointer, int idx);
int deallocation_op(lp_state_type * pointer);

extern int	object_total_size,
		timestamp_distribution,
		max_size,
		min_size,
		num_buffers,
		read_correction,
		write_correction;
extern unsigned int 
		complete_alloc;
extern double	write_distribution,
		read_distribution,
		tau;

