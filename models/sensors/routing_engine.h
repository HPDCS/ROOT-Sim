#ifndef SENSORSNETWORKMODELPROJECT_ROUTING_ENGINE_H
#define SENSORSNETWORKMODELPROJECT_ROUTING_ENGINE_H

#include <stdbool.h>
#include "application.h"

typedef struct _node node;
typedef struct _ctp_routing_frame ctp_routing_frame;
typedef struct _node_state node_state;
typedef struct _route_info route_info;

/* ROUTING ENGINE API */

void neighbor_evicted(unsigned int address,node_state* state);
bool get_etx(unsigned short* etx,node_state* state);
unsigned int get_parent(node_state* state);
void update_route(node_state* state);
void reset_beacon_interval(node_state* state);
void receive_beacon(ctp_routing_frame* routing_frame, unsigned int from,node_state*state);
void send_beacon(node_state* state);
void schedule_beacons_interval_update(node_state* state);
void double_beacons_send_interval(node_state* state);
bool is_neighbor_worth_inserting(ctp_routing_frame* routing_frame,node_state* state);
bool compare_beacons(ctp_routing_frame* a,ctp_routing_frame* b);

#endif
