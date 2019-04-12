#include <math.h>
#include "physical_layer.h"
#include "link_layer.h"
#include "parameters.h"

/*
 * PHYSICAL LAYER MODEL
 *
 * This piece of code models the behaviour of the physical layer. This depends on two elements:
 *
 * 1-the radio
 * 2-the surrounding environment (channel)
 *
 * Because of the limited GAIN of the radio featured by the nodes and because of the NOISE associated to the environment,
 * a packet sent from one node to another can't always be received => before a packet is delivered to another node, i.e.
 * it receives an event, the simulation has to check that the packet can actually be received by the recipient and this
 * module performs this check.
 * The interferences of the environment (noise) are simulated using an additive signal strength model: the power of the
 * signal through the channel is the sum of the gain associated to all the packets sent and not yet received in the
 * network => a channel is considered free only if the total power of the signal representing such a noise is below a
 * threshold.
 */

extern node_statistics* node_statistics_list;
typedef struct _pending_transmission pending_transmission;

/*
 * GAIN ADJACENCY LIST
 *
 * Array of unsorted lists, one for each node: the list contains the gain of all the wireless links having the node as
 * source node.
 * This data structure is dynamically allocated before the simulation starts, reading the values for the gain from the
 * INPUT FILE.
 */

gain_entry** gains_list=NULL;

/*
 * NOISE FLOOR LIST
 *
 * Dynamically allocated array containing the representation of the noise for each node node in the network.
 * It has a static component (the noise floor), and a dynamic component, because of the thermal noise, referred to as
 * white gaussian noise. The latter is modelled as a gaussian random variable, having mean 0 and whose standard deviation
 * is read from the input file, together with the value of the noise floor.
 */

noise_entry* noise_list=NULL;

extern FILE* file;

/*
 * INIT RADIO
 *
 * Initialize the variables in the state object that are related to the radio and to the physical layer
 *
 * @state: pointer to the object representing the current state of the node
 */

void init_physical_layer(node_state* state){

        /*
         * Set the pointer to the frame being sent to NULL
         */

        state->radio_outgoing=NULL;

        /*
         * Set the power of the pending incoming transmissions to NULL
         */

        state->pending_transmissions_power=0;

        /*
         * Set the list containing pending transmissions to NULL
         */

        state->pending_transmissions=NULL;
}

/*
 * CREATE PENDING TRANSMISSION
 *
 * Create a new instance of type "pending_transmission" to keep track of a new pending transmission
 *
 * @type: byte telling whether the frame contains a beacon or a data packet
 * @frame: pointer to the frame carried by the signal
 * @power: power of the signal
 * @lost: boolean variable set to true if the transmission won't be received by the recipient, either because too
 *        weak or because the node is busy receiving/transmitting
 *
 * Returns the address of the new instance
 */

pending_transmission* create_pending_transmission(unsigned char type,void* frame, double power,bool lost){

        /*
         * Allocate a new instance of type "pending_transmission"
         */

        pending_transmission* new_transmission=(pending_transmission*)malloc(sizeof(pending_transmission));

        /*
         * Set to zero the allocated buffer
         */

        memset(new_transmission, 0, sizeof(pending_transmission));

        /*
         * Parse the content of the frame
         */

        if(type==CTP_BEACON){

                /*
                 * Parse the frame into a beacon
                 */

                ctp_routing_packet* beacon=(ctp_routing_packet*)frame;

                /*
                 * The frame contains a beacon => set the type
                 */

                new_transmission->frame_type=CTP_BEACON;

                /*
                 * Copy the beacon into the transmission object, field by field
                 */

                new_transmission->frame.routing_packet.link_frame.duration=beacon->link_frame.duration;
                new_transmission->frame.routing_packet.link_frame.gain=beacon->link_frame.gain;
                new_transmission->frame.routing_packet.link_frame.sink=beacon->link_frame.sink;
                new_transmission->frame.routing_packet.link_frame.src=beacon->link_frame.src;
                new_transmission->frame.routing_packet.link_estimator_frame.seq=beacon->link_estimator_frame.seq;
                new_transmission->frame.routing_packet.routing_frame.ETX=beacon->routing_frame.ETX;
                new_transmission->frame.routing_packet.routing_frame.options=beacon->routing_frame.options;
                new_transmission->frame.routing_packet.routing_frame.parent=beacon->routing_frame.parent;
        }
        else{

                /*
                 * Parse the frame into a data packet
                 */

                ctp_data_packet* data_packet=(ctp_data_packet*)frame;

                /*
                 * The frame contains a data packet => set the type
                 */

                new_transmission->frame_type=CTP_DATA_PACKET;

                /*
                 * Copy the data packet into the transmission object, field by field
                 */

                new_transmission->frame.data_packet.link_frame.sink=data_packet->link_frame.sink;
                new_transmission->frame.data_packet.link_frame.src=data_packet->link_frame.src;
                new_transmission->frame.data_packet.link_frame.duration=data_packet->link_frame.duration;
                new_transmission->frame.data_packet.link_frame.gain=data_packet->link_frame.gain;
                new_transmission->frame.data_packet.payload=data_packet->payload;
                new_transmission->frame.data_packet.data_packet_frame.ETX=data_packet->data_packet_frame.ETX;
                new_transmission->frame.data_packet.data_packet_frame.options=data_packet->data_packet_frame.options;
                new_transmission->frame.data_packet.data_packet_frame.origin=data_packet->data_packet_frame.origin;
                new_transmission->frame.data_packet.data_packet_frame.seqNo=data_packet->data_packet_frame.seqNo;
                new_transmission->frame.data_packet.data_packet_frame.THL=data_packet->data_packet_frame.THL;
        }

        /*
         * Set the flag indicating whether the frame has been lost or not
         */

        new_transmission->lost=lost;

        /*
         * Set the next pointer to NULL: this will be the last pending transmission in the list
         */

        new_transmission->next=NULL;

        /*
         * Set the power of the transmission
         */

        new_transmission->power=power;

        /*
         * Return the address
         */

        return new_transmission;
}

/*
 * ADD GAIN ENTRY
 *
 * Add a new element in the list associated to the given source node.
 *
 * @source: ID of the source node of the link
 * @sink: ID of the sink node of the link
 * @gain: gain of the link
 *
 * The function aborts the simulation if any of the parameters is not valid
 */

void add_gain_entry(unsigned int source, unsigned int sink, double gain){

        /*
         * Pointer to the new instance of gain_entry
         */

        gain_entry* entry;

        /*
         * Check that the first two parameters correspond to valid node IDs
         */

        if(source>=n_prc_tot || sink>=n_prc_tot){
                printf("[FATAL ERROR] Node IDs of the link are not valid\n");
                fclose(file);
                exit(EXIT_FAILURE);
        }

        /*
         * Allocate the instance
         */

        entry=malloc(sizeof(gain_entry));

        /*
         * Set the gain of the link
         */

        entry->gain=gain;

        /*
         * Set the sink node of the link
         */

        entry->sink=sink;

        /*
         * Set the pointer to the next element to NULL
         */

        entry->next=NULL;

        /*
         * Check if this is the first element for the list of the source node
         */

        if(!gains_list[source]){

                /*
                 * It's the first element for the list of the source node => set the pointer in the array to this
                 * instance
                 */

                gains_list[source]=entry;

        }
        else{

                /*
                 * Add the new entry after the last element of the list, starting from the first element
                 */

                gain_entry* current=gains_list[source];
                while(current->next){

                        /*
                         * Go to next element
                         */

                        current=current->next;
                }

                /*
                 * Now current points to the last element of the list => connect it to the new entry
                 */

                current->next=entry;
        }
}

/*
 * ADD NOISE ENTRY
 *
 * Add an element with the noise floor and white noise of a node
 *
 * @node: ID of the node
 * @noise_floor: value of the noise floor for the node
 * @white_noise: value of the white noise for the node
 *
 * The function aborts the simulation if any of the parameters is not valid
 */

void add_noise_entry(unsigned int node_id, double noise_floor, double white_noise){

        /*
         * Pointer to the new noise_entry
         */

        noise_entry* entry;

        /*
         * Check that the first parameter corresponds to valid node ID
         */

        if(node_id>=n_prc_tot){
                printf("[FATAL ERROR] Node IDs of the link are not valid\n");
                fclose(file);
                exit(EXIT_FAILURE);
        }

        /*
         * Initialize the entry of node i to the i-the entry of the noise_list array
         */

        entry=&noise_list[node_id];

        /*
         * Set the value of noise floor in the entry
         */

        entry->noise_floor=noise_floor;

        /*
         * Set the value of the white noise in the entry
         */

        entry->range=white_noise;
}

/*
 * COMPARE PENDING TRANSMISSIONS
 *
 * Given the pointers to two pending transmissions, this helper function compares their relevant fields and returns
 * true if they all coincide, false otherwise
 *
 * @a: pointer to a pending transmission
 * @b: pointer to the other pending transmission
 */

bool compare_pending_transmissions(pending_transmission*a,pending_transmission*b){

        /*
         * Check if the metadata of the two transmissions coincide
         */

        if(a->frame_type==b->frame_type && a->lost==b->lost && a->power==b->power){

                /*
                 * Determine the type of frames brought by the transmissions
                 */

                if(a->frame_type==CTP_BEACON){

                        /*
                         * The frames are beacons
                         */

                        ctp_routing_packet* beacon_a=&a->frame.routing_packet;
                        ctp_routing_packet* beacon_b=&b->frame.routing_packet;

                        /*
                         * Return true if the the beacons coincide
                         */

                        return compare_link_layer_frames(&beacon_a->link_frame,&beacon_b->link_frame) &&
                               compare_link_estimator_frames(&beacon_a->link_estimator_frame,
                                                             &beacon_b->link_estimator_frame) &&
                               compare_beacons(&beacon_a->routing_frame,&beacon_b->routing_frame);
                }
                else{

                        /*
                         * The frames are data packets
                         */

                        ctp_data_packet* data_packet_a=&a->frame.data_packet;
                        ctp_data_packet* data_packet_b=&b->frame.data_packet;

                        /*
                         * Return true if the data packets coincide
                         */

                        return compare_link_layer_frames(&data_packet_a->link_frame,&data_packet_b->link_frame) &&
                               compare_data_packets(&data_packet_a->data_packet_frame,&data_packet_b->data_packet_frame,
                               data_packet_a->payload,data_packet_b->payload);
                }
        }

        /*
         * The two objects refers to different transmissions because they have different metadata => return false
         */

        return false;
}

/*
 * SEND ACK
 *
 * Function dedicated to the sending of the acknowledgement for a frame in case this contains a data packet
 *
 * @state: pointer to the object representing the current state of the node
 * @packet: pointer to the frame containing the data packet
 */

void send_ack(node_state* state,ctp_data_packet* packet){

        /*
         * If there's no other transmission going on, the packet is acknowledged
         */

        if(!state->radio_outgoing) {

                /*
                 * The radio transceiver is not busy => get the sender of the packet and send it an event to inform
                 * about the reception of the ack
                 */

                unsigned int sender=packet->link_frame.src;
                if(sender<n_prc_tot)
                        ScheduleNewEvent(sender,state->lvt,ACK_RECEIVED,packet,sizeof(ctp_data_packet));
                else{
                        printf("[FATAL ERROR] Scheduling event for node %d, that does not exist"
                                       "\n", sender);
                        exit(EXIT_FAILURE);
                }

        }
}

/*
 * NEW PENDING TRANSMISSION
 *
 * Helper function for the event of type TRANSMISSION_STARTED
 * It is in charge of creating a new entry for the new pending transmission in the dedicated list of the node state;
 * also it has to check whether the transmission will be received by the node or not, depending on the actual strength
 * of the interferences created by other signals
 *
 * @state: pointer to the object representing the current state of the node
 * @gain: strength of the new transmission (in dBm)
 * @type: byte telling whether the frame contains a beacon or a data packet
 * @frame: pointer to the frame carried by the signal
 * @duration: duration of the new transmission
 */

void new_pending_transmission(node_state* state, double gain,unsigned char type,void* frame,double duration){

        /*
         * Pointer to the new instance of type "pending_transmission" created for the new transmission
         */

        pending_transmission* new_pending_transmission=NULL;

        /*
         * Pointer to the last element in the list of pending transmissions
         */

        pending_transmission* last_transmission=NULL;

        /*
         * Boolean value telling whether the new transmission has enough power to be received by the node
         */

        bool lost_transmission=true;

        /*
         * Counter of pending transmissions
         */

        unsigned int pending_transmissions_count=0;

        /*
         * Check if the node is running: if not, it will not receive the frame transmitted
         */

        if(state->state&RUNNING) {

                /*
                 * Then get the strength of the signal affecting the channel perceived by the node at
                 * the moment
                 */

                double channel_strength=compute_signal_strength(state);

                /*
                 * Check if the gain of the link is enough strong for the frame to be delivered, considering the
                 * actual strength of the signal sensed by the node: if not, the frame is dropped
                 */

                if(channel_strength+csma_sensitivity<gain){

                        /*
                         * The signal carrying the frame has enough power for the frame to be received
                         * by the node.
                         * Check if the node is in the busy receiving or transmitting another frame:
                         * if so, drop the current frame
                         */

                        if(!(state->radio_state&RADIO_RECEIVING) &&
                           !(state->radio_state&RADIO_TRANSMITTING)){

                                /*
                                 * The radio is not busy receiving or transmitting another frame => the current frame is
                                 * received => set the state of the radio to RADIO_RECEIVING and clear the lost flag for
                                 * the "pending_transmission" object that describes the transmission of the frame
                                 */

                                state->radio_state|=RADIO_RECEIVING;
                                lost_transmission=false;
                        }

                }
        }

        /*
         * Go through the list of pending transmissions and check whether the current transmission will cause the node
         * to miss some of them because they are too weak w.r.t to the new one
         */

        pending_transmission* current=state->pending_transmissions;
        while(current!=NULL){

                /*
                 * If the difference between the power of the current transmission and the power of the new one is below
                 * the threshold, the former is lost
                 */

                if(current->power-csma_sensitivity<gain)
                        current->lost=true;

                /*
                 * Go to next element of the list and store the pointer to the last NOT NULL element
                 */

                last_transmission=current;
                current=current->next;

                /*
                 * Update the counter of pending transmissions
                 */

                pending_transmissions_count++;
        }

        /*
         * Increment the counter of the strength of the signal sensed by the node (convert from dBm to
         * mW)
         */

        state->pending_transmissions_power+=pow(10.0, gain / 10.0);

        /*
         * Create an entry for the the new pending transmission
         */

        new_pending_transmission=create_pending_transmission(type,frame,gain,lost_transmission);

        /*
         * Check if there are other pending transmissions
         */

        if(pending_transmissions_count){

                /*
                 * There are other pending transmissions => add the new transmission at the end of the list and get
                 * the address
                 */

                last_transmission->next=new_pending_transmission;

        }
        else{

                /*
                 * This is the only pending transmission => set the pointer of the list of pending transmission to its
                 * address
                 */

                state->pending_transmissions=new_pending_transmission;
        }

        /*
         * Schedule a new event corresponding to the moment when the transmission will be finished
         */

        if(state->me<n_prc_tot)
                ScheduleNewEvent(state->me,state->lvt+duration,TRANSMISSION_FINISHED,new_pending_transmission,
                         sizeof(pending_transmission));
        else{
                printf("[FATAL ERROR] Scheduling event for node %d, that does not exist"
                               "\n", state->me);
                exit(EXIT_FAILURE);
        }
}

/*
 * TRANSMISSION FINISHED
 *
 * Helper function for the event TRANSMISSION_FINISHED
 * It is in charge of removing the element of the finished transmission from the list of pending transmissions: if the
 * transmission has been successfully received by the node, it starts processing the associated frame
 *
 * @state: pointer to the object representing the current state of the node
 * @finished_transmission: pointer to the finished transmission
 */

void transmission_finished(node_state* state,pending_transmission* finished_transmission){

        /*
         * Pointer to the element of the list that points to the transmission that is now finished
         */

        pending_transmission* predecessor=NULL;

        /*
         * The address given as second parameter does not coincide with the address of the object representing the
         * finished transmission in the list of the pending transmissions => we have to find it scanning the list.
         * It is used to free memory allocated for the transmission
         */

        pending_transmission* finished_transmission_list=NULL;

        /*
         * Type of the content of the frame transmitted
         */

        unsigned char type;

        /*
         * In time between the beginning and the end of this transmission, new frames may have been sent to the node and
         * so there may be further pending transmissions whose power is not strong enough compared to the current
         * transmission => they will be missed by the node
         */

        /*
         * Pointer to the current transmission being checked
         */

        pending_transmission* current_transmission=state->pending_transmissions;
        if(!state->pending_transmissions) {
                /*printf("pending transmissions NULL\n");
                fflush(stdout);*/
        }

        /*
         * Go through all the pending transmissions
         */

        while(current_transmission!=NULL){

                /*
                 * Check if the transmission analyzed is predecessor of the transmission finisehed in the list of
                 * pending transmissions
                 */

                if(current_transmission->next &&
                   compare_pending_transmissions(current_transmission->next,finished_transmission)) {

                        /*
                         * Found the predecessor
                         */

                        predecessor=current_transmission;

                        /*
                         * Found the address of the pending transmission in the list
                         */

                        finished_transmission_list=current_transmission->next;
                }

                /*
                 * Check if the transmission analyzed will be missed by the node because not enough strong w.r.t. the
                 * transmission that is finishing
                 */

                if(!compare_pending_transmissions(current_transmission,finished_transmission)){
                        if(current_transmission->power-csma_sensitivity<finished_transmission->power){
                                current_transmission->lost=true;
                        }
                }

                /*
                 * Go to next element of the list
                 */

                current_transmission=current_transmission->next;
        }

        /*
         * Remove the finished transmission from the list
         */

        if(predecessor){

                /*
                 * The list has this structure at the moment:
                 *
                 * ...->x->y->z..
                 *
                 * where y is the element to be removed
                 */

                predecessor->next=predecessor->next->next;

                /*
                 * Now the list is like this:
                 *
                 * ...x->z..
                 */
        }
        else{

                /*
                 * If it is the first element of the list, update the pointer to the list
                 */

                finished_transmission_list=state->pending_transmissions;
                state->pending_transmissions=state->pending_transmissions->next;
        }

        /*
         * Remove the power associated to transmission from the strength of the global signal sensed by the node
         */

        state->pending_transmissions_power-=pow(10.0, finished_transmission->power / 10.0);

        /*
         * It may be the case that while the transmission was ongoing, even  stronger transmission have come and so this
         * transmission will be missed by the node too
         */

        if(finished_transmission->power-csma_sensitivity<compute_signal_strength(state)) {
                finished_transmission->lost = true;
                if(finished_transmission->frame_type==CTP_BEACON)
                        node_statistics_list[state->me].lost_beacons+=1;
                else
                        node_statistics_list[state->me].lost_data_packets+=1;
        }

        /*
         * If the frame has been received by the node, it has to be processed
         */

        if(!finished_transmission->lost){

                /*
                 * The frame has been received => get its type
                 */

                type=finished_transmission->frame_type;

                /*
                 * Inform the LINK ESTIMATOR or the FORWARDING ENGINE about the reception: check the type to determine if
                 * the frame contains a beacon or a data packet
                 */

                if(type==CTP_BEACON) {
                        frame_received(state, &finished_transmission->frame.routing_packet, type);
                }
                else{
                        /*
                         * If the frame contains a data packet, the sender is waiting for an acknowledgment by the
                         * intended recipient => if this node is the recipient and it is not busy transmitting another
                         * frame, it has to send the acknowledgment to the recipient.
                         * First get the recipient, as indicated in the link layer frame
                         */

                        /*
                         * The frame contains a data packet => extract the link layer frame from the packet
                         */

                        link_layer_frame *link_frame = &finished_transmission->frame.data_packet.link_frame;

                        /*
                         * Get the intended recipient
                         */

                        unsigned int recipient = link_frame->sink;

                        /*
                         * If this is the recipient and is not busy transmitting, send an acknowledgment
                         */

                        if(recipient==state->me && !state->link_layer_transmitting)
                                send_ack(state,&finished_transmission->frame.data_packet);

                        /*
                         * Inform the forwarding engine that a frame has been received
                         */

                        frame_received(state, &finished_transmission->frame.data_packet, type);

                }
        }

        /*
         * The radio is no longer receiving => clear the corresponding flag
         */

        state->radio_state&=~RADIO_RECEIVING;

        /*
         * Reset the pointer to the outgoing frame
         */

        state->radio_outgoing=NULL;

        /*
         * Finally remove the element associated to the pending transmission
         */

        free(finished_transmission_list);

}

/*
 * CHECK GAINS LIST
 *
 * This function checks that there's a list of gains for each node in the simulation and that such a list contains
 * exactly a number of elements equal to n_prc_tot-1: if this two conditions are not verified, the simulation is aborted
 */

void check_gains_list(void){

        /*
         * Counter of elements in each list
         */

        unsigned int counter=0;

        /*
         * Index of the node
         */

        unsigned int index;

        /*
         * Check correctness of the list for each node
         */

        for(index=0;index<n_prc_tot;index++){

                /*
                 * Check that there is a list of gains for node "index"
                 */

                if(gains_list[index]) {

                        /*
                         * The first element of the list
                         */

                        gain_entry *current =gains_list[index];

                        /*
                         * Count the number of elements in the list
                         */

                        while(current){
                                counter++;
                                current=current->next;
                        }

                        /*
                         * Check that the list contains n_prc_tot-1 elements: if not, return false
                         */

                        if(counter!=n_prc_tot-1) {
                                printf("[FATAL ERROR] Node %d has %d links; they have to be %d\n",index,counter,
                                       n_prc_tot-1);
                                exit(EXIT_FAILURE);
                        }

                        /*
                         * The list of gains for the current node is correct = > resent the counter and go to the next
                         * node
                         */

                        counter=0;
                        continue;
                }

                /*
                 * The list of gains of the links for the current node is missing => abort
                 */

                printf("[FATAL ERROR] No link specified for node %d; they have to be %d\n",index,n_prc_tot-1);
                exit(EXIT_FAILURE);
        }
}

/*
 * CHECK NOISE LIST
 *
 * This function checks that there's a value of noise floor and white gaussian noise for each node in the simulation and
 * if this condition is not verified, the simulation is aborted
 */

void check_noises_list(void){

        /*
         * Index of the node
         */

        unsigned int index;

        /*
         * Check correctness of the value for each node
         */

        for(index=0;index<n_prc_tot;index++){

                /*
                 * Check that the noise entry for node "index" is initialized: if not, abort the simulation
                 */

                if(!noise_list[index].noise_floor && !noise_list[index].range) {
                        printf("[FATAL ERROR] Noise for node %d is not given\n", index);
                        exit(EXIT_FAILURE);
                }
        }

}

/*
 * TRANSMIT FRAME
 *
 * This function simulates the physical transmission of a link-layer frame to another node through a wireless link.
 * The wireless channel is shared by all the nodes of the network, so the signals carrying the frames will interfere
 * one another => a signal travelling through a link is received by the sink node only if its strength is greater than
 * the strength of the noise resulting from the sum of all the other signals plus the noise affecting the sink node
 * itself. Moreover, if a signal B is sent to same sink node as another signal A, with A weaker than B, the sink node
 * will only receive the signal B => we have to keep track of all the signals being transmitted to each node: if two or
 * more signals overlaps in time, only the strongest one will be received by the node. That's why this function only
 * informs the recipient node that the transmission of a frame has started at time x and will finish at some time in the
 * future y, when the frame will be delivered => the recipient node has to keep track of the ongoing transmissions that
 * occur in the interval between x and y because they may overwrite the former transmission
 *
 * @state: pointer to the object representing the current state of the node
 * @type: byte telling whether the frame contains a beacon or a data packet
 */

void transmit_frame(node_state* state,unsigned char type){

        /*
         * Get the pointer to the frame to be sent: either the beacon of the node or the packet in the head of the
         * output queue
         */

        if(type==CTP_BEACON)
                state->radio_outgoing = &state->routing_packet;
        else {
                state->radio_outgoing = &state->forwarding_queue[state->forwarding_queue_head]->packet;
        }

        /*
         * Get the first element in the list of the links of the sender
         */

        gain_entry* gain_entry=gains_list[state->me];

        /*
         * Transmit the frame to all the nodes connected to the sender: the description of each link is an element of
         * the list.
         */

        while(gain_entry){

                /*
                 * Get the gain of the link
                 */

                double gain=gain_entry->gain;

                /*
                 * Get the sink node of the link
                 */

                unsigned int sink=gain_entry->sink;

                /*
                 * Set the value of the gain in the link-layer header of the frame: this is required by the simulation
                 * to determine whether the packet will be received by the recipient node or not.
                 * First parse the frame being transmitted to the data structure corresponding to the type
                 */

                if(type==CTP_BEACON){

                        /*
                         * This frame contains a beacon
                         */

                        ((ctp_routing_packet*)state->radio_outgoing)->link_frame.gain=gain;

                        /*
                         * Schedule a new event destined to the sink node of the link, containing the frame being
                         * transmitted
                         */

                        if(sink<n_prc_tot)
                                ScheduleNewEvent(sink,state->lvt,TRANSMISSION_BEACON_STARTED,state->radio_outgoing,
                                         sizeof(ctp_routing_packet));
                        else{
                                printf("[FATAL ERROR] Scheduling event for node %d, that does not exist"
                                               "\n", sink);
                                exit(EXIT_FAILURE);
                        }

                }
                else{

                        /*
                         * This frame contains a data packet
                         */

                        ((ctp_data_packet*)state->radio_outgoing)->link_frame.gain=gain;

                        /*
                         * Schedule a new event destined to the sink node of the link, containing the frame being
                         * transmitted
                         */

                        if(sink<n_prc_tot)
                                ScheduleNewEvent(sink,state->lvt,TRANSMISSION_DATA_PACKET_STARTED,state->radio_outgoing,
                                         sizeof(ctp_data_packet));
                        else{
                                printf("[FATAL ERROR] Scheduling event for node %d, that does not exist"
                                               "\n", sink);
                                exit(EXIT_FAILURE);
                        }
                }

                /*
                 * Go to next entry
                 */

                gain_entry=gain_entry->next;
        }
}

/*
 * GET CURRENT NOISE
 *
 * This function simulates the variability of the noise affecting a node by returning a random value from the uniform
 * distribution [white_noise_mean-noise_range,white_noise_mean+noise_range] and adding to the value pf the noise floor
 */

double get_current_noise(unsigned int node){

        /*
         * Get the mean value of the dynamic component of the noise
         */

        double mean=white_noise_mean;

        /*
         * Get a random value to be added to the mean value of the dynamic component of the noise
         */

        double rand=RandomRange(0,2000000);
        rand/=1000000.0;
        rand-=1.0;
        rand*=noise_list[node].range;

        /*
         * Return the sum of the mean value of the of the dynamic component of the noise plus the random value from the
         * uniform distribution
         */

        return mean+rand+noise_list[node].noise_floor;
}


/*
 * COMPUTE STRENGTH OF THE SIGNAL SENSED BY THE NODE
 *
 * This function returns the power of the signal affecting the channel calculated by a node: it's the sum of the power
 * of the noise from the environment plus the power of the signals associated to all the packets that are being sent
 * to the node when the calculation is done.
 *
 * @state: pointer to the object representing the current state of the node
 */

double compute_signal_strength(node_state* state){

        /*
         * Get the current value of the noise affecting the node
         */

        double noise=get_current_noise(state->me);

        /*
         * Transform the value above from dBm to mW (milli Watt)
         * This is necessary to perform the sum of the power associated to signals as an algebraic sum
         *
         * 1mW=10^(1dbm/10)
         */

        double strength=pow(10.0,noise/10.0);

        /*
         * Add the sum of the power of all the transmissions sensed by the node (this value is in mW)
         */

        strength+=state->pending_transmissions_power;

        /*
         * Transforms the value back to dBm because all the other parameters related to the strength of signals are
         * given in dBm
         */

        return 10.0*log(strength)/log(10.0);
}

/*
 * IS CHANNEL FREE
 *
 * Function invoked by the link layer to determine whether the channel is free and a frame can hence be transmitted
 * through it.
 * A channel is declared to be free if the strength of the signal traversing it is below the threshold corresponding
 * to variable CHANNEL_FREE_THRESHOLD
 *
 * @state: pointer to the object representing the current state of the node
 *
 * Returns true if the channel is to be considered free, false otherwise
 */

bool is_channel_free(node_state* state){

        /*
         * Get the strength of the signal occupying the channel at the moment
         */

        double signal_strength=compute_signal_strength(state);

        /*
         * Return true if the strength is less than the threshold, false otherwise
         */

        if(signal_strength<channel_free_threshold)
                return true;
        return false;
}
