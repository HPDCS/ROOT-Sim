#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include "application.h"

bool 	pcs_statistics = false,
	approximated = false,
	fading_check = false,  // Is the model set up to periodically recompute the fading of all ongoing calls?
	variable_ta = false; // Should the call interarrival frequency change depending on the current time?
unsigned complete_calls = COMPLETE_CALLS,
	channels_per_cell = CHANNELS_PER_CELL; // Total channels per each cell
double 	ref_ta = TA,   // Initial call interarrival frequency (same for all cells)
	ta_duration = TA_DURATION, // Average duration of a call
	ta_change = TA_CHANGE; // Average time after which a call is diverted to another cell

enum {
	OPT_STAT = 128, /// this tells argp to not assign short options
	OPT_TA,
	OPT_TAD,
	OPT_TAC,
	OPT_CPC,
	OPT_CC,
	OPT_FR,
	OPT_VTA,
	OPT_APX,
};

const struct argp_option model_options[] = {
		{"pcs-statistics", OPT_STAT, NULL, 0, NULL, 0},
		{"ta", OPT_TA, "FLOAT", 0, NULL, 0},
		{"ta-duration", OPT_TAD, "FLOAT", 0, NULL, 0},
		{"ta-change", OPT_TAC, "FLOAT", 0, NULL, 0},
		{"channels-per-cell", OPT_CPC, "UINT", 0, NULL, 0},
		{"complete-calls", OPT_CC, "INT", 0, NULL, 0},
		{"fading-recheck", OPT_FR, NULL, 0, NULL, 0},
		{"variable-ta", OPT_VTA, NULL, 0, NULL, 0},
		{"approximated", OPT_APX, NULL, 0, NULL, 0},
		{0}
};

// this macro abuse looks so elegant though...
#define HANDLE_CASE(label, fmt, var)	\
	case label: \
		if(sscanf(arg, fmt, &var) != 1){ \
			return ARGP_ERR_UNKNOWN; \
		} \
	break

static error_t model_parse (int key, char *arg, struct argp_state *state) {
	(void)state;
	
	switch (key) {
		HANDLE_CASE(OPT_TA, "%lf", ref_ta);
		HANDLE_CASE(OPT_TAD, "%lf", ta_duration);
		HANDLE_CASE(OPT_TAC, "%lf", ta_change);
		HANDLE_CASE(OPT_CPC, "%u", channels_per_cell);
		HANDLE_CASE(OPT_CC, "%u", complete_calls);

		case OPT_STAT:
			pcs_statistics = true;
			break;
		case OPT_FR:
			fading_check = true;
			break;;
		case OPT_VTA:
			variable_ta = true;
			break;
		case OPT_APX:
			approximated = true;
			break;

		case ARGP_KEY_SUCCESS:
			printf("CURRENT CONFIGURATION:\ncomplete calls: %d\nTA: %f\nta_duration: %f\nta_change: %f\nchannels_per_cell: %d\nfading_recheck: %d\nvariable_ta: %d\napproximated: %d\n",
				complete_calls, ref_ta, ta_duration, ta_change, channels_per_cell, fading_check, variable_ta, approximated);
			fflush(stdout);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef HANDLE_CASE

struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};

struct _topology_settings_t topology_settings = {.default_geometry = TOPOLOGY_HEXAGON};

void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *ptr) {
	(void)size;
	
	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.channel = -1;
	new_event_content.call_term_time = -1;

	simtime_t handoff_time;
	simtime_t timestamp = 0;

	lp_state_type *state;
	state = (lp_state_type*)ptr;

	switch(event_type) {

		case INIT:

			// Initialize the LP's state
			state = (lp_state_type *)malloc(sizeof(lp_state_type));
			if (state == NULL){
				printf("Out of memory!\n");
				exit(EXIT_FAILURE);
			}
			SetState(state);
			bzero(state, sizeof(lp_state_type));

			state->channel_counter = channels_per_cell;
			state->channels = malloc(sizeof(channel) * channels_per_cell);

			// Setup channel state
			state->core_data = malloc(sizeof(struct core_data_t));
			bzero(state->core_data, sizeof(struct core_data_t));
			
			state->core_data->channel_state = malloc(sizeof(unsigned int) * (CHANNELS_PER_CELL / BITS + 1));
			bzero(state->core_data->channel_state, sizeof(unsigned int) * (CHANNELS_PER_CELL / BITS + 1));
                        state->core_data->ta = ref_ta;

                        if(approximated){
                        	CoreMemoryMark(state->core_data);
                        	CoreMemoryMark(state->core_data->channel_state);
                        	RollbackModeSet(true);
                        }

			// Start the simulation
			timestamp = (simtime_t) (20 * Random());
			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);

			// fading recheck
			timestamp = (simtime_t) (FADING_RECHECK_FREQUENCY * Random());
			ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
			

			break;


		case START_CALL:

			state->arriving_calls++;

			if (variable_ta)
				state->core_data->ta = recompute_ta(ref_ta, now);

			// Determine the time at which a new call will be issued
			switch (DISTRIBUTION) {

				case UNIFORM:
					timestamp = now + (simtime_t)(state->core_data->ta * Random());
					break;

				case EXPONENTIAL:
					timestamp = now + (simtime_t)(Expent(state->core_data->ta));
					break;

				default:
					timestamp = now + (simtime_t) (5 * Random());

			}

			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);

			if (state->channel_counter == 0) {
				state->blocked_on_setup++;
				break;
			}

			state->channel_counter--;

			new_event_content.channel = allocation(state);
			new_event_content.from = me;
			new_event_content.sent_at = now;

			// Determine call duration
			switch (DURATION_DISTRIBUTION) {

				case UNIFORM:
					new_event_content.call_term_time = now + (simtime_t)(ta_duration * Random());
					break;

				case EXPONENTIAL:
					new_event_content.call_term_time = now + (simtime_t)(Expent(ta_duration));
					break;

				default:
					new_event_content.call_term_time = now + (simtime_t) (5 * Random() );
			}

			// Determine whether the call will be handed-off or not
			switch (CELL_CHANGE_DISTRIBUTION) {

				case UNIFORM:

					handoff_time  = now + (simtime_t)((ta_change) * Random());
					break;

				case EXPONENTIAL:
					handoff_time = now + (simtime_t)(Expent(ta_change));
					break;

				default:
					handoff_time = now + (simtime_t)(5 * Random());

			}

			if(new_event_content.call_term_time < handoff_time) {
				ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));
			} else {
				new_event_content.cell = FindReceiver();
				ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));
			}


			break;

		case END_CALL:

			state->channel_counter++;
			state->core_data->complete_calls++ ;
			deallocation(state, event_content->channel);

			break;

		case HANDOFF_LEAVE:

			state->channel_counter++;
			state->leaving_handoffs++;
			deallocation(state, event_content->channel);

			new_event_content.call_term_time =  event_content->call_term_time;
			new_event_content.from = me;
			ScheduleNewEvent(event_content->cell, now, HANDOFF_RECV, &new_event_content, sizeof(new_event_content));
			break;

		case HANDOFF_RECV:
			state->arriving_handoffs++;
			state->arriving_calls++;

			if (state->channel_counter == 0)
				state->blocked_on_handoff++;
			else {
				state->channel_counter--;

				new_event_content.channel = allocation(state);
				new_event_content.call_term_time = event_content->call_term_time;


				switch (CELL_CHANGE_DISTRIBUTION) {
					case UNIFORM:
						handoff_time  = now + (simtime_t)((ta_change) * Random());

						break;
					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent(ta_change));

						break;
					default:
						handoff_time = now + (simtime_t) (5 * Random());
				}

				if(new_event_content.call_term_time < handoff_time ) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver();
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));
				}
			}


			break;

		case FADING_RECHECK:

			fading_recheck(state);
			timestamp = now + (simtime_t) (FADING_RECHECK_FREQUENCY );
			ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
			break;

		default:
			fprintf(stdout, "PCS: Unknown event type! (me = %d - event type = %d)\n", me, event_type);
			abort();

	}
}

void RestoreApproximated(void *ptr) {
	lp_state_type *state = (lp_state_type*)ptr;
	unsigned occupied = reallocate_channels(state);
	state->channel_counter = channels_per_cell - occupied;
}

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void)me;

	if (snapshot->core_data->complete_calls < complete_calls)
		return false;
	return true;
}
