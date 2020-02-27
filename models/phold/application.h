#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>

#define MAX_BUFFERS 4000
#define MAX_BUFFER_SIZE 512
#define TAU 1.5
#define SEND_PROBABILITY 0.05
#define ALLOC_PROBABILITY 0.2
#define DEALLOC_PROBABILITY 0.2

// Event types
enum {
	LOOP = INIT + 1,
	RECEIVE
};

#define COMPLETE_EVENTS 10000	// for the LOOP traditional case
#define LOOP_COUNT	1000

// This structure defines buffers lists
typedef struct _buffer {
	unsigned count; // the number of elements of data
	struct _buffer *next;
	unsigned *data; //synthetic data array
} buffer;

// LP simulation state
typedef struct _lp_state_type {
	unsigned int events;
	unsigned buffer_count;
	unsigned total_checksum;
	buffer *head;
} lp_state_type;

buffer* get_buffer(buffer *head, unsigned i);
unsigned read_buffer(buffer *head, unsigned i); //reads synthetic data and performs stupid hash
buffer* allocate_buffer(buffer *head, unsigned *data, unsigned count); //allocates a buffer with @count of elements, initialized with content of @data.
buffer* deallocate_buffer(buffer *head, unsigned i); //deallocate buffer at pos i



