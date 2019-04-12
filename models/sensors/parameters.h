/*
 * parameters.h
 *
 *  Created on: 23 ott 2018
 *      Author: andrea
 */

#ifndef MODELS_SENSORS_PARAMETERS_H_
#define MODELS_SENSORS_PARAMETERS_H_

/*
 * PARAMETERS OF THE SIMULATION - start
 */

/*
 * FAILURE LAMBDA
 *
 * The lambda parameter of the exponential failure distribution: it depends on the devices used as node of the collection
 * tree and is equivalent to the failure rate or MTTF(Mean Time To Failure)
 */

#ifndef FAILURE_LAMBDA
#define FAILURE_LAMBDA 0.0005
#endif

/*
 * FAILURE PROBABILITY THRESHOLD
 *
 * The exponential failure distribution tells the probability that a failure occurs before a certain time =>
 * the following parameter determines which is the minimum probability for the node to be considered as failed by the
 * simulator
 */

#ifndef FAILURE_THRESHOLD
#define FAILURE_THRESHOLD 0.9
#endif

/*
 * MAX SIMULATION TIME
 *
 * Maximum value for the simulation time: when reached, the simulation stops (in seconds)
 */

#ifndef MAX_TIME
#define MAX_TIME 10
#endif

/*
 * COLLECTED DATA PACKETS GOAL
 *
 * Lower bound of data packets received by the root from each node for the simulation to stop
 */

#ifndef COLLECTED_DATA_PACKETS_GOAL
#define COLLECTED_DATA_PACKETS_GOAL 10
#endif

/*
 * PARAMETERS OF THE SIMULATION - end
 */

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


/*
 * PARAMETERS RELATED TO THE LINK ESTIMATOR
 */

#ifndef NEIGHBOR_TABLE_SIZE
#define NEIGHBOR_TABLE_SIZE 10 // Number of entries in the link estimator table (aka neighbor table)
#endif

/*
 * If a node has an 1-hop ETX bigger than this threshold, it is evicted from the estimator table in case a new entry has
 * to added and the table itself is full
 */

#ifndef EVICT_WORST_ETX_THRESHOLD
#define EVICT_WORST_ETX_THRESHOLD 65
#endif

/*
 * If a node has an 1-hop ETX bigger than this threshold, it is evicted from the estimator table if a new entry has to
 * be added and the table itself is full AND A FREE PLACE FOR THE ROOT NODE HAS TO BE FOUND
 *
 * Since the root is the most important, it's crucial to create an entry for it when a beacon by it is received  => if
 * the table is full, another node has to be replaced => with such a tighter threshold, which corresponds to one hop
 * (recall that ETX is about ten times the number of hops), we are likely to find a victim node
 */

#ifndef EVICT_BEST_ETX_THRESHOLD
#define EVICT_BEST_ETX_THRESHOLD 10
#endif

/*
 * If the number of beacons lost from a neighbor is bigger than this value, the entry for the neighbor is reinitialized
 */

#ifndef MAX_PKT_GAP
#define MAX_PKT_GAP 10
#endif

/*
 * If it's not possible to compute the link quality, the 1-hop ETX is set to the highest value as possible, so that the
 * corresponding node will never be chosen as parent
 */

#ifndef VERY_LARGE_ETX_VALUE
#define VERY_LARGE_ETX_VALUE 0xffff
#endif

#ifndef ALPHA
#define ALPHA 9 // The link estimation is exponentially decayed with this parameter ALPHA
#endif

#ifndef DLQ_PKT_WINDOW
#define DLQ_PKT_WINDOW 5 // # of packets to be sent before updating the outgoing quality of the link to a neighbor
#endif

#ifndef BLQ_PKT_WINDOW
#define BLQ_PKT_WINDOW 3 // # of beacons to be received before updating the ingoing quality of the link to a neighbor
#endif

#ifndef INVALID_ENTRY
#define INVALID_ENTRY 0xff // Value returned when the entry corresponding to a neighbor is not found
#endif


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


/*
 * ID of the node chosen as root of the collection tree => all the data packets will (hopefully) be collected by this
 * node.
 * If the ID of the node is not specified as parameter of the simulation, the default root is the node with ID=0
 */

extern unsigned ctp_root;

extern char *config_file_path;

extern unsigned long collected_packets_goal;

extern double max_simulation_time,
		failure_lambda,
		failure_threshold;

extern double white_noise_mean,
		channel_free_threshold;

extern unsigned csma_symbols_per_sec,
		csma_bits_per_symbol,
		csma_min_free_samples,
		csma_max_free_samples,
		csma_high,
		csma_low,
		csma_init_high,
		csma_init_low,
		csma_rxtx_delay,
		csma_exponent_base,
		csma_preamble_length,
		csma_ack_time;
extern double csma_sensitivity;

extern unsigned evict_worst_etx_threshold,
		evict_best_etx_threshold,
		max_pkt_gap;
extern unsigned short alpha,
		dlq_pkt_window,
		blq_pkt_window;

extern double update_route_timer,
		min_beacons_send_interval,
		max_beacons_send_interval;
extern unsigned max_one_hop_etx,
		parent_switch_threshold;

extern unsigned max_retries,
		min_payload,
		max_payload;
extern double data_packet_transmission_offset,
		data_packet_transmission_delta,
		no_route_offset,
		send_packet_timer,
		create_packet_timer;


#endif /* MODELS_SENSORS_PARAMETERS_H_ */
