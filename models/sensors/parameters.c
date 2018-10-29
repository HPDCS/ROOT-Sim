/*
 * parameters.c
 *
 *  Created on: 23 ott 2018
 *      Author: andrea
 */

#include <ROOT-Sim.h>
#include <limits.h>

#include "parameters.h"

unsigned ctp_root = 0;

char *config_file_path = NULL;

unsigned long collected_packets_goal = COLLECTED_DATA_PACKETS_GOAL;

double 	max_simulation_time = MAX_TIME,
		failure_lambda = FAILURE_LAMBDA,
		failure_threshold = FAILURE_THRESHOLD;

double white_noise_mean = WHITE_NOISE_MEAN,
		channel_free_threshold = CHANNEL_FREE_THRESHOLD;

unsigned csma_symbols_per_sec = CSMA_SYMBOLS_PER_SEC,
		csma_bits_per_symbol = CSMA_BITS_PER_SYMBOL,
		csma_min_free_samples = CSMA_MIN_FREE_SAMPLES,
		csma_max_free_samples = CSMA_MAX_FREE_SAMPLES,
		csma_high = CSMA_HIGH,
		csma_low = CSMA_LOW,
		csma_init_high = CSMA_INIT_HIGH,
		csma_init_low = CSMA_INIT_LOW,
		csma_rxtx_delay = CSMA_RXTX_DELAY,
		csma_exponent_base = CSMA_EXPONENT_BASE,
		csma_preamble_length = CSMA_PREAMBLE_LENGTH,
		csma_ack_time = CSMA_ACK_TIME;
double csma_sensitivity = CSMA_SENSITIVITY;

unsigned evict_worst_etx_threshold = EVICT_WORST_ETX_THRESHOLD,
		evict_best_etx_threshold = EVICT_BEST_ETX_THRESHOLD,
		max_pkt_gap = MAX_PKT_GAP;
unsigned short alpha = ALPHA,
			dlq_pkt_window = DLQ_PKT_WINDOW,
			blq_pkt_window = BLQ_PKT_WINDOW;

double 	update_route_timer = UPDATE_ROUTE_TIMER,
		min_beacons_send_interval = MIN_BEACONS_SEND_INTERVAL,
		max_beacons_send_interval = MAX_BEACONS_SEND_INTERVAL;
unsigned max_one_hop_etx = MAX_ONE_HOP_ETX,
		parent_switch_threshold = PARENT_SWITCH_THRESHOLD;

unsigned max_retries = MAX_RETRIES,
		min_payload = MIN_PAYLOAD,
		max_payload = MAX_PAYLOAD;
double 	data_packet_transmission_offset = DATA_PACKET_RETRANSMISSION_OFFSET,
		data_packet_transmission_delta = DATA_PACKET_RETRANSMISSION_DELTA,
		no_route_offset = NO_ROUTE_OFFSET,
		send_packet_timer = SEND_PACKET_TIMER,
		create_packet_timer = CREATE_PACKET_TIMER;


enum{
	OPT_ROOT = 128, /// this tells argp to not assign short options
	OPT_INPUT,
	OPT_FLAMBDA,
	OPT_FTHRESH,
	OPT_MAXST,
	OPT_CPGOAL,
	OPT_P_WNMEAN,
	OPT_P_CHTHRESH,
	OPT_C_SPS,
	OPT_C_BPS,
	OPT_C_MINFS,
	OPT_C_MAXFS,
	OPT_C_HIGH,
	OPT_C_LOW,
	OPT_C_IHIGH,
	OPT_C_ILOW,
	OPT_C_RXTXD,
	OPT_C_EXP,
	OPT_C_PLEN,
	OPT_C_ACK,
	OPT_C_SENS,
	OPT_E_WET,
	OPT_E_BET,
	OPT_E_MPG,
	OPT_E_ALPHA,
	OPT_E_DPW,
	OPT_E_BPW,
	OPT_R_URT,
	OPT_R_MINBSI,
	OPT_R_MAXBSI,
	OPT_R_MAXOHE,
	OPT_R_PST,
	OPT_F_MAXR,
	OPT_F_MINP,
	OPT_F_MAXP,
	OPT_F_DPTO,
	OPT_F_DPTD,
	OPT_F_NRO,
	OPT_F_SPT,
	OPT_F_CPT
};

const struct argp_option model_options[] = {
		{NULL, 0, NULL, 0, "General parameters", 1},
		{"root", 					OPT_ROOT, "UINT", 0, NULL, 1},
		{"input", 					OPT_INPUT, "PATH", 0, NULL, 1},
		{"failure-lambda", 			OPT_FLAMBDA, "DOUBLE", 0, NULL, 1},
		{"failure-threshold", 		OPT_FTHRESH, "DOUBLE", 0, NULL, 1},
		{"max-simulation-time",		OPT_MAXST, "DOUBLE", 0, NULL, 1},
		{"collected-packets-goal", 	OPT_CPGOAL, "LONGUINT", 0, NULL, 1},

		{NULL, 0, NULL, 0, "Physical layer parameters", 2},
		{"white-noise-mean", 		OPT_P_WNMEAN, "DOUBLE", 0, NULL, 2},
		{"channel-free-threshold", 	OPT_P_CHTHRESH, "DOUBLE", 0, NULL, 2},

		{NULL, 0, NULL, 0, "Link layer parameters", 3},
		{"csma-symbols-per-sec", 	OPT_C_SPS, "UINT", 0, NULL, 3},
		{"csma-bits-per-symbol", 	OPT_C_BPS, "UINT", 0, NULL, 3},
		{"csma-min-free-samples", 	OPT_C_MINFS, "UINT", 0, NULL, 3},
		{"csma-max-free-samples", 	OPT_C_MAXFS, "UINT", 0, NULL, 3},
		{"csma-high", 				OPT_C_HIGH, "UINT", 0, NULL, 3},
		{"csma-low", 				OPT_C_LOW, "UINT", 0, NULL, 3},
		{"csma-init-high", 			OPT_C_IHIGH, "UINT", 0, NULL, 3},
		{"csma-init-low", 			OPT_C_ILOW, "UINT", 0, NULL, 3},
		{"csma-rxtx-delay", 		OPT_C_RXTXD, "UINT", 0, NULL, 3},
		{"csma-exponent-base", 		OPT_C_EXP, "UINT", 0, NULL, 3},
		{"csma-preamble-length", 	OPT_C_PLEN, "UINT", 0, NULL, 3},
		{"csma-ack-time", 			OPT_C_ACK, "UINT", 0, NULL, 3},
		{"csma-sensitivity", 		OPT_C_SENS, "DOUBLE", 0, NULL, 3},

		{NULL, 0, NULL, 0, "Link estimator parameters", 4},
		{"evict-worst-etx-threshold", OPT_E_WET, "UINT", 0, NULL, 4},
		{"evict-best-etx-threshold", OPT_E_BET, "UINT", 0, NULL, 4},
		{"max-pkt-gap", 			OPT_E_MPG, "UINT", 0, NULL, 4},
		{"alpha", 					OPT_E_ALPHA, "USHORT", 0, NULL, 4},
		{"dlq-pkt-window", 			OPT_E_DPW, "USHORT", 0, NULL, 4},
		{"blq-pkt-window", 			OPT_E_BPW, "USHORT", 0, NULL, 4},

		{NULL, 0, NULL, 0, "Routing engine parameters", 5},
		{"update-route-timer", 		OPT_R_URT, "DOUBLE", 0, NULL, 5},
		{"min-beacons-send-interval", OPT_R_MINBSI, "DOUBLE", 0, NULL, 5},
		{"max-beacons-send-interval", OPT_R_MAXBSI, "DOUBLE", 0, NULL, 5},
		{"max-one-hop-etx", 		OPT_R_MAXOHE, "UINT", 0, NULL, 5},
		{"parent-switch-threshold", OPT_R_PST, "UINT", 0, NULL, 5},

		{NULL, 0, NULL, 0, "Forward engine parameters", 6},
		{"max-retries", 			OPT_F_MAXR, "UINT", 0, NULL, 6},
		{"min-payload", 			OPT_F_MINP, "UINT", 0, NULL, 6},
		{"max-payload", 			OPT_F_MAXP, "UINT", 0, NULL, 6},
		{"data-packet-transmission_offset", OPT_F_DPTO, "DOUBLE", 0, NULL, 6},
		{"data-packet-transmission_delta", OPT_F_DPTD, "DOUBLE", 0, NULL, 6},
		{"no-route-offset", 		OPT_F_NRO, "DOUBLE", 0, NULL, 6},
		{"send-packet-timer", 		OPT_F_SPT, "DOUBLE", 0, NULL, 6},
		{"create-packet-timer", 	OPT_F_CPT, "DOUBLE", 0, NULL, 6},
		{0}
};

#define HANDLE_ARGP_CASE(label, fmt, var)	\
	case label: \
		if(sscanf(arg, fmt, &var) != 1){ \
			return ARGP_ERR_UNKNOWN; \
		} \
	break

static error_t model_parse(int key, char *arg, struct argp_state *state) {
	(void)state;
	switch (key) {

		case OPT_INPUT:
			config_file_path = arg;
			break;

		HANDLE_ARGP_CASE(OPT_ROOT, 		"%u", 	ctp_root);
		HANDLE_ARGP_CASE(OPT_FLAMBDA, 	"%lf", 	failure_lambda);
		HANDLE_ARGP_CASE(OPT_FTHRESH, 	"%lf", 	failure_threshold);
		HANDLE_ARGP_CASE(OPT_MAXST, 	"%lf", 	max_simulation_time);
		HANDLE_ARGP_CASE(OPT_CPGOAL, 	"%lu", 	collected_packets_goal);

		HANDLE_ARGP_CASE(OPT_P_WNMEAN, 	"%lf", 	white_noise_mean);
		HANDLE_ARGP_CASE(OPT_P_CHTHRESH, "%lf", channel_free_threshold);

		HANDLE_ARGP_CASE(OPT_C_SPS, 	"%u", 	csma_symbols_per_sec);
		HANDLE_ARGP_CASE(OPT_C_BPS, 	"%u", 	csma_bits_per_symbol);
		HANDLE_ARGP_CASE(OPT_C_MINFS, 	"%u", 	csma_min_free_samples);
		HANDLE_ARGP_CASE(OPT_C_MAXFS, 	"%u", 	csma_max_free_samples);
		HANDLE_ARGP_CASE(OPT_C_HIGH, 	"%u", 	csma_high);
		HANDLE_ARGP_CASE(OPT_C_LOW, 	"%u",	csma_low);
		HANDLE_ARGP_CASE(OPT_C_IHIGH, 	"%u", 	csma_init_high);
		HANDLE_ARGP_CASE(OPT_C_ILOW, 	"%u", 	csma_init_low);
		HANDLE_ARGP_CASE(OPT_C_RXTXD, 	"%u", 	csma_rxtx_delay);
		HANDLE_ARGP_CASE(OPT_C_EXP,		"%u", 	csma_exponent_base);
		HANDLE_ARGP_CASE(OPT_C_PLEN, 	"%u", 	csma_preamble_length);
		HANDLE_ARGP_CASE(OPT_C_ACK, 	"%u", 	csma_ack_time);
		HANDLE_ARGP_CASE(OPT_C_SENS, 	"%lf", 	csma_sensitivity);

		HANDLE_ARGP_CASE(OPT_E_WET, 	"%u", 	evict_worst_etx_threshold);
		HANDLE_ARGP_CASE(OPT_E_BET, 	"%u", 	evict_best_etx_threshold);
		HANDLE_ARGP_CASE(OPT_E_MPG, 	"%u", 	max_pkt_gap);
		HANDLE_ARGP_CASE(OPT_E_ALPHA, 	"%hu", 	alpha);
		HANDLE_ARGP_CASE(OPT_E_DPW, 	"%hu", 	dlq_pkt_window);
		HANDLE_ARGP_CASE(OPT_E_BPW, 	"%hu", 	blq_pkt_window);

		HANDLE_ARGP_CASE(OPT_R_URT, 	"%lf", 	update_route_timer);
		HANDLE_ARGP_CASE(OPT_R_MINBSI, 	"%lf", 	min_beacons_send_interval);
		HANDLE_ARGP_CASE(OPT_R_MAXBSI, 	"%lf", 	max_beacons_send_interval);
		HANDLE_ARGP_CASE(OPT_R_MAXOHE, 	"%u", 	max_one_hop_etx);
		HANDLE_ARGP_CASE(OPT_R_PST, 	"%u", 	parent_switch_threshold);

		HANDLE_ARGP_CASE(OPT_F_MAXR, 	"%u", 	max_retries);
		HANDLE_ARGP_CASE(OPT_F_MINP, 	"%u", 	min_payload);
		HANDLE_ARGP_CASE(OPT_F_MAXP, 	"%u", 	max_payload);
		HANDLE_ARGP_CASE(OPT_F_DPTO, 	"%lf", 	data_packet_transmission_offset);
		HANDLE_ARGP_CASE(OPT_F_DPTD, 	"%lf", 	data_packet_transmission_delta);
		HANDLE_ARGP_CASE(OPT_F_NRO, 	"%lf", 	no_route_offset);
		HANDLE_ARGP_CASE(OPT_F_SPT, 	"%lf", 	send_packet_timer);
		HANDLE_ARGP_CASE(OPT_F_CPT, 	"%lf", 	create_packet_timer);

		case ARGP_KEY_SUCCESS:
			if(!config_file_path)
				argp_failure(state, EXIT_FAILURE, 0, "[FATAL ERROR] The path to a file "
						"containing the configuration  of the network is mandatory => "
						"specify it after the option \"--input\"\n");

			if(ctp_root >= n_prc_tot)
				argp_failure(state, EXIT_FAILURE, 0, "[FATAL ERROR] The given root ID "
						"is not valid: it has to be less that the number of LPs\n");
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef HANDLE_ARGP_CASE

struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};

/* READ INPUT FILE (ONLY THE ROOT NODE) - end */
