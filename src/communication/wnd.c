#ifdef HAS_MPI

#include <communication/wnd.h>
#include <communication/mpi.h>

/* TODO
* the implementation of the linked list used in this module (src/datatypes/list.h)
* requires to specify a lid in order to optimize memory access in a NUMA architecture.
* The linked lists used here need to be accessed by all the threads on the current kernel,
* Thus it does not make sense to specify a specific lid.
*  A fake lid is used as temporary fix!
*/
#define FAKE_LID 0


outgoing_queue* outgoing_queues;
int n_queues = 0;


void outgoing_window_init(void){
	n_queues = n_ker;
	outgoing_queues = rsalloc(n_queues * sizeof(outgoing_queue));
	outgoing_queue* oq;
	int i;
	for(i=0; i<n_queues; i++){
		oq = outgoing_queues+i;
		spinlock_init(&(oq->lock));
		oq->queue = new_list(FAKE_LID, outgoing_msg);
	}
}


void outgoing_window_finalize(void){
	prune_outgoing_queues();
	size_t pending_out_msgs = outgoing_queues_size();
	if(pending_out_msgs > 0)
		printf("Outgoing queues not empty on exit: %zu\n", outgoing_queues_size());
	//TODO should free each queue that was allocate with the new_list call
	rsfree(outgoing_queues);
}


outgoing_msg* allocate_outgoing_msg(void){
	return (outgoing_msg*) list_allocate_node_buffer(FAKE_LID, sizeof(outgoing_msg));
}


bool is_msg_delivered(outgoing_msg* msg){
	return is_request_completed(&(msg->req));
}


void store_outgoing_msg(outgoing_msg * out_msg, unsigned int dest_kid){

	outgoing_queue* oq = &outgoing_queues[dest_kid];

	spin_lock(&(oq->lock));
	list_insert_tail_by_content(oq->queue, out_msg);
	spin_unlock(&(oq->lock));
}


int prune_outgoing_queue(outgoing_queue* oq){
	int pruned = 0;

	spin_lock(&(oq->lock));

	outgoing_msg* msg = list_head(oq->queue);

	// check all the outgoing messages in the queue starting from the
	// head ( the entry with the minimum timestamp ) and delete them
	// if they have been already delivered
	while(msg != NULL && is_msg_delivered(msg)){
		list_delete_by_content(FAKE_LID, oq->queue, msg);
		pruned++;
		msg = list_head(oq->queue);
	}

	spin_unlock(&(oq->lock));

	return pruned;
}


size_t outgoing_queues_size(void){
	int i;
	size_t size = 0;
	for(i=0; i<n_queues; i++){
		size += list_sizeof(outgoing_queues[i].queue);
	}
	return size;
}


int prune_outgoing_queues(void){
	int i;
	int pruned = 0;
	for(i=0; i<n_queues; i++){
		pruned += prune_outgoing_queue(outgoing_queues + i);
	}
	return pruned;
}


#endif
