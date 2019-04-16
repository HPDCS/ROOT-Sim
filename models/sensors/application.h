#ifndef SENSORSNETWORKMODELPROJECT_APPLICATION_H
#define SENSORSNETWORKMODELPROJECT_APPLICATION_H

#include "link_estimator.h"
#include "routing_engine.h"
#include "forwarding_engine.h"
#include "parameters.h"
#include <ROOT-Sim.h>
#include <math.h>
#include <pthread.h>

/*
 * EVENT TYPES
 *
 * List of IDs associated to the events of the simulation
 */

enum{
        /*
         * After the root node (logical process) has parsed the configuration file provided by the user, it broadcasts this
         * event to all the other nodes to tell them they can finally start
         */

        START_NODE=1,
        SEND_BEACONS_TIMER_FIRED=2, // The timer for beacons has been fired  => broadcast a beacon
        SEND_PACKET_TIMER_FIRED=3, // The timer for data packets has been fired  => send a data packet
        CREATE_PACKET_TIMER_FIRED=4, // // The timer for data packets has been fired  => create a data packet
        UPDATE_ROUTE_TIMER_FIRED=5, // The timer for updating the route has been fired
        SET_BEACONS_TIMER=6, // The interval of the timer for beacons has to be updated
        RETRANSMITT_DATA_PACKET=7, // Try to re-send a data packet whose first sending attempt failed
        ACK_RECEIVED=8, // The ack for the last data packet sent has just been received
        CHECK_CHANNEL_FREE=9, // The link layer has to check whether the channel is free
        START_FRAME_TRANSMISSION=10, // The link layer starts to transmit a frame over the channel
        FRAME_TRANSMITTED=11, // The frame has been transmitted
        TRANSMISSION_BEACON_STARTED=12, // The transmission of a new frame containing a beacon has started
        TRANSMISSION_DATA_PACKET_STARTED=13, // The transmission of a new frame containing a data packet has started
        TRANSMISSION_FINISHED=14 // The transmission of a new frame has finished
};

/*
 * RADIO FLAGS
 *
 * Flags that indicate the state of the radio transceiver
 */

enum {
        RADIO_TRANSMITTING=0x1, // The radio is busy transmitting a frame
        RADIO_RECEIVING=0x2 // The radio is busy receiving a frame
};

/*
 * STATE FLAGS
 *
 * Flags that indicate the state of the node
 */

enum{
        SENDING_DATA_PACKET=0x1, // Busy sending a data packet => wait before sending another one
        SENDING_BEACON=0x2, // Busy sending a beacon => wait before sending another one
        SENDING_LOCAL_DATA_PACKET=0x4, // Busy sending a local data packet => wait before sending another one
        RUNNING=0x8 // The node is running => has not failed (yet)
};

/*
 * CTP CONSTANTS
 */

enum{
        CTP_PULL= 0x80, // TEP 123: P field
        CTP_CONGESTED= 0x40, // TEP 123: C field
        BROADCAST_ADDRESS=0xffff, // A packet with such an address is sent to all the neighbor nodes
        CTP_BEACON=0x1, // Flag indicating that the packet is a beacon
        CTP_DATA_PACKET=0x2 // Flag indicating that the packet carries data from the sensor(s)
};

/*
 * LINK-LAYER FRAME
 */

typedef struct _link_layer_frame{
        unsigned int src; // ID of the node that sends the frame
        unsigned int sink; // ID of the node the frame is destined to

        /*
         * Gain of the sender node: this is used by the simulator to determine whether the a frame will be received by
         * a node or not
         */

        double gain;
        double duration; // Time necessary to deliver the packet; this depends on the size of the packet
}link_layer_frame;

/*
 * CTP LINK ESTIMATOR FRAME
 */

typedef struct _ctp_link_estimator_frame{
        unsigned char seq;
}ctp_link_estimator_frame;

/*
 * CTP ROUTING FRAME
 */

typedef struct _ctp_routing_frame{
        unsigned char options;
        unsigned int parent;
        unsigned short ETX;
}ctp_routing_frame;

/*
 * ROUTING PACKET (BEACON)
 *
 * Structure representing a routing packet: it contains the description of the current route the node, and it has to be
 * sent to neighbors nodes in order to help them in the choice of their own route to the root of the collection tree.
 * A beacon passes from the ROUTING ENGINE to the LINK ESTIMATOR, before being transmitted from the LINK LAYER
 */

typedef struct _ctp_routing_packet{
        link_layer_frame link_frame;
        ctp_link_estimator_frame link_estimator_frame;
        ctp_routing_frame routing_frame;
}ctp_routing_packet;

/*
 * CTP DATA FRAME
 */

typedef struct _ctp_data_packet_frame{
        unsigned char options;
        unsigned char THL;
        unsigned short ETX;
        unsigned int origin;
        unsigned char seqNo;
}ctp_data_packet_frame;

/*
 * CTP DATA PACKET
 */

typedef struct _ctp_data_packet{
        link_layer_frame link_frame;
        ctp_data_packet_frame data_packet_frame;
        int payload;
}ctp_data_packet;

/*
 * Structure associated to an element of the forwarding queue: it features a pointer to a data packet and a counter of
 * the number of times the engine has already tried to transmitting the packet.
 *
 * In order to send a data packet, a corresponding element of this type has to be stored in the forwarding queue => it
 * will remain in the queue until the packet is sent or the limit for the number of transmissions is reached
 */

typedef struct _forwarding_queue_entry{
        ctp_data_packet packet; // The data packet to send
        unsigned char retries; // Number of transmission attempts performed so far

        /*
         * Flag indicating whether the data packet is local, namely it has been created by the
         */

        bool is_local;
} forwarding_queue_entry;

/*
 * ROUTE INFO
 *
 * Structure describing the current path chosen by a node to send data packets
 */

typedef struct _route_info{
        unsigned int parent; // ID of the parent node
        unsigned short etx; // ETX of the parent node + 1-hop ETX of the link to the parent node
        bool congested; // Boolean flag telling whether the node is congested (half of its forwarding queue full) or not
}route_info;

typedef struct _routing_table_entry{
        unsigned int neighbor;
        route_info info;
}routing_table_entry;

/*
 * GAIN ENTRY
 *
 * Data structure representing the gain associated to a single directed wireless link.
 * The value of the gain is provided by the input file of the simulation.
 * For each node of the simulation there's a list of elements of such type, one for each of its links
 */

typedef struct _gain_entry{
        double gain; // Gain associated to the link
        unsigned int sink; // ID of the sink node of the link
        struct _gain_entry* next; // Pointer to the next entry in the list of gains
}gain_entry;

/*
 * NOISE ENTRY
 *
 * Data structure representing the noise associated to a node (in dBm).
 * The input file contains the value of the noise floor for each node. Because of thermal noise, the node's noise floor
 * read by the node changes over time: this variability is modelled as a Gaussian random variable with MEAN VALUE 0 and
 * STANDARD DEVIATION given by the WHITE GAUSSIAN NOISE, also provided together with the noise floor by the input file.
 */

typedef struct _noise_entry{
        double noise_floor; // Value of the noise floor for the node
        double range; // Standard deviation of the dynamic component of the noise characterizing a node
}noise_entry;

/*
 * PENDING TRANSMISSION FRAME
 *
 * A pending transmission is referred to both beacons and data packets => a union is used to represent the payload of
 * a transmission
 */

union transmission_frame{
        ctp_data_packet data_packet;
        ctp_routing_packet routing_packet;
};


/*
 * PENDING TRANSMISSION
 *
 * Data structure representing the transmission of a frame.
 * More than one frame may be sent to a node at the same time, but it will receive only the one associated with the
 * strongest signal, given that the strength is greater than the noise affecting the node.
 */

typedef struct _pending_transmission{

        /*
         * The frame carried by the transmission: it's either "ctp_data_packet" or "ctp_routing_packet"
         */

        union transmission_frame frame;
        unsigned char frame_type; // The type of the frame, either CTP_BEACON or CTO_DATA_PACKET
        double power; // The strength of the transmission
        bool lost; // Boolean value set to true in case a stronger transmission comes and hides the current one
        struct _pending_transmission* next; // Pointer to the next element in the list of pending transmissions
}pending_transmission;

/*
 * STATISTICS
 *
 * This data structure is used to track some useful pieces of information about a node that can be used to better
 * understand how the network behaves
 */

typedef struct _node_statistics{
        unsigned long beacons_received; // The number of beacons received by the node
        unsigned long data_packets_received; // The number of data packets received by the node
        unsigned long beacons_sent; // The number of beacons sent by the node
        unsigned long data_packets_sent; // The number of data packets sent by the node
        unsigned long data_packets_acked; // The number of data packets sent by the node that have been acked
        unsigned long collected_packets; // The number of packets sent by the node and collected by the root
        unsigned long lost_beacons; // The number of beacons lost by the node
        unsigned long lost_data_packets; // The number of data packets lost by the node
        bool failed; // Boolean value indicating whether a node has crashed
}node_statistics;

/*
 * NODE STATE
 *
 * Structure representing the state of a node (logic process) at any point in the virtual time.
 * Its main data structures are those related to the stack of the Collection Tree Protocol, i.e. the Link Estimator, the
 * Routing Engine and the Forwarding Engine; also it contains the data structure related to the Data Link layer and to
 * the radio model
 */

typedef struct _node_state{

        bool is_retransmitting;

        /* RADIO FIELDS - start */

        /*
         * PENDING TRANSMISSIONS
         *
         * This is the pointer to the first element of a list that keeps track of all the incoming transmissions
         */

        pending_transmission* pending_transmissions;

        /*
         * PENDING TRANSMISSIONS POWER
         *
         * This variable holds the sum of the power of signals associated to all the packets that have been transmitted
         * to this node and have not been received yet. As soon as a packet is received by the node, the associated
         * signal no longer exists, so the corresponding power is subtracted from this variable.
         * When a node checks whether the channel is free, it checks whether the power of the signal resulting from all
         * the ongoing transmission is below a threshold: if so, the channel is regarded as free, otherwise it is
         * regarded as busy and the node backs off
         */

        double pending_transmissions_power;

        /*
         * Pointer to the frame being sent: if upper layers ask for the transmission of a new frame and this is not
         * NULL, the radio transceiver ignores the requests
         */

        void* radio_outgoing;

        /*
         * Bit-wise OR combinations of flags indicating the actual state of the radio transceiver (whether it is busy
         * transmitting or receiving frames)
         */

        unsigned char radio_state;

        /* RADIO FIELDS - end */

        /* LINK LAYER FIELDS - start */

        unsigned char backoff_count; // Number of times the node experienced a collision on the channel and backed off
        unsigned char free_channel_count; // Number of times that the node has "seen" the channel free

        /*
         * Pointer to the link layer frame of the next packet to be sent: if upper layers ask for the transmission of a
         * new packet and this point is not set to NULL, the link layer ignores the requests. This may happen when the
         * node is sending a beacon and the time to send a data packet too has come and vice-versa, or when the time to
         * send a data packet too has come and the node is backing off for the retransmission offset
         */

        link_layer_frame* link_layer_outgoing;

        /*
         * Flag indicating whether the frame that is being sent is a beacon or a data packet
         */

        unsigned char link_layer_outgoing_type;
        bool link_layer_transmitting; // Boolean value telling whether the link layer is transmitting a frame

        /* LINK LAYER FIELDS - end */

        /* LINK ESTIMATOR FIELDS - start */

        /*
         * LINK ESTIMATOR TABLE
         *
         * An array of link_estimator_table_entry with a number of NEIGHBOR_TABLE_SIZE elements: each entry corresponds
         * to a neighbor node
         */

        link_estimator_table_entry link_estimator_table[NEIGHBOR_TABLE_SIZE];

        /*
         * BEACON SEQUENCE NUMBER
         *
         * Sequence number of the beacon, incremented by one at every beacon transmission.
         * By counting the number of beacons received from a neighbor and comparing this with the sequence number
         * reported in the packet, one can determine if, and how many if so, beacons from that neighbor have been lost
         * => this provides an estimate of the ingoing quality of the link to that neighbor
         */

        unsigned char beacon_sequence_number;

        /* LINK ESTIMATOR FIELDS - end */

        /* ROUTING ENGINE FIELDS - start */

        ctp_routing_packet routing_packet; // The routing packet of the node
        route_info route; // Description of the route from the current node to the root

        /*
         * BEACONS INTERVAL/SENDING TIME
         */

        /*
         * The current value of I_b (interval between sending of two successive beacons)
         */

        double current_interval;

        /*
         * Time to wait before sending another beacon; it is chosen within the interval [I_b/2 , I_b]
         */

        double beacon_sending_time;

        /*
         * ROUTING TABLE
         *
         * An array of routing_table_entry with a number of ROUTING_TABLE_SIZE elements, each corresponding to a
         * neighbor node => the routing engine maintains for each the value of ETX and selects the one with the lowest
         * value as parent
         */

        routing_table_entry routing_table[ROUTING_TABLE_SIZE];
        unsigned char neighbors; // Number of active entries in the routing table

        /* ROUTING ENGINE FIELDS - end */

        /* FORWARDING ENGINE FIELDS - start */

        /*
         * FORWARDING POOL - start
         *
         * When a data packet has to be forwarded, the node extracts one entry from this pool, initializes it to the
         * data of the data packet received and finally stores a pointer to the entry in the forwarding queue.
         *
         * The pool is nothing more than fixed-size array, whose elements are of type "forwarding_queue_entry": in fact
         * packets to be forwarded are stored in the same output queue as packets created by the node itself and as soon
         * as they reach the head of the queue they are sent.
         *
         * An entry is taken from the pool using the "get" method and it is given back to the pool using the "put"
         * method: the entries are taken in order, according to their position, and are released in order.
         *
         * Two variables help handling the pool:
         *
         * 1-forwarding_pool_count
         * 2-forwarding_pool_index
         */

        forwarding_queue_entry forwarding_pool[FORWARDING_POOL_DEPTH];
        unsigned char forwarding_pool_count; // Number of elements in the pool
        unsigned char forwarding_pool_index; // Index of the array where the next entry put will be collocated

        /* FORWARDING POOL - end */

        /*
         * FORWARDING QUEUE - start
         *
         * An array of elements of pointers to "forwarding_queue_entry" represents the output queue of the node; pointers
         * refer to packets (actually entries) created by the node or packets (entries) received by other nodes
         * that have to be forwarded.
         *
         * Three variables are necessary to implement the logic of a FIFO queue using such an array:
         *
         * 1-forwarding_queue_count
         * 2-forwarding_queue_head
         * 3-forwarding_queue_tail
         *
         * They are all set to 0 at first. Entries are not explicitly cleared when elements are dequeued, nor the head
         * of the queue is always represented by the first element of the array => the actual positions of the elements
         * in the array don't correspond to their logical position within the queue.
         *
         * As a consequence, it is necessary to check the value of "forwarding_queue_count" in order to determine
         * whether the queue is full or not.
         *
         * A packet is enqueued before being sent: when it reaches the head of the queue it is forwarded; at some point
         * it is then dequeued
         */

        forwarding_queue_entry* forwarding_queue[FORWARDING_QUEUE_DEPTH];

        unsigned char forwarding_queue_count; // The counter of the elements in the forwarding queue
        unsigned char forwarding_queue_head; // The index of the first element in the queue (least recently added)
        unsigned char forwarding_queue_tail; // The index of the last element in the queue (most recently added)

        /* FORWARDING QUEUE - end */

        /*
         * OUTPUT CACHE - start
         *
         * An array of pointers to data packets represents the output LRU (Least Recently Used )cache of the node, where
         * are stored the most recently packets sent by the node => it's used to avoid forwarding the same packet twice.
         *
         * NOTE it is assumed that the node does not produce duplicates on its own => duplicates only regard packets to
         * be forwarded; usually they are caused by not acknowledged packets
         *
         * Two variables are necessary to implement the logic of a LRU cache:
         *
         * 1-output_cache_count
         * 2-output_cache_first
         *
         * They are both set to 0 at first. Entries are not explicitly cleared when elements are removed, nor the least
         * recent entry of the cache is always represented by the first element of the array => the actual positions of
         * the elements in the array don't correspond to their logical position within the cache.
         *
         * As a consequence, it is necessary to check the value of "output_cache_count" in order to determine whether
         * the cache is full or not.
         *
         * When a packet is inserted in the cache, it means it has been used => if the same packet is already in the
         * cache, it is moved in order to indicate that it has been recently accessed, otherwise the element that was
         * least recently used is removed
         */

        ctp_data_packet output_cache[CACHE_SIZE];

        unsigned char output_cache_count; // Number of sent data packets cached
        unsigned char output_cache_first; // Index of the entry in the cache that was least recently added

        /* OUTPUT CACHE - end */

        /*
         * DATA PACKET
         *
         * Next data packet to be sent: it carries the actual payload the node wants to be delivered to the root of the
         * tree
         */

        ctp_data_packet data_packet;

        ctp_data_packet last_packet_acked; // When a packet is acked, this variable points to the packet

        /*
         * LOCAL FORWARDING QUEUE ENTRY
         *
         * The entry of the forwarding queue associated to the current node when this has some data to be sent
         */

        forwarding_queue_entry local_entry;

        unsigned char data_packet_seqNo; // Sequence number of the data packet to be sent (initially 0)

        /* FORWARDING ENGINE FIELDS - end */

        /* STATISTICS - start */

        unsigned long parent_changes; // Number of times the node has changes its parent
        unsigned long routing_loops; // Number of times the node detects the risk of a routing loop
        unsigned long duplicates; // Number of duplicates detected by the node

        /* STATISTICS - end */

        bool root; // Boolean variable that is set to true if the node is the designated root of the collection tree
        unsigned int me; // ID of this node (logical process)
        unsigned char state; // Bit-wise OR combination of flags indicating the state of the node
        simtime_t lvt; // Value of the Local Virtual Time
} node_state;

void wait_until(unsigned int me,simtime_t timestamp,unsigned int type);
void collected_data_packet(ctp_data_packet* packet);

#endif
