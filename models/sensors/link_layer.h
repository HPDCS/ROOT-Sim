#ifndef SENSORSNETWORKMODELPROJECT_LINK_LAYER_H
#define SENSORSNETWORKMODELPROJECT_LINK_LAYER_H

#include "application.h"

void start_frame_transmission(node_state* state);
bool send_frame(node_state* state,unsigned int recipient, unsigned char type);
void frame_transmitted(node_state* state);
void frame_received(node_state* state,void* frame, unsigned char type);
void check_channel(node_state* state);
void init_link_layer(node_state* state);
bool compare_link_layer_frames(link_layer_frame* a,link_layer_frame* b);
#endif //SENSORSNETWORKMODELPROJECT_LINK_LAYER_H
