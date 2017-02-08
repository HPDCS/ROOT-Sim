#ifndef SENSORSNETWORKMODELPROJECT_ROUTING_ENGINE_H
#define SENSORSNETWORKMODELPROJECT_ROUTING_ENGINE_H

#include <stdbool.h>
#include "application.h"

typedef struct _node node;
typedef struct _ctp_routing_frame ctp_routing_frame;
typedef struct _node_state node_state;
typedef struct _route_info route_info;

/*
 * PARAMETERS RELATED TO THE ROUTING ENGINE
 */

#ifndef ROUTING_TABLE_SIZE
#define ROUTING_TABLE_SIZE 10 // Number of entries in the routing table
#endif

#ifndef UPDATE_ROUTE_TIMER
#define UPDATE_ROUTE_TIMER 8 // After such interval of time, the route of the node is (re)computed (in seconds)
#endif

#ifndef INVALID_ADDRESS
#define INVALID_ADDRESS 0xFFFF // Value used for the ID of neighbor that is not valid
#endif

/*
 * If the current parent is not congested, a new parent is chosen only if the associated route has an ETX that is at
 * least PARENT_SWITCH_THRESHOLD less than the ETX of the current route
 */

#ifndef PARENT_SWITCH_THRESHOLD
#define PARENT_SWITCH_THRESHOLD 15
#endif

/*
 * Neighbors whose links have a 1-hop ETX bigger than or equal to this threshold can't be selected as parent
 */

#ifndef MAX_ONE_HOP_ETX
#define MAX_ONE_HOP_ETX 50
#endif

#ifndef INFINITE_ETX
#define INFINITE_ETX 0xFFFF // Highest value for ETX => it's used to avoid that neighbor is selected as parent
#endif

/*
 * Minimum value (max frequency) for the interval between two beacons sent (in seconds)
 */

#ifndef MIN_BEACONS_SEND_INTERVAL
#define MIN_BEACONS_SEND_INTERVAL 0.125
#endif

/*
 * Maximum value (min frequency) for the interval between two beacons sent (in seconds)
 */

#ifndef MAX_BEACONS_SEND_INTERVAL
#define MAX_BEACONS_SEND_INTERVAL 500
#endif

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
void parse_routing_engine_parameters(void* event_content);
bool compare_beacons(ctp_routing_frame* a,ctp_routing_frame* b);

#endif