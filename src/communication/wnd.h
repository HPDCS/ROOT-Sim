#ifndef _WND_H_
#define _WND_H_

#ifdef HAS_MPI


#include <mpi.h>

#include <datatypes/list.h>
#include <mm/dymelor.h>
#include <ROOT-Sim.h>
#include <arch/atomic.h>


typedef struct _outgoing_msg {
	MPI_Request req;
	msg_t *msg;
} outgoing_msg;


typedef struct _outgoing_queue {
	spinlock_t lock;
	list(outgoing_msg) queue;
} outgoing_queue;


void outgoing_window_init(void);
void outgoing_window_finalize(void);
outgoing_msg* allocate_outgoing_msg(void);
bool is_msg_delivered(outgoing_msg* msg);
void store_outgoing_msg(outgoing_msg* out_msg, unsigned int dest_kid);
int prune_outgoing_queue(outgoing_queue* oq);
int prune_outgoing_queues(void);
simtime_t min_timestamp_outgoing_msgs(void);
size_t outgoing_queues_size(void);


#endif /* HAS_MPI */
#endif /* _WND_H_ */
