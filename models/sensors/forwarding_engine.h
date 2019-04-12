#ifndef SENSORSNETWORKMODELPROJECT_FORWARDING_ENGINE_H
#define SENSORSNETWORKMODELPROJECT_FORWARDING_ENGINE_H

#include <stdbool.h>

typedef struct _ctp_data_packet_frame ctp_data_packet_frame;
typedef struct _ctp_data_packet ctp_data_packet;

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
