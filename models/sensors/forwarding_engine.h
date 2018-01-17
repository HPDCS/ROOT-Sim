#ifndef SENSORSNETWORKMODELPROJECT_FORWARDING_ENGINE_H
#define SENSORSNETWORKMODELPROJECT_FORWARDING_ENGINE_H

#include <stdbool.h>

typedef struct _ctp_data_packet_frame ctp_data_packet_frame;
typedef struct _ctp_data_packet ctp_data_packet;

/*
 * PARAMETERS RELATED TO FORWARDING ENGINE
 */

#ifndef FORWARDING_QUEUE_DEPTH
#define FORWARDING_QUEUE_DEPTH 13 // Max number of packets that can be stored in the forwarding queue at the same time
#endif

#ifndef FORWARDING_POOL_DEPTH
#define FORWARDING_POOL_DEPTH 13 // Max number of packets that can be stored in the forwarding pool at the same time
#endif

#ifndef CACHE_SIZE
#define CACHE_SIZE 4 // Max number of packets that can be stored in the output cache at the same time
#endif

#ifndef MAX_RETRIES
#define MAX_RETRIES 30 // Max number of times the forwarding engine will try to transmit a packet before giving up
#endif

/*
 * Interval of time after which the node tries to resend a data packet that has not been successfully sent or
 * acknowledged (in seconds)
 */

#ifndef DATA_PACKET_RETRANSMISSION_OFFSET
#define DATA_PACKET_RETRANSMISSION_OFFSET 0.022
#endif

#ifndef DATA_PACKET_RETRANSMISSION_DELTA
#define DATA_PACKET_RETRANSMISSION_DELTA 0.007 // Delta applied to calculate the random interval before a retransmission
#endif

/*
 * Interval of time after which the node tries to resend a data packet in case it has not chosen a parent yet (in
 * seconds)
 */

#ifndef NO_ROUTE_OFFSET
#define NO_ROUTE_OFFSET 10
#endif

#ifndef SEND_PACKET_TIMER
#define SEND_PACKET_TIMER 1 // Period of the timer that triggers the sending of a new data packet (in seconds)
#endif

#ifndef CREATE_PACKET_TIMER
#define CREATE_PACKET_TIMER 3 // Period of the timer that triggers the creation of a new data packet (in seconds)
#endif

#ifndef MIN_PAYLOAD
#define MIN_PAYLOAD 10 // Lower bound for the range of the data gathered by the node
#endif

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 100 // Upper bound for the range of the data gathered by the node
#endif


/* FORWARDING ENGINE API */

void create_data_packet(node_state* state);
void start_forwarding_engine(node_state* state);
bool send_data_packet(node_state* state);
void forward_data_packet(ctp_data_packet* packet,node_state* state);
void transmitted_data_packet(node_state* state,bool result);
void received_data_packet(void* message,node_state* state);
bool is_congested(node_state* state);
void parse_forwarding_engine_parameters(void* event_content);
bool compare_data_packets(ctp_data_packet_frame* a,ctp_data_packet_frame* b,int payload_a,int payload_b);

#endif