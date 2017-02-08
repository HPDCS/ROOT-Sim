#ifndef SENSORSNETWORKMODELPROJECT_PHYSICAL_LAYER_H
#define SENSORSNETWORKMODELPROJECT_PHYSICAL_LAYER_H

#include <stdbool.h>
#include "application.h"

/*
 * PHYSICAL LAYER PARAMETERS' DEFAULT VALUES
 */

#ifndef WHITE_NOISE_MEAN
#define WHITE_NOISE_MEAN 0 // The white noise has a gaussian distribution with the mean value given by this parameter
#endif

/*
 * If the strength of the signal perceived is below this threshold, the channel is considered free; the value of this
 * constant is the same used for the CC2420 radio
 */

#ifndef CHANNEL_FREE_THRESHOLD
#define CHANNEL_FREE_THRESHOLD -95
#endif


void init_physical_layer(node_state* state);
void parse_physical_layer_parameters(void* event_content);
pending_transmission* create_pending_transmission(unsigned char type,void* frame, double power,bool lost);
void add_gain_entry(unsigned int source, unsigned int sink, double gain);
void add_noise_entry(unsigned int node, double noise_floor, double white_noise);
void check_gains_list();
void check_noises_list();
double compute_signal_strength(node_state* state);
bool is_channel_free(node_state* state);
void transmit_frame(node_state* state,unsigned char type);
void transmission_finished(node_state* state,pending_transmission* finished_transmission);
#endif //SENSORSNETWORKMODELPROJECT_PHYSICAL_LAYER_H
