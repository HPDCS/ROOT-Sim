/*
 * FORWARDING ENGINE
 *
 * Its main task is forwarding data packets received from neighbors as well as sending packets created by the node
 * itself. Moreover, it is in charge of detect duplicate packets and routing loops. Finally, it snoops data packets
 * directed to other nodes
 *
 * The forwarding engine has a FIFO queue of fixed depth at its disposal to store packets before forwarding them: here
 * packets are both those coming from other neighbors and those created by the node itself.
 *
 * The forwarding engine waits for an acknowledgment for each packet sent => if this is not received within a certain
 * timeout, it tries to retransmit the packet for a limited number of times: if no acknowledgement is ever received, the
 * packet is discarded.
 *
 * The forwarding engine is also capable of detecting duplicated packets. In fact, the tuple <Origin,SeqNo,THL> identify
 * a packet uniquely => comparing each received data packet with those in the forwarding queue, it is possible to say
 * whether a packet is duplicated or not. Also the engine maintains a cache of the last four transmitted packets so
 * duplicates can be detected even when they are not in the input queue
 *
 * Another interesting feature of the forwarding engine is the detection of ROUTING LOOPS.
 * This is achieved by comparing the ETX reported in the received packet with the ETX of the current node: the former
 * has to be strictly higher than the latter, because the ETX of a node is inductively defined as the quality of the
 * link to the parent plus the ETX of the parent itself, with the root node having ETX=0 => if the above check is not
 * passed, the forwarding engine resets the beacon interval and sets the PULL FLAG of the data packets in order to
 * force a topology update that will hopefully fix the loop; also it stops forwarding data packets for a LOOP interval,
 * which is the time needed to repair the LOOP.
 */

#include "application.h"
#include "link_layer.h"
#include "parameters.h"


/* FORWARD DECLARATIONS */

void cache_remove(unsigned char offset,node_state* state);

extern node_statistics* node_statistics_list;

/* FORWARDING POOL - start */

/*
 * FORWARDING POOL - GET ENTRY
 *
 * Get an entry from the pool; it's the one at the position indicated by the variable "forwarding_pool_index"
 *
 * @state: pointer to the object representing the current state of the node
 *
 * Returns a pointer to the entry of the pool, NULL if the pool is empty
 */

forwarding_queue_entry* forwarding_pool_get(node_state* state){

        /*
         * If the pool is empty, return NULL
         */

        if(!state->forwarding_pool_count)
                return NULL;

        /*
         * The return value: the entry corresponding to element "index" of the pool
         */

        forwarding_queue_entry* entry=&state->forwarding_pool[state->forwarding_pool_index];

        /*
         * Set the "index" variable to the next available entry of the pool
         */

        state->forwarding_pool_index+=1;

        /*
         * Reduce by one the counter of entries in the pool
         */

        state->forwarding_pool_count-=1;

        /*
         * If "index" is now beyond the limit of the pool, set it to the first position
         */

        if(state->forwarding_pool_index==FORWARDING_POOL_DEPTH)
                state->forwarding_pool_index=0;

        /*
         * Return the entry
         */

        return entry;
}

/*
 * FORWARDING POOL - PUT ENTRY
 *
 * Add an entry to the pool in the first available position
 *
 * @entry: pointer to the "forwarding_queue_entry" representing the element to be put in the pool
 * @state: pointer to the object representing the current state of the node
 */

void forwarding_pool_put(forwarding_queue_entry* entry,node_state* state){

        /*
         * Index of the first free place where the entry will be stored
         */

        unsigned char index;

        /*
         * Check if the pool is full: an entry can be added only if not full
         */

        if(state->forwarding_pool_count<FORWARDING_POOL_DEPTH){

                /*
                 * Get the index of a free position in the pool where the entry can be stored
                 */

                index=state->forwarding_pool_count+state->forwarding_pool_index;

                /*
                 * If the index is beyond the limit of the pool, correct it
                 */

                if(index>=FORWARDING_POOL_DEPTH)
                        index-=FORWARDING_POOL_DEPTH;

                /*
                 * Put the given entry in the first free place
                 */

                state->forwarding_pool[index]=*entry;

                /*
                 * Increase by one the counter of elements in the pool
                 */

                state->forwarding_pool_count+=1;
        }

}

/* FORWARDING POOL - end */

/* FORWARDING QUEUE - start */

/*
 * FORWARDING QUEUE - ENQUEUE ELEMENT
 *
 * When a new element has to be enqueued, it's inserted at the position specified by the variable "tail", because the
 * queue has a FIFO logic. After the element has been inserted, the "tail" variable is incremented, so the the next
 * queued element will be inserted after the current element; also the counter of the elements in the queue is
 * incremented.
 *
 * @entry: pointer to the "forwarding_queue_entry" representing the element to be enqueued
 * @state: pointer to the object representing the current state of the node
 *
 * Returns true if the element is successfully enqueued, false otherwise
 */

bool forwarding_queue_enqueue(forwarding_queue_entry* entry,node_state* state){

        /*
         * Check if there's free space in the queue
         */

        if(state->forwarding_queue_count<FORWARDING_QUEUE_DEPTH){

                /*
                 * There's enough space in the queue for at least one new element => insert the new element at position
                 * determined by the "tail" variable
                 */

                state->forwarding_queue[state->forwarding_queue_tail]=entry;

                /*
                 * Update the counter for the number of elements in the queue
                 */

                state->forwarding_queue_count+=1;

                /*
                 * Update the position corresponding to the tail of the queue, where the next coming element will be
                 * inserted
                 */

                state->forwarding_queue_tail+=1;

                /*
                 * Check if the tail is now beyond the limit of the queue: if so, reset the position of the tail to 0.
                 * This is mandatory to implement the FIFO logic
                 */

                if(state->forwarding_queue_tail==FORWARDING_QUEUE_DEPTH)
                        state->forwarding_queue_tail=0;

                /*
                 * Packet enqueued => return true
                 */

                return true;
        }

        /*
         * The queue is full, so no entry can be initialized => return false
         */

        return false;
}

/*
 * FORWARDING QUEUE - DEQUEUE ELEMENT
 *
 * Every time an element of the queue is to be removed, the one corresponding to the head of the queue is chosen. The
 * counter of the elements in the queue is decreased, while the position of the head is incremented, so the element that
 * was added after the current one will be chosen next time this function will be invoked.
 *
 * @state: pointer to the object representing the current state of the node
 *
 * NOTE: this function has to be called after a packet has been successfully forwarded  in order to free space in the
 * output queue
 */

void forwarding_queue_dequeue(node_state* state){

        /*
         * Check if there's at least one element in the queue
         */

        if(state->forwarding_queue_count){

                /*
                 * There's at least one new element => set the position of the head to the next element in the queue
                 */

                state->forwarding_queue_head+=1;

                /*
                 * Decrease the counter for the number of elements in the queue
                 */

                state->forwarding_queue_count-=1;

                /*
                 * Check if the head is now beyond the limit of the queue: if so, reset the position of the head to 0.
                 * This is mandatory to implement the FIFO logic
                 */

                if(state->forwarding_queue_head==FORWARDING_QUEUE_DEPTH)
                        state->forwarding_queue_head=0;
        }

        /*
         * No element in the queue => do nothing
         */
}

/*
 * FORWARDING QUEUE - LOOKUP
 *
 * Returns true if the given data packet is in the output queue, i.e. it is waiting for other packets to be sent, false
 * otherwise
 *
 * @data_frame: the data frame of the packet to be looked up
 * @forwarding_queue: the forwarding queue of the current node
 * @count: number of queued packets
 */

bool forwarding_queue_lookup(ctp_data_packet_frame* data_frame,forwarding_queue_entry* forwarding_queue[],
                             unsigned char count){

        /*
         * Index used to iterate through the packets in the cache
         */

        unsigned char i;

        /*
         * Scan the output queue until an item matching the searched packet is found
         */

        for(i=0;i<count;i++){

                /*
                 * The data frame of the element of the output queue analyzed
                 */

                ctp_data_packet_frame current=forwarding_queue[i]->packet.data_packet_frame;

                /*
                 * If the current element matches the given packet return true
                 */

                if(data_frame->THL==current.THL &&
                   data_frame->origin==current.origin &&
                   data_frame->seqNo==current.seqNo)

                        return true;
        }

        /*
         * The searched packets has not been found => return false
         */

        return false;
}

/* FORWARDING QUEUE - end */

/* OUTPUT CACHE - start */

/*
 * OUTPUT CACHE - LOOKUP
 *
 * Returns the index of the given data packet if it's in the output cache, i.e. it has recently been sent, and the
 * counter of elements in the cache otherwise
 *
 * @data_frame: the data frame of the packet to be looked up
 * @state: pointer to the object representing the current state of the node
 *
 * NOTE: the index is returned as offset from the position of the least recently added element of the cache
 */

unsigned char cache_lookup(ctp_data_packet_frame* data_frame,node_state* state){

        /*
         * Variable used to iterate through the packets in the cache
         */

        unsigned char i;

        /*
         * Index of the current entry during the iteration
         */

        unsigned char index;

        /*
         * Scan the output cache until an item matching the searched packet is found
         */

        for(i=0;i<state->output_cache_count;i++){

                /*
                 * Get the index of the entry
                 */

                index=(state->output_cache_first+i)%(unsigned char)CACHE_SIZE;

                /*
                 * The data frame of the element of the cache analyzed
                 */

                ctp_data_packet_frame current=state->output_cache[index].data_packet_frame;

                /*
                 * If the current element matches the given packet return true
                 */

                if(data_frame->THL==current.THL &&
                   data_frame->origin==current.origin &&
                   data_frame->seqNo==current.seqNo)

                        break;
        }

        /*
         * Return the index of the entry corresponding to given packet, the counter of elements in the cache otherwise
         */

        return i;
}

/*
 * OUTPUT CACHE - ENQUEUE
 *
 * Adds a new entry in the output cache.
 * If there's no space left for the packet, since the cache adopts a LRU logic, the least recently inserted packet is
 * removed from the cache to free space for the new packet to be inserted
 *
 * @data_frame: the data frame of the packet to be inserted
 * @state: pointer to the object representing the current state of the node
 */

void cache_enqueue(ctp_data_packet_frame* data_frame,node_state* state){

        /*
         * Index of the entry of the cache corresponding to the given packet
         */

        unsigned char i;

        /*
         * Pointer to the data frame of the element of the cache analyzed
         */

        ctp_data_packet_frame* new_data_frame;

        /*
         * Check whether the cache is full
         */

        if(state->output_cache_count==CACHE_SIZE){

                /*
                 * The output cache is full => remove the least recently inserted packet from it
                 * Check if the packet is already in the cache
                 */

                i=cache_lookup(data_frame,state);

                /*
                 * If the packet was not already in the cache, "i" is set to the number of packets cached => the first
                 * element (least recently inserted) is removed.
                 * If the packet was already in the cache, "i" is set to its offset w.r.t. the least recently added
                 * element => the packet is removed from the cache before being re-inserted
                 */

                cache_remove(i%state->output_cache_count,state);
        }

        /*
         * Get the data frame of the entry where the most recently accessed element will be put
         */

        new_data_frame=&state->output_cache[(state->output_cache_first+state->output_cache_count)%CACHE_SIZE].
                data_packet_frame;

        /*
         * Set the new entry
         */

        new_data_frame->THL=data_frame->THL;
        new_data_frame->origin=data_frame->origin;
        new_data_frame->seqNo=data_frame->seqNo;

        /*
         * Update the counter of elements in the cache
         */

        state->output_cache_count+=1;
}

/*
 * OUTPUT CACHE - REMOVE
 *
 * Remove the entry in the output cache at given offset with respect the index corresponding to "output_cache_first".
 * This function is called only when an element has to be inserted in the cache and this is full, so after an element is
 * removed, a new element is inserted
 *
 * @state: pointer to the object representing the current state of the node
 */

void cache_remove(unsigned char offset,node_state* state){

        /*
         * Variable used to iterate through existing entries
         */

        unsigned char i;

        /*
         * Check if the given offset is valid, namely it's less than the number of entries in the cache
         */

        if(offset<state->output_cache_count)

                /*
                 * Given index does not correspond to any entry of the cache
                 */

                return;

        /*
         * If the given offset is 0, the packet to be removed is the one that was least recently added, which is at
         * position indicated by the variable "output_cache_first" => the element that is about to be inserted is going
         * to replace this element.
         * It is necessary to shift "output_cache_first" by 1, so that the next element removed will always be the least
         * recently accessed one.
         */

        if(!offset) {
                state->output_cache_first+=1;
                state->output_cache_first = (state->output_cache_first) % (unsigned char)CACHE_SIZE;
        }
        else{

                /*
                 * The element to be removed is not the least recently accessed one: this happens when an element that
                 * is already in the cache has to be inserted again, namely it is accessed again => it has to be moved
                 * to indicate that is the most recently accessed element of the cache; also the least recently accessed
                 * element has to be removed
                 *
                 * In order to do this, all the elements of the cache are shifted backward by one position, without
                 * changing the value of "output_cache_first" => the most recently accessed element will be inserted
                 * before the least recently accessed one, pointed by the "output_cache_first" variable
                 */

                for(i=offset;i<state->output_cache_count;i++){
                        memcpy(&state->output_cache[(offset+i)%CACHE_SIZE],&state->output_cache[(offset+i+1)%CACHE_SIZE]
                                ,sizeof(ctp_data_packet));
                }
        }

        /*
         * Decrease by one the counter of packets in the cache
         */

        state->output_cache_count-=1;
}

/* OUTPUT CACHE - end */

/*
 * SCHEDULE NEW SENDING
 *
 * Set the retransmission timer to a value calculated with some randomness and schedule a new sending phase when the
 * timer is fired. The value of the timer is randomly selected in the range [delta,interval-1+delta].
 * The FORWARDING ENGINE keep retransmitting a packet until it's successfully submitted to the link layer and, when this
 * happens, keeps retransmitting for a maximum number of attempts
 *
 * @state: pointer to the object representing the current state of the node
 * @interval: value of the desired interval
 * @delta: value of the desired delta
 */

void schedule_retransmission(node_state* state){

        /*
         * Schedule the new sending after an interval of time whose length is randomly selected in the range
         * [delta,interval-1+delta].
         * Do calculation in milliseconds
         */

        double interval=(RandomRange((unsigned int)(data_packet_transmission_delta*1000),
                                    (unsigned int)(((data_packet_transmission_delta+
                                            data_packet_transmission_offset)*1000)-1)))/1000.0;
        /*printf("Node %d schedules retransmission at time %f\n",state->me,state->lvt+interval);
        printf("TIME:%f\n",state->lvt);
        printf("ìììììììì\n");
        fflush(stdout);*/
        wait_until(state->me,state->lvt+interval,RETRANSMITT_DATA_PACKET);
}

/*
 * START FORWARDING ENGINE
 *
 * This function is in charge of initializing the variables of the forwarding engine and to start a periodic timer that
 * sends data packets as it gets fired (if not the root node)
 * It is invoked when the node (logical process) is delivered the INIT event.
 *
 * @state: pointer to the object representing the current state of the node
 */

void start_forwarding_engine(node_state* state){

        /*
         * First initialize the forwarding pool
         */

        state->forwarding_pool_count=FORWARDING_POOL_DEPTH;
        state->forwarding_pool_index=0;

        /*
         * Then the forwarding queue
         */

        state->forwarding_queue_count=0;
        state->forwarding_queue_head=0;
        state->forwarding_queue_tail=0;

        /*
         * Then set the sequence number of the first data packet to be sent to 0
         */

        state->data_packet_seqNo=0;

        /*
         * Set the counter of routing loops detected to 0 initially
         */

        state->routing_loops=0;

        /*
         * Set the counter of duplicates to 0 initially
         */

        state->duplicates=0;

        /*
         * Check if it's the root node: if not, schedule the sending of a data packet
         */

        if(!state->root) {

                /*
                 * Start the periodic timer with interval SEND_PACKET_TIMER: every time is fired, the data packet in
                 * the head of the output queue is sent.
                 * The simulator is in charge of re-setting the timer every time it is fired
                 */

                wait_until(state->me, state->lvt + send_packet_timer, SEND_PACKET_TIMER_FIRED);

                /*
                 * Start the periodic timer with interval CREATE_PACKET_TIMER: every time is fired, a new data packet
                 * containing data sampled from sensors is put in the output queue
                 * The simulator is in charge of re-setting the timer every time it is fired
                 */

                wait_until(state->me, state->lvt + create_packet_timer, CREATE_PACKET_TIMER_FIRED);

        }
}

/*
 * SEND DATA PACKET
 *
 * This function is in charge of forwarding the first element of the output queue (the least recently added, because it
 * is a FIFO queue), if any.
 *
 * If the output queue contains at least one packet, the forwarding engine checks that a route exists towards the root
 * of the collection tree: if so, a few parameters are set for sending the packet and then it is sent. The forwarding
 * engine relies on the routing engine for a few pieces of information related to the path of the packet to be forwarded.
 *
 * This function removes packets from the output queue until one that is not a duplicated is found; the corresponding
 * entry of the forwarding queue is not dequeued until the packet gets acknowledged by the intended recipient
 *
 * Returns true if the function has to be invoked again. This happens when the head of the queue is a duplicate: in fact
 * it is removed from the queue, so a further call to the queue can be made to forward the next packet (unless it is a
 * duplicate on its turn).
 *
 * @state: pointer to the object representing the current state of the node
 */

bool send_data_packet(node_state* state) {

        /*
         * Value of the ETX of the current route
         */

        unsigned short etx;

        /*
         * Pointer to the entry of the forwarding queue that currently occupies the head position
         */

        forwarding_queue_entry* first_entry;

        /*
         * ID of the recipient of the data packet, namely the the current parent => the forwarding engine asks the
         * routing engine about the identity of the current parent node
         */

        unsigned int parent;

        /*
         * Boolean variable telling whether the data packet has been successfully submitted to the underlying LINK
         * LAYER
         */

        bool submitted;

        /*
         * Check if there at least one packet to forward; when the output queue is empty, its "counter" is set to 0
         */

        if (!state->forwarding_queue_count){

                /*
                 * Output queue is empty => return false because a further invocation will be of no help
                 */

                return false;
        }

        /*
         * Check if the node has already submitted another data packet to the link layer: if so, it has to wait
         * before submitting a new packet
         */

        if(state->state&SENDING_DATA_PACKET) {
                return false;
        }

        /*
         * The output queue is not empty and the node is not sending another packet.
         * Check if the node has a valid route => ask the routing engine the ETX corresponding to the current route of
         * the node
         */

        if ((!get_etx(&etx,state))) {

                /*
                 * The function "get_etx" returns false if the parent of the node is not valid => if this is the case,
                 * it means the route of the node is not valid => schedule a new forwarding attempt after an interval of
                 * time equal to NO_ROUTE_RETRY; during this time, hopefully the node has fixed its route.
                 */

                wait_until(state->me,state->lvt+no_route_offset,RETRANSMITT_DATA_PACKET);

                /*
                 * Return false, since an immediate further invocation will be of no help: it is necessary to wait at
                 * least an interval of time equal to NO_ROUTE_RETRY
                 */

                return false;
        }

        /*
         * The node has a valid route (parent) => before sending the head packet, check that it's not duplicated.
         * Get a pointer the to entry corresponding to the head of the output queue
         */

        first_entry=state->forwarding_queue[state->forwarding_queue_head];

        /*
         * Perform the check on the packet corresponding to the selected entry of the queue
         */

        if(cache_lookup(&first_entry->packet.data_packet_frame,state)<state->output_cache_count){

                /*
                 * The data packet is already in the output cache => is a duplicate => remove the entry of the current
                 * packet from the output queue...
                 */

                forwarding_queue_dequeue(state);

                /*
                 * ...and give it back to forwarding pool
                 */

                forwarding_pool_put(first_entry,state);

                /*
                 * Now that the duplicated has been removed from the forwarding queue, return true because the new head
                 * of the queue may not be a duplicate
                 */

                return true;
        }

        /*
         * The packet is not a duplicate => it can be forwarded.
         * Set the ETX field of the data frame
         */

        first_entry->packet.data_packet_frame.ETX=etx;

        /*
         * Clear PULL flag from the packets to be forwarded
         */

        first_entry->packet.data_packet_frame.options&=~CTP_PULL;

        /*
         * Check if the node is congested: if so, set the flag in the packet, otherwise clear the flag
         */

        if(is_congested(state))
                first_entry->packet.data_packet_frame.options |= CTP_CONGESTED;
        else
                first_entry->packet.data_packet_frame.options&=~CTP_CONGESTED;

        /*
         * Get the ID and coordinates of the recipient (parent node) from the routing engine
         */

        parent=get_parent(state);

        /*
         * Set the "src" and "dst" fields of the physical overhead of the packet
         */

        first_entry->packet.link_frame.src=state->me;
        first_entry->packet.link_frame.sink=parent;

        /*
         * Forward the data packet to the specified destination => call the dedicated function from the link layer
         */

        submitted=send_frame(state,parent,CTP_DATA_PACKET);

        /*
         * If the packet has been successfully submitted, set the flag SENDING_DATA_PACKET
         */

        if(submitted)
                state->state|=SENDING_DATA_PACKET;

        /*
         * The data packet has been sent => no need to re-execute this function
         */

        return false;
}

/*
 * CREATE DATA PACKET
 *
 * Create a well-formed data packet (see its definition in "application.h") out of a payload (the data that node has
 * collected and is willing to deliver to the root of the collection tree) and add it to the forwarding queue.
 * The payload here is simulated as a random value within a given range.
 * Packets in the forwarding queue are forwarded, one at a time, according to a FIFO logic => after having queued up
 * a new packet, forward the one that currently is at the head of the queue.
 * This function can't be invoked by the root node, because this only collects data from the collection tree
 *
 * NOTE a node sends one data packet at a time => after the last created data packet has been acknowledged, a new packet
 * can be created. In other words, there's a data packet created by the node at a time in the forwarding queue
 *
 * @state: pointer to the object representing the current state of the node
 */

void create_data_packet(node_state* state){

        /*
         * Pointer to the data frame of the next data packet to send
         */

        ctp_data_packet_frame *data_frame;

        /*
         * Check if the last data packet created by the node has already been enqueued: if not, wait before creating a
         * new one, otherwise the former would be overwritten
         */

        //if(!state->sending_local_data_packet) {
        if(!(state->state&SENDING_LOCAL_DATA_PACKET)) {

                /*
                 * Set the payload of the data packet to be sent
                 */

                state->data_packet.payload = RandomRange(min_payload, max_payload);

                /*
                 * Get the data frame from the data packet to be sent
                 */

                data_frame = &state->data_packet.data_packet_frame;

                /*
                 * Set the fields of the data frame related to forwarding.
                 * Start with origin, which has to be set to the ID of the current node
                 */

                data_frame->origin = state->me;

                /*
                 * Then set the sequence number
                 */

                data_frame->seqNo = state->data_packet_seqNo;

                /*
                 * Update the value of the sequence number for the next data packet
                 */

                state->data_packet_seqNo += 1;

                /*
                 * Finally set the THL (Time Has Lived) field to 0, because the packet has been just created
                 */

                data_frame->THL = 0;

                /*
                 * Check if there's at least one free entry in output queue to send the new packet => if this, is the
                 * case, the variable "forwarding_queue_count" is less than the depth of the queue
                 */

                if (state->forwarding_queue_count < FORWARDING_QUEUE_DEPTH) {

                        /*
                         * The function that is in charge of actually sending the packet, works as follows;
                         * 1-gets the head entry in the forwarding queue
                         * 2-extracts a packet from it
                         * 3-sends it to the parent node.
                         *
                         * If the head of the queue is a duplicate packet, it is removed => in this case the function
                         * returns true, meaning that it has to be invoked again to send the next packet in the queue
                         * => the variable below is set to true if a new sending attempt has to be made
                         */

                        bool try_again;

                        /*
                         * There's free space in the forwarding queue => initialize the entry for the packet to be
                         * queued up. Initialize the dedicated pointer to the data packet the entry corresponds to
                         */

                        state->local_entry.packet = state->data_packet;

                        /*
                         * Set the number of transmission attempt to its maximum value: every time a transmission fails,
                         * this counter is decreased and when it's equal to 0 the packet is dropped
                         */

                        state->local_entry.retries = (unsigned char)max_retries;

                        /*
                         * Set the flag indicating that this node created the packet to be sent
                         */

                        state->local_entry.is_local = true;

                        /*
                         * Insert the entry for the packet to be sent in the forwarding queue: we have already seen that
                         * the queue is not full, so we don't further check the return value of the following call
                         */

                        forwarding_queue_enqueue(&state->local_entry, state);

                        /*
                         * Set the guard flag because the packet created is now in the forwarding queue
                         */

                        //state->sending_local_data_packet=true;
                        state->state|=SENDING_LOCAL_DATA_PACKET;

                        /*
                         * Now check the state of the node: if another data packet has already been submitted to the
                         * data link layer, it's necessary to wait, otherwise extract the packet from the head of the
                         * output queue and send it to the parent node
                         */

                        if(!(state->state&SENDING_DATA_PACKET)){

                                /*
                                 * A new data packet can be sent => call the sending function the first time
                                 */

                                try_again = send_data_packet(state);

                                /*
                                 * If "try_again" is "true", it means that a new sending attempt is necessary => keep
                                 * trying until a valid (not duplicate) packet is found in the output queue or this is
                                 * empty (in either case,"try_again" is set to "false" and the loop ends)
                                 */

                                while (try_again)
                                        try_again = send_data_packet(state);
                        }
                }
        }
}

/*
 * RECEIVED DATA PACKET
 *
 * This is a callback function invoked by the LINK LAYER to tell the FORWARDING ENGINE that a data packet has been
 * received => this function processes the message. If this node is the root node, it simply stores the packet received,
 * otherwise it forwards the packet to its parent.
 * A check is made to detect if the message is duplicated => we check both the output queue and the cache with the most
 * recently forwarded packets, hence it is possible to detect duplicates also after they have been sent
 *
 * @message: the payload from the content of the event delivered to the node
 * @state: pointer to the object representing the current state of the node
 */

void received_data_packet(void* message,node_state* state) {

        /*
         * Update statistics about data packets received (and acked) by the node
         */

        node_statistics_list[state->me].data_packets_received+=1;

        /*
         * Parse the buffer received to a data packet
         */

        ctp_data_packet* packet = (ctp_data_packet *) message;

        /*
         * Increment the THL, because the packet is being forwarded by the current node
         */

        packet->data_packet_frame.THL+=1;

        /*
         * Now check if this is a duplicated one => first check if it has been already transmitted, looking for it in
         * the output cache
         */

        if (cache_lookup(&packet->data_packet_frame, state) < state->output_cache_count) {

                /*
                 * The received message has been recently sent, so this is a duplicate => drop it.
                 * Also update the counter of duplicates
                 */

                state->duplicates+=1;

                return;

        }

        /*
         * Then check if the packet has been already been inserted in the output queue, meaning that it will be soon
         * forwarded.
         * First check if the queue contains some element
         */

        if (state->forwarding_queue_count){
                if (forwarding_queue_lookup(&packet->data_packet_frame, state->forwarding_queue,
                                            state->forwarding_queue_count)) {

                        /*
                         * The received message is already in the output queue, so this is a duplicate => drop it
                         * Also update the counter of duplicates
                         */

                        state->duplicates+=1;
                        return;

                }
        }

        /*
         * The packet received is not a duplicate => check if the current node is the root of the collection tree
         */

        if(state->root) {

                /*
                 * The current node is the root of the collection tree => the packet reached its intended destination
                 * => schedule a new event in order to signal the reception. This translates into setting some variables
                 * that are read in order to decide whether the simulation has come to and end or not
                 */


                collected_data_packet(packet);
        }
        else{

                /*
                 * Check if the packet is for me: if so, forward it otherwise drop it
                 */

                if(packet->link_frame.sink==state->me) {

                        /*
                         * Forward the data packet received
                         */

                        forward_data_packet(packet, state);
                }
        }
}

/*
 * FORWARD DATA PACKET
 *
 * Forward the given data packet => this means getting an entry from the forwarding pool and adding it to the forwarding
 * queue; as soon as it occupies the head of the queue, it will be sent
 *
 * @packet: pointer to to the packet to be forwarded
 * @state: pointer to the object representing the current state of the node
 */

void forward_data_packet(ctp_data_packet* packet,node_state* state){

        /*
         * Check that the forwarding is not empty: if so, the packet can't be stored in the forwarding pool and it has
         * to be dropped
         */

        if(state->forwarding_pool_count){

                /*
                 * The entry corresponding to the packet to be forwarded
                 */

                forwarding_queue_entry* entry;

                /*
                 * Get the entry from the pool; can't be NULL because we have already checked if the poll is empty or
                 * not
                 */

                entry=forwarding_pool_get(state);

                /*
                 * Initialize the pointer of the entry to the packet received
                 */

                entry->packet=*packet;

                /*
                 * Set the number of retransmissions attempts to MAX_RETRIES: this will be decreased every time a
                 * retransmission fails; if it goes to 0, the corresponding packet is dropped
                 */

                entry->retries=MAX_RETRIES;

                /*
                 * Clear the flag to signal the fact that the packet is a forwarded one (not created by the node)
                 */

                entry->is_local=false;

                /*
                 * Try to add the entry to the forwarding queue
                 */

                if(forwarding_queue_enqueue(entry,state)){

                        /*
                         * The entry has been successfully enqueued.
                         * Now check if there's no routing loop, i.e. the ETX reported in the packet received is less
                         * than or equal to the ETX of this node => if so, if the packet was accepted, the packet may
                         * return to the sender, because this would be selected as parent by the current node => there
                         * would be a never-ending lopp => in order to avoid such loops, nodes drop packets whose ETX is
                         * less than or equal to their ETX.
                         * Ask the ETX of the current route to the ROUTING ENGINE
                         */

                        unsigned short my_etx;

                        /*
                         * Check whether the ROUTING ENGINE is aware of the ETX: if not, skip the check on the loop
                         */

                        if(get_etx(&my_etx,state)){

                                /*
                                 * Now that we know the ETX of the current node, get the one reported in the received
                                 * packet
                                 */

                                unsigned short packet_etx=packet->data_packet_frame.ETX;

                                /*
                                 * Compare the two values,
                                 */

                                if(packet_etx<=my_etx){

                                        /*
                                         * There's the risk that a routing loop exists => ask the ROUTING ENGINE to
                                         * update the route (by setting the maximum beacons frequency) and schedule a
                                         * new sending
                                         */

                                        reset_beacon_interval(state);
                                        schedule_retransmission(state);

                                        /*
                                         * Also update the counter of loops detected by this node
                                         */

                                        state->routing_loops+=1;

                                        return;
                                }
                        }

                        /*
                         * We get here if no loop has been detected or if it has been detected and it has been fixed and
                         * the node is not waiting for the acknowledgment at the moment
                         * => start sending packets in the forwarding queue, so the packet received will be forwarded
                         * sooner or later
                         */

                        send_data_packet(state);
                }

                /*
                 * The forwarding queue is full => return the entry to the message pool; the packet is dropped
                 */

                forwarding_pool_put(entry,state);
        }
}

/*
 * TRANSMITTED DATA PACKET
 *
 * This is a callback function invoked by the LINK LAYER to tell the FORWARDING ENGINE whether the last data packet
 * submitted has been successfully transmitted to the recipient or not
 *
 * @state: pointer to the object representing the current state of the node
 * @result: boolean value set to true if the packet has been successfully transmitted, false otherwise
 */

void transmitted_data_packet(node_state* state,bool result) {

        /*
         * If the packet has not been transmitted, schedule a new sending phase
         */

        if (!result) {

                /*
                 * Schedule the new sending phase adding some randomness
                 */

                state->is_retransmitting=true;
                schedule_retransmission(state);
        }
        else {

                /*
                 * Get a pointer to the head entry of the output queue and to the corresponding packet
                 */

                forwarding_queue_entry *head_entry = state->forwarding_queue[state->forwarding_queue_head];
                ctp_data_packet *head = &head_entry->packet;

                /*
                 * Update statistics about data packets sent by the node
                 */

                node_statistics_list[state->me].data_packets_sent+=1;


                /*
                 * The packet has been successfully transmitted => check if it has been acked, i.e if the packet at
                 * the head of the output queue is the last acknowledged
                 */

                if (compare_data_packets(&head->data_packet_frame, &state->last_packet_acked.data_packet_frame,
                                                 head->payload, state->last_packet_acked.payload) &&
                            head->link_frame.src== state->last_packet_acked.link_frame.src &&
                                head->link_frame.sink== state->last_packet_acked.link_frame.sink) {

                                /*
                                 * The packet has been acknowledged => remove the message from the output queue, so that
                                 * next transmission phase will send the next packet in the output queue
                                 */

                                /*printf("Node %d -> packet acked ->before dequeueing head was %d and state was %d\n",state->me,
                                state->forwarding_queue_head,state->state);
                                printf("TIME:%f\n",state->lvt);
                                printf("//////////\n");
                                fflush(stdout);*/
                                forwarding_queue_dequeue(state);

                                /*
                                 * Remove the SENDING_DATA_PACKET flag
                                 */

                                state->state &= ~SENDING_DATA_PACKET;

                                /*
                                 * Inform the LINK ESTIMATOR about the fact that the recipient acknowledged the data
                                 * packet, since this piece of information is used by the LINK ESTIMATOR to re-calculate
                                 * the outgoing link quality between the current node and the recipient => extract the
                                 * ID of the latter from the last data packet sent
                                 */

                                ack_received(head->link_frame.sink, true, state->link_estimator_table);

                                /*
                                 * If the last packet sent was a forwarded one, insert in the output cache in order to
                                 * avoid duplicates
                                 */

                                if (!head_entry->is_local) {
                                        cache_enqueue(&head_entry->packet.data_packet_frame, state);

                                        /*
                                         * Return the entry of the last sent data packet to the forwarding pool
                                         */

                                        forwarding_pool_put(head_entry, state);
                                }
                                else{

                                        /*
                                         * If the packet was created by the node, clear the flag indicating that a
                                         * local data packet is being sent
                                         */

                                        state->state&=~SENDING_LOCAL_DATA_PACKET;
                                }

                                /*
                                 * Update statistics about data packets sent by the node that have been acked
                                 */

                                node_statistics_list[state->me].data_packets_acked += 1;

                                /*
                                 * Reset the last packet acked
                                 */

                                memset(&state->last_packet_acked, 0, sizeof(ctp_data_packet));
                        }
                else{

                        /*
                         * The packet has not been acknowledged => if the limit of re-transmissions has not been yet
                         * reached for this data packet, try to send the packet again after a random interval.
                         * Before doing this, inform the LINK ESTIMATOR about the fact that the recipient did not
                         * acknowledge the data packet, since this piece of information is used by the LINK ESTIMATOR
                         * to re-calculate the outgoing link quality between the current node and the recipient =>
                         * extract the ID of the latter from the last data packet sent
                         */

                        ack_received(head->link_frame.sink, false, state->link_estimator_table);

                        /*
                         * The outgoing link quality between the current node and the recipient has possibly changed,
                         * so it may be the case that another neighbor is a better parent for this node => in order to
                         * check if this is the case (and eventually choose a new parent) invoke an update of the route
                         * to the ROUTING ENGINE
                         */

                        update_route(state);

                        /*
                         * Check whether the limit of re-transmissions has been reached
                         */

                        if(head_entry->retries) {

                                /*
                                 * It's still possible to retransmit the data packet => schedule a new event, that will
                                 * trigger a new transmission attempt
                                 * First, update the counter of transmission attempts for the packet
                                 */

                                head_entry->retries -= 1;
                                state->is_retransmitting=true;
                                schedule_retransmission(state);
                                return;
                        }
                        else{

                                /*
                                 * This node has already tried to re-transmit the packet for a number of times bigger
                                 * than MAX_RETRIES => it has to give up with this intention => first of all remove the
                                 * packet from the forwarding queue, so that the next packet will be sent in the next
                                 * forwarding phase
                                 */

                                /*printf("Node %d -> packet dropped ->before dequeueing head was %d and state was %d\n",state->me,
                                       state->forwarding_queue_head,state->state);
                                printf("Entry was local?%d\n",head_entry->is_local);
                                printf("TIME:%f\n",state->lvt);
                                printf("//////////\n");
                                fflush(stdout);*/
                                forwarding_queue_dequeue(state);

                                /*
                                 * Remove the SENDING_DATA_PACKET FLAG
                                 */

                                state->state &= ~SENDING_DATA_PACKET;

                                /*
                                 * If the last packet sent was a forwarded one, give the entry back to the pool
                                 */

                                if (!head_entry->is_local) {
                                        forwarding_pool_put(head_entry,state);
                                }
                                else{

                                        /*
                                         * The packet was created by the node, so clear the flag indicating that a local
                                         * data packet is being sent
                                         */

                                        state->state&=~SENDING_LOCAL_DATA_PACKET;
                                }
                        }
                }
        }
}

/*
 * IS THE NODE CONGESTED
 *
 * This function is invoked by the ROUTING ENGINE to get to know whether the node is congested, i.e. half of its
 * forwarding queue is full.
 * The ROUTING ENGINE asks about congestion before sending beacons to neighbors: if it is congested, it sets the flag
 * CONGESTION in the beacons sent => in this way, neighbors are aware that the node is congested and won't add further
 * workload to it by sending data packets
 *
 * @state: pointer to the object representing the current state of the node
 *
 * Returns true if more than half of the forwarding queue is full, false otherwise
 */

bool is_congested(node_state* state){

        /*
         * Get the counter of elements in the forwarding queue
         */

        unsigned char count=state->forwarding_queue_count;

        /*
         * Return true if more than half is full...
         */

        if(count>FORWARDING_QUEUE_DEPTH/2)
                return true;

        /*
         * ...false otherwise
         */

        return false;
}

/*
 * COMPARE DATA PACKETS
 *
 *  Helper function that returns true if two given data packet frames and their respective payloads coincide, false
 *  otherwise
 *
 * @a:pointer to the first packet
 * @b:pointer to the other packet
 * @payload_a: the payload of the first packet
 * @payload_b: the payload of the second packet
 */

bool compare_data_packets(ctp_data_packet_frame* a,ctp_data_packet_frame* b,int payload_a,int payload_b){
        return payload_a==payload_b && a->ETX==b->ETX && a->origin==b->origin && a->seqNo==b->seqNo&&
                a->THL==b->THL&& a->options==b->options;
}
