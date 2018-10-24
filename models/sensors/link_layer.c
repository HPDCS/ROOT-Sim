#include <math.h>
#include "link_layer.h"
#include "physical_layer.h"
#include "parameters.h"

/*
 * LINK LAYER
 *
 * This layer simulates the lowest component of the software stack featured by each node and it performs various tasks:
 *
 * 1 - encapsulates data packets from upper layers into link-layer frames, setting a number of header fields specified
 *     by the link-layer protocol
 * 2 - by mean of the Medium Access Control (MAC) protocol, it decides how the frame has to be transmitted onto the link
 *     (depending if it's point-to-point or broadcast)
 * 3 - guarantees reliable delivery of packets across the wireless link
 * 4 - the signal degradation and the electromagnetic noise may cause the variations of some bits of the frames => the
 *     link layers ensures that these are detected and fixed
 *
 * Since wireless links are BROADCAST LINKS, the link layer is in charge of dealing with the problem of multiple access
 * to the channel => this simulation of the link layer adopts the Carrier Sense Multiple Access with Collision Detection
 * (CSMA/CD) protocol to solve this problem:
 *
 * 1 - the node waits for the channel to be free, then starts to send a frame
 * 2 - if the node detects that another node is sending a frame while it's sending its own frame, it stops the sending
 * 3 - after having aborted the transmission, the node waits a random time before start sending again: this time is the
 *     so called "backoff time".
 *
 * The backoff time adopted here is the BINARY EXPONENTIAL BACKOFF TIME adopted in the Ethernet protocol. If a node has
 * already experienced n collisions, it chooses a random K in [0,1,2...2^n-1] => it waits time equal to K*512 before
 * transmitting again.
 * In the implementation of the protocol used here, the nodes wait for a number of times where it sees the channel free
 * equal to CSMA_MIN_FREE_SAMPLES before starting to send a frame; also there's an upper bound for the number of times
 * the node checks if the channel is free (CSMA_MAX_FREE_SAMPLES).
 * The initial value for the backoff time is selected in the range [CSMA_INIT_LOW,CSMA_INIT_HIGH]
 */

extern node_statistics* node_statistics_list;

/*
 * INITIALIZE THE LINK LAYER
 *
 * Sets the variables related to the link layer in the state object to their initial values
 *
 * @state: pointer to the object representing the current state of the node
 */

void init_link_layer(node_state* state){

        /*
         * Set the pointer to the frame being sent to NULL
         */

        state->link_layer_outgoing=NULL;

        /*
         * Set the type for the the frame being sent to
         */

        state->link_layer_outgoing_type=0;

        /*
         * Set the source field in the link frame of both the beacon and the data packet to the ID of current node
         */

        state->data_packet.link_frame.src=state->me;
        state->routing_packet.link_frame.src=state->me;
}

/*
 * GET FRAME
 * The LINK ESTIMATOR and the FORWARDING ENGINE are "wired" to the LINK LAYER using the same interface for sending
 * different types of packets, brodcast messages and unicast messages (that require an acknowledgment) respectively:
 * they only provide the type of the packet to be transmitted, so the link layer has to set the pointer to the link
 * layer frame of the next packet to be transmitted => this function performs this task
 *
 * @state: pointer to the object representing the current state of the node
 * @type: the type of the packet to be transmitted (either CTP_BEACON or CTP_DATA_PACKET)
 *
 */

void get_frame(node_state* state,unsigned char type){

        /*
         * Check the type of the packet
         */

        if(type==CTP_BEACON){

                /*
                 * The packet is a beacon => set link_layer_outgoing to the link layer frame of beacon of the node
                 */

                state->link_layer_outgoing=&state->routing_packet.link_frame;
                /*printf("Node %d has state with address is %p\n",state->me,
                       state);
                printf("TIME:%.15f\n",state->lvt);
                printf("Address of link layer of routing packet for node %d is %p\n",state->me,
                       state->link_layer_outgoing);
                printf("**********\n");
                fflush(stdout);*/
        }
        else{

                /*
                 * The packet is a data packet => set link_layer_outgoing to the link layer frame of the head of the
                 * output queue; also save the type in the corresponding variable of the state
                 */

                state->link_layer_outgoing=&state->forwarding_queue[state->forwarding_queue_head]->packet.link_frame;
                /*printf("Address of state for node %d is %p\n",state->me,
                       state);
                printf("TIME:%.15f\n",state->lvt);
                printf("Address of link layer of data packet for node %d is %p\n",state->me,
                       state->link_layer_outgoing);
                printf("**********\n");
                fflush(stdout);*/
        }
}

/*
 * START THE CSMA/CD PROTOCOL
 *
 * This function simulates the execution of the CSMA/CD protocol
 *
 * @state: pointer to the object representing the current state of the node
 */

void start_csma(node_state* state){

        /*
         * Virtual time of the moment when the node will first check if the channel is free
         */

        simtime_t first_sample;

        /*
         * The backoff time: first set it to a random value in the range [CSMA_INIT_LOW,CSMA_INIT_HIGH]
         */

        simtime_t backoff=(double)RandomRange(csma_init_low,csma_init_high);

        /*
         * The backoff time is in terms of symbols => divide by the number of symbols per second to get the back off
         * time in seconds
         */

        backoff/=(double)csma_symbols_per_sec;

        /*
         * Set the virtual time when the node will first check whether the channel is free: it has to wait a time equal
         * to the backoff time
         */

        first_sample=state->lvt+backoff;

        /*
         * Schedule a new event to tell this node to check whether the channel is free after the backoff time
         */

        /*printf("Node %d schedules check channel at time %f\n",state->me,first_sample);
        printf("The state is %d\n",state->state);
        printf("Link layer:%p\n",state->link_layer_outgoing);
        printf("++++++++\n");
        fflush(stdout);*/
        wait_until(state->me,first_sample,CHECK_CHANNEL_FREE);
}

/*
 * CHECK CHANNEL
 *
 * This is the handler of the CHECK_CHANNEL_FREE event: after a backoff time, the link layer has to check whether the
 * channel is free or not and, depending on the outcome of this check and on the parameters chosen for the CSMA/CD
 * protocol, it decides what to do next.
 * In particular the node has to "see" the channel free for a number of consecutive times equal to CSMA_MAX_FREE_SAMPLES:
 * as soon as this holds, or when CSMA_MAX_FREE_SAMPLES is set to zero, the node starts to transmit the frame.
 * In case the node sees that the channel is busy, it backs off, i.e. it recomputes the value of the backoff time and
 * schedules a new check when such a time has elapsed (unless the maximum number of backoffs has already been reached).
 *
 * @state: pointer to the object representing the current state of the node
 */

void check_channel(node_state* state){

        /*
         * Check if the packet to be sent is still valid: if not, clear all the flags of the node and stop
         */

        if(!state->link_layer_outgoing){
                /*printf("NULL node %d, time %.15f\n",state->me,state->lvt);
                fflush(stdout);*/
                state->state&=~SENDING_BEACON;
                state->state&=~SENDING_DATA_PACKET;
                return;
        }

        /*
         * Increment the counter of backoffs by one
         */

        state->backoff_count+=1;

        /*
         * Check if the channel is free
         */

        if(is_channel_free(state)){

                /*
                 * The channel is free => decrement the number of times the channel is seen as free (required before
                 * sending a packet) by one
                 */

                state->free_channel_count-=1;
        }
        else{

                /*
                 * The channel is not free => reset the counter keeping track of the number of times the channel has
                 * been seen free, because the packet will be sent only if a number of CONSECUTIVE times when it is
                 * perceived as free is achieved
                 */

                state->free_channel_count=(unsigned char)csma_min_free_samples;
        }

        /*
         * Check if the channel has been free enough times
         */

        if(!state->free_channel_count){

                /*
                 * The link layer is sure enough that the channel is free => the radio transceiver has to be switched
                 * from reception mode to transmission mode => the packet will be transmitted with a delay equal to:
                 *
                 * RXTX_DELAY(in symbols)/SYMBOLS_PER_SEC
                 */

                simtime_t rxtx_switch_delay=(double)csma_rxtx_delay/(double)csma_symbols_per_sec;

                /*
                 * Set the state of the link layer and of the underlying radio to "transmitting"
                 */

                state->link_layer_transmitting=true;

                /*
                 * Set the state of the radio to "transmitting"
                 */

                state->radio_state|=RADIO_TRANSMITTING;

                /*
                 * Schedule a new event for the current node, after the above delay, whose handler will start the
                 * transmission of the packet
                 */

                /*printf("Node %d schedules start transmission at time %.15f\n",state->me,state->lvt+rxtx_switch_delay);
                printf("TIME:%.15f\n",state->lvt);
                printf("The state is %d\n",state->state);
                printf("Link layer:%p\n",state->link_layer_outgoing);
                printf("++++++++\n");
                fflush(stdout);*/
                wait_until(state->me,state->lvt+rxtx_switch_delay,START_FRAME_TRANSMISSION);
        }
        else if(!csma_max_free_samples || state->backoff_count<=csma_max_free_samples){

                /*
                 * The link layer has not collected enough samples of the free channel yet => it backs again off and try
                 * again after a new backoff time or it drops the packet.
                 * If the number of backoffs already performed is below the limit (CSMA_MAX_FREE_SAMPLES) or if there's
                 * no limit at all, the node backs off. This is the case here => being n the number of backoffs already
                 * performed, calculate the new backoff time as random value in the range
                 *
                 * [0,(CSM_HIGH-CSMA_LOW)*CSMA_EXPONENT_BASE^n]
                 *
                 * and add it to CSMA_LOW.
                 * Finally, multiple by the number of symbols per second
                 */

                simtime_t backoff=RandomRange(0,(unsigned int)((csma_high-csma_low)*pow(csma_exponent_base,
                                                                                        state->backoff_count)));
                backoff+=csma_init_low;
                backoff/=(double)csma_symbols_per_sec;

                /*
                 * Schedule a new event to tell this node to check whether the channel is free after the backoff time
                 */

                /*printf("Node %d schedules check channel at time %f\n",state->me,state->lvt+backoff);
                printf("TIME:%.15f\n",state->lvt);
                printf("The state is %d\n",state->state);
                printf("Link layer:%p\n",state->link_layer_outgoing);
                printf("++++++++\n");
                fflush(stdout);*/
                wait_until(state->me,state->lvt+backoff,CHECK_CHANNEL_FREE);
        }
        else{

                unsigned char temp=state->link_layer_outgoing_type;

                /*
                 * The link layer has not collected enough samples of the free channel yet and the limit for backoffs
                 * has been achieved => the packet has to be dropped => clear the pointer to packet being sent in the
                 * state of the node
                 */

                state->link_layer_outgoing=NULL;

                /*
                 * Clear the variable related to the type of the outgoing frame
                 */

                state->link_layer_outgoing_type=0;

                /*
                 * Tell the layer that requested the transmission that the packet had to be dropped
                 */

                //if(state->link_layer_outgoing_type==CTP_BEACON){
                if(temp==CTP_BEACON){

                        /*
                         * The packet is a beacon => inform the ROUTING ENGINE: simply clear its sending guard flag
                         */

                        //state->sending_beacon=false;
                        state->state&=~SENDING_BEACON;
                }
                else{

                        /*
                         * The packet is a data packet => inform the FORWARDING ENGINE about the failure
                         */

                        transmitted_data_packet(state,false);
                }
        }

}

/*
 * START THE TRANSMISSION OF A FRAME
 *
 * This is the handler of the START_FRAME_TRANSMISSION event: the link layer gathered CSMA_MIN_FREE_SAMPLES samples of
 * the channel when this was free => it's time to start the transmission of the pending frame.
 * This function triggers the transmission of the frame to the radio transceiver
 *
 * @state: pointer to the object representing the current state of the node
 */

void start_frame_transmission(node_state* state){

        /*
         * Check if the packet to be sent is still valid: if not, clear all the flags of the node and stop
         */

        if(!state->link_layer_outgoing){
                state->state&=~SENDING_BEACON;
                state->state&=~SENDING_DATA_PACKET;
                return;
        }

        /*
         * Get the type of the frame to be transmitted, either CTP_BEACON or CTP_DATA_PACKET
         */

        unsigned char type=state->link_layer_outgoing_type;

        /*
         * Set the state of the radio to RADIO_TRANSMITTING
         */

        //state->state|=RADIO_TRANSMITTING;

        /*
         * Duration of the transmission of the frame, i.e. the time it takes for the neighbour nodes to successfully
         * receive a packet (including the time to transmit an acknowledgment, if required).
         * This depends on the number of bytes in the data frame (comprising the preamble added by the physical layer)
         * and on the number of bytes of the acknowledgment frame (if it has to be sent) => it is generally referred to
         * as transmission delay; the propagation delay, i.e. the time it takes for the electromagnetic wave associated
         * to the signal to reach the recipient is negligible with respect to the transmission delay, so it is ignored.
         * First get the length of the payload in bits (sizeof returns the length in bytes and 1 byte=8 bits): this
         * depends on the size of the content of the frame, either a beacon or a data packet
         */

        double bits_length;
        if(type==CTP_BEACON)
                bits_length=sizeof(ctp_routing_packet)*8;
        else
                bits_length=sizeof(ctp_data_packet)*8;

        /*
         * Then set the duration of the transmission to the number of symbols in the frame
         */

        simtime_t duration=bits_length/(double)csma_bits_per_symbol;

        /*
         * Add the length (in symbols) of the preamble added by the physical layer
         */

        duration+=csma_preamble_length;

        /*
         * In case the node expects an acknowledgment by the recipient for the frame being transmitted, add the number
         * of symbols contained in an acknowledgement frame (including the delay between the reception of the packet and
         * the sending of the frame)
         */

        if(type==CTP_DATA_PACKET)
                duration+=csma_ack_time;

        /*
         * Transform the duration from symbols to seconds
         */

        duration/=(double)csma_symbols_per_sec;

        /*
         * Set the duration of the transmission in the link layer frame of the packet
         */

        if(!state->link_layer_outgoing) {
                /*printf("NULL outgoing with type:%d for state %d; address %p\n", state->link_layer_outgoing_type,
                       state->me,state->link_layer_outgoing);
                printf("TIME:%.15f\n",state->lvt);
                fflush(stdout);*/
                //return;
        }
        state->link_layer_outgoing->duration=duration;

        /*
         * Start the transmission of the frame using the radio transceiver: the last parameter is the virtual time when
         * the nodes that have successfully received the packet will start processing it
         */

        transmit_frame(state,type);

        /*
         * As regards with the sender node, the time necessary for switching the radio from transmission to the
         * reception mode has to be added before the transmission can be regarded as finished
         */

        duration+=csma_rxtx_delay/(double)csma_symbols_per_sec;

        /*
         * Schedule a new event to signal that the transmission is finished and acknowledgment should have been received
         */

        /*printf("Node %d schedules transmission finished at time %f\n",state->me,state->lvt+duration);
        printf("TIME:%.15f\n",state->lvt);
        printf("The state is %d\n",state->state);
        printf("Link layer:%p\n",state->link_layer_outgoing);
        printf("++++++++\n");
        fflush(stdout);*/
        wait_until(state->me,state->lvt+duration,FRAME_TRANSMITTED);
}

/*
 * SEND FRAME
 *
 * This function is invoked by both the forwarding engine and the link estimator when they have to send a data packet
 * and a beacon respectively through a wireless link.
 *
 * @state: pointer to the object representing the current state of the node
 * @recipient: ID of the recipient node
 * @type: the type of the packet (either CTP_BEACON or CTP_DATA_PACKET)
 *
 * Returns true if it's possible to send the frame now, false otherwise
 */

bool send_frame(node_state* state,unsigned int recipient, unsigned char type){

        /*
         * First check that the link layer is not already busy sending another packet: if so, return false
         */

        if(state->link_layer_outgoing || state->is_retransmitting)
                return false;

        /*
         * The link layer is not busy => get the pointer to the link layer frame of the new packet to be transmitted
         */

        get_frame(state,type);

        /*
         * Save the type of the frame in the corresponding variable of the state
         */

        state->link_layer_outgoing_type=type;

        /*
         * Set the ID of the destination node in the link layer frame
         */

        state->link_layer_outgoing->sink=recipient;

        /*
         * Set the number of times the nodes wants to see the channel free before transmitting the packet
         */

        state->free_channel_count=(unsigned char)csma_min_free_samples;

        /*
         * Set to zero the number of collisions experienced by the node at the beginning
         */

        state->backoff_count=0;

        /*
         * Start the CSMA/CD protocol
         */

        start_csma(state);

        /*
         * The packet passed by above layers has been accepted by the link layer and will now be sent inside a link
         * layer frame => return true
         */

        return true;
}

/*
 * FRAME TRANSMITTED
 *
 * This is the handler of the FRAME_TRANSMITTED event: it has to clear some flags in the state of the node and notify
 * the event to above layers of the CTP stack
 *
 * @state: pointer to the object representing the current state of the node
 */

void frame_transmitted(node_state* state){

        /*
         * Check if the packet to be sent is still valid: if not, clear all the flags of the node and stop
         */

        if(!state->link_layer_outgoing){
                state->state&=~SENDING_BEACON;
                state->state&=~SENDING_DATA_PACKET;
                return;
        }

        /*
         * Get the type of the packet transmitted
         */

        unsigned char type=state->link_layer_outgoing_type;

        /*
         * Clear the pointer to the frame being transmitted
         */

        /*printf("Node %d setting outgoing to NULL: before it was pointing %p\n",state->me,state->link_layer_outgoing);
        printf("Fields of last link frame were:%d,%d,%f,%f\n",state->link_layer_outgoing->src,state->link_layer_outgoing->sink,
               state->link_layer_outgoing->gain,state->link_layer_outgoing->duration);
        printf("TIME:%.15f\n",state->lvt);
        printf("----------\n");
        fflush(stdout);*/
        state->link_layer_outgoing=NULL;

        /*
         * Clear the variable related to the type of the last frame sent
         */

        state->link_layer_outgoing_type=0;

        /*
         * Clear the flag indicating that the node is transmitting a frame
         */

        state->link_layer_transmitting=false;

        /*
         * Clear the flag indicating that the radio is transmitting a frame
         */

        state->radio_state&=~RADIO_TRANSMITTING;

        /*
         * Signal reception to above layers, either to the FORWARDING ENGINE or the ROUTING ENGINE
         */

        if(type==CTP_BEACON) {

                /*
                 * Clear the flag indicating the transmission of a beacon
                 */

                //state->sending_beacon=false;
                state->state&=~SENDING_BEACON;

                /*
                 * Update statistics about beacons sent by the node
                 */

                node_statistics_list[state->me].beacons_sent+=1;
        }
        else{

                /*
                 * Inform the FORWARDING ENGINE that the packet has been successfully sent
                 */

                transmitted_data_packet(state,true);
        }
}

/*
 * FRAME RECEIVED
 *
 * This function is called by the physical layer when the node has received a new frame => this layer is in charge of
 * informing either the LINK ESTIMATOR or the FORWARDING ENGINE about this event, depending if the frame contains a
 * beacon or a data packet respectively.
 * The frame can be received only if the node is not busy sending a frame on its own
 *
 * @state: pointer to the object representing the current state of the node
 * @frame: pointer to the frame received
 * @type: either CTP_BEACON or CTP_DATA_PACKET
 */

void frame_received(node_state* state,void* frame, unsigned char type){

        /*
         * Check if the node is transmitting: if so, return
         */

        if(state->link_layer_transmitting)
                return;

        /*
         * Parse the frame depending on the type
         */

        if(type==CTP_BEACON){

                /*
                 * The frame contains a beacon => pass it to the LINK ESTIMATOR
                 */

                receive_routing_packet(frame,state);

        }
        else{

                /*
                 * The frame contains a data packet => pass it to the ROUTING ENGINE
                 */

                received_data_packet(frame,state);

        }
}

/*
 * COMPARE LINK LAYER FRAMES
 *
 * Helper function that returns true if two given link layer frames coincide, false otherwise
 *
 * @a: pointer to the first link layer frame
 * @b: pointer to the second link layer frame
 */

bool compare_link_layer_frames(link_layer_frame* a,link_layer_frame* b){
        return a->duration==b->duration && a->gain==b->gain && a->sink==b->sink && a->src==b->src;
}
