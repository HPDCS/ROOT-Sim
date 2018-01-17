#ifndef SENSORSNETWORKMODELPROJECT_LINK_LAYER_H
#define SENSORSNETWORKMODELPROJECT_LINK_LAYER_H

#include "application.h"

/*
 * PARAMETERS OF THE CARRIER SENSE MULTIPLE ACCESS PROTOCOL (CSMA) - start
 */

#ifndef CSMA_SYMBOLS_PER_SEC
#define CSMA_SYMBOLS_PER_SEC 65536 // Number of symbols per second (baud rate)
#endif

#ifndef CSMA_BITS_PER_SYMBOL
#define CSMA_BITS_PER_SYMBOL 4 // Number of bits per symbol
#endif

#ifndef CSMA_MIN_FREE_SAMPLES
#define CSMA_MIN_FREE_SAMPLES 1 // Number of times the node "sees" the channel free before it starts sending
#endif

/*
 * Upper bound for the number of times the node "sees" the channel free before it starts sending
 */

#ifndef CSMA_MAX_FREE_SAMPLES
#define CSMA_MAX_FREE_SAMPLES 0
#endif

/*
 * Upper bound of the backoff range (in symbols). It is multiplied by the exponent base to the n-th power,where n is the
 * number of times the node has already backed off => after the first backoff the upper bound of the range is
 * CSMA_HIGH*CSMA_EXPONENT_BASE, after the second one it is CSMA_HIGH*CSMA_EXPONENT_BASE*CSMA_EXPONENT_BASE
 */

#ifndef CSMA_HIGH
#define CSMA_HIGH 160
#endif

/*
 * Lower bound of the backoff range (in symbols). It is multiplied by the exponent base to the n-th power,where n is the
 * number of times the node has already backed off => after the first backoff the lower bound of the range is
 * CSMA_LOW*CSMA_EXPONENT_BASE, after the second one it's CSMA_LOW*CSMA_EXPONENT_BASE*CSMA_EXPONENT_BASE
 */

#ifndef CSMA_LOW
#define CSMA_LOW 20
#endif

#ifndef CSMA_INIT_HIGH
#define CSMA_INIT_HIGH 640 // Upper bound of the initial range for the backoff (in symbols)
#endif

#ifndef CSMA_INIT_LOW
#define CSMA_INIT_LOW 20 // Lower bound of the initial range for the backoff (in symbols)
#endif

/*
 * Time needed by the radio transceiver to switch from Transmission (TX) to Reception (RX) and vice-versa, expressed in
 * symbols (500 us ~= 32 symbols)
 */

#ifndef CSMA_RXTX_DELAY
#define CSMA_RXTX_DELAY 11
#endif

/*
 * Base of the exponent used to calculate the backoff; if equal to 1, the range where the random value of the backoff
 * time is selected is fixed
 */

#ifndef CSMA_EXPONENT_BASE
#define CSMA_EXPONENT_BASE 1
#endif

/*
 * Number of symbols corresponding to the preamble that precedes every frame transmitted by the radio (in accordance
 * with the IEEE 802.15.4 standard). This comprises three parts:
 *
 * PREAMBLE SEQUENCE (4 bytes) | START OF FRAME DELIMITER (1 byte) | FRAME LENGTH (1 byte)
 *
 * The total length of the preamble is 6 bytes = 48 bits => since each symbol corresponds to 4 bits, the length of the
 * preamble in symbols is 48/4=12
 */

#ifndef CSMA_PREAMBLE_LENGTH
#define CSMA_PREAMBLE_LENGTH 12
#endif

/*
 * In accordance with the IEEE 802.15.4 standard, an acknowledgement frame is transmitted by the receiver 12 symbol
 * periods after the last symbol of the incoming frame. Its format includes a 6 bytes preamble and 5 bytes MAC PROTOCOL
 * DATA UNIT (MPDU), so the size of an acknowledgment is 11 bytes = 88 bits =>  since each symbol corresponds to 4 bits,
 * the length in symbols is 22. Adding the 12 symbols delay, the total number of symbols for the reception of an ack is
 * 34
 */

#ifndef CSMA_ACK_TIME
#define CSMA_ACK_TIME 34
#endif

/*
 * The strength of a signal has to be weaker by at most this value than the strength of the interferences for the signal
 * to be received by the radio transceiver of a node
 */

#ifndef CSMA_SENSITIVITY
#define CSMA_SENSITIVITY 4
#endif

/*
 * PARAMETERS OF THE CARRIER SENSE MULTIPLE ACCESS PROTOCOL (CSMA) - end
 */

void start_frame_transmission(node_state* state);
bool send_frame(node_state* state,unsigned int recipient, unsigned char type);
void frame_transmitted(node_state* state);
void frame_received(node_state* state,void* frame, unsigned char type);
void check_channel(node_state* state);
void init_link_layer(node_state* state);
void parse_link_layer_parameters(void* event_content);
bool compare_link_layer_frames(link_layer_frame* a,link_layer_frame* b);
#endif //SENSORSNETWORKMODELPROJECT_LINK_LAYER_H
