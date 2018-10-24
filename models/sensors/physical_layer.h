#ifndef SENSORSNETWORKMODELPROJECT_PHYSICAL_LAYER_H
#define SENSORSNETWORKMODELPROJECT_PHYSICAL_LAYER_H

#include <stdbool.h>
#include "application.h"

void init_physical_layer(node_state* state);
pending_transmission* create_pending_transmission(unsigned char type,void* frame, double power,bool lost);
void add_gain_entry(unsigned int source, unsigned int sink, double gain);
void add_noise_entry(unsigned int node, double noise_floor, double white_noise);
void check_gains_list(void);
void check_noises_list(void);
double compute_signal_strength(node_state* state);
bool is_channel_free(node_state* state);
void transmit_frame(node_state* state,unsigned char type);
void transmission_finished(node_state* state,pending_transmission* finished_transmission);
#endif //SENSORSNETWORKMODELPROJECT_PHYSICAL_LAYER_H
