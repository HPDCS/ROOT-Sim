#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>

#include "application.h"

static unsigned max_buffers = MAX_BUFFERS;
static unsigned max_buffer_size = MAX_BUFFER_SIZE;
static unsigned complete_events = COMPLETE_EVENTS;
static bool 	new_mode = true,
		approximated = true;
static double 	tau = TAU,
		send_probability = SEND_PROBABILITY,
		alloc_probability = ALLOC_PROBABILITY,
		dealloc_probability = DEALLOC_PROBABILITY;

enum{
	OPT_MAX_BUFFERS = 128, /// this tells argp to not assign short options
	OPT_MAX_BUFFER_SIZE,
	OPT_TAU,
	OPT_SENDP,
	OPT_ALLOCP,
	OPT_DEALLOCP,
	OPT_EVT,
	OPT_MODE,
	OPT_PREC
};

const struct argp_option model_options[] = {
		{"max-buffers", 	OPT_MAX_BUFFERS, "INT", 0, NULL, 0},
		{"max-buffer-size", 	OPT_MAX_BUFFER_SIZE, "INT", 0, NULL, 0},
		{"tau", 		OPT_TAU, "DOUBLE", 0, NULL, 0},
		{"send-probability", 	OPT_SENDP, "DOUBLE", 0, NULL, 0},
		{"alloc-probability", 	OPT_ALLOCP, "DOUBLE", 0, NULL, 0},
		{"dealloc-probability", OPT_DEALLOCP, "DOUBLE", 0, NULL, 0},
		{"complete-events", 	OPT_EVT, "INT", 0, NULL, 0},
		{"old-mode", 		OPT_MODE, NULL, 0, NULL, 0},
		{"precise-mode", OPT_PREC, NULL, 0, NULL, 0},
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

		HANDLE_ARGP_CASE(OPT_MAX_BUFFERS, 	"%u", 	max_buffers);
		HANDLE_ARGP_CASE(OPT_MAX_BUFFER_SIZE, 	"%u", 	max_buffer_size);
		HANDLE_ARGP_CASE(OPT_TAU, 		"%lf", 	tau);
		HANDLE_ARGP_CASE(OPT_SENDP, 		"%lf", 	send_probability);
		HANDLE_ARGP_CASE(OPT_ALLOCP, 		"%lf", 	alloc_probability);
		HANDLE_ARGP_CASE(OPT_DEALLOCP, 		"%lf", 	dealloc_probability);
		HANDLE_ARGP_CASE(OPT_EVT, 		"%u", 	complete_events);

		case OPT_MODE:
			new_mode = false;
			break;

		case OPT_PREC:
			approximated = false;
		break;

		case ARGP_KEY_SUCCESS:
			printf("\t* ROOT-Sim's PHOLD Benchmark - Current Configuration *\n");
			printf("old-mode: %s\n"
				"max-buffers: %u\n"
				"max-buffer-size: %u\n"
				"tau: %lf\n"
				"send-probability: %lf\n"
				"alloc-probability: %lf\n"
				"dealloc-probability: %lf\n"
				"approximated: %d\n",
				new_mode ? "false" : "true",
				max_buffers, max_buffer_size, tau,
				send_probability, alloc_probability, dealloc_probability, approximated);
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef HANDLE_ARGP_CASE

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .default_geometry = TOPOLOGY_GRAPH, .write_enabled = false};
struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};


void ProcessEvent(unsigned me, simtime_t now, int event_type, unsigned *event_content, unsigned int event_size, void *state) {
	lp_state_type *state_ptr = (lp_state_type *)state;

	switch (event_type) {

		case INIT:
			state_ptr = malloc(sizeof(lp_state_type));
                        if(state_ptr == NULL){
                                exit(-1);
                        }
                        memset(state_ptr, 0, sizeof(lp_state_type));

                        SetState(state_ptr);

			if(new_mode) {
				unsigned buffers_to_allocate = (unsigned)(Random() * max_buffers);

				unsigned robba_allocata = 0;
				for (unsigned i = 0; i < buffers_to_allocate; i++) {
					state_ptr->head = allocate_buffer(state_ptr->head, NULL, (unsigned)(Random() * max_buffer_size) / sizeof(unsigned));
					state_ptr->buffer_count++;
					robba_allocata += state_ptr->head->count;
				}

				while (robba_allocata < buffers_to_allocate * max_buffer_size) {
                                        state_ptr->head = allocate_buffer(state_ptr->head, NULL, (unsigned)(Random() * max_buffer_size) / sizeof(unsigned));
                                        state_ptr->buffer_count++;
                                        robba_allocata += state_ptr->head->count;
				}
			}

			RollbackModeSet(approximated);

			ScheduleNewEvent(me, 20 * Random(), LOOP, NULL, 0);
			break;


		case LOOP:
			state_ptr->events++;
			simtime_t timestamp = now + (Expent(tau));
			ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			if(Random() < 0.2)
				ScheduleNewEvent(FindReceiver(), timestamp, LOOP, NULL, 0);

			if(!new_mode){
				volatile unsigned j = 0;
				for(unsigned i = 0; i < LOOP_COUNT; i++) {
					j = i;
				}
				break;
			}

			if(state_ptr->buffer_count)
				state_ptr->total_checksum ^= read_buffer(state_ptr->head, (unsigned)(Random() * state_ptr->buffer_count));

			if(state_ptr->buffer_count < max_buffers && Random() < alloc_probability) {
				state_ptr->head = allocate_buffer(state_ptr->head, NULL, (unsigned)(Random() * max_buffer_size) / sizeof(unsigned));
				state_ptr->buffer_count++;
			}

			if(state_ptr->buffer_count && Random() < dealloc_probability) {
				state_ptr->head = deallocate_buffer(state_ptr->head, (unsigned)(Random() * state_ptr->buffer_count));
				state_ptr->buffer_count--;
			}

			if(state_ptr->buffer_count && Random() < send_probability) {
				unsigned i = (unsigned)(Random() * state_ptr->buffer_count);
				buffer *to_send = get_buffer(state_ptr->head, i);
				timestamp = now + (Expent(tau));

				ScheduleNewEvent(FindReceiver(), timestamp, RECEIVE, to_send->data, to_send->count * sizeof(unsigned));

				state_ptr->head = deallocate_buffer(state_ptr->head, i);
				state_ptr->buffer_count--;
			}
			break;

		case RECEIVE:
			if(state_ptr->buffer_count >= max_buffers)
				break;
			state_ptr->head = allocate_buffer(state_ptr->head, event_content, event_size / sizeof(unsigned));
			state_ptr->buffer_count++;
			break;

		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			break;
	}
}

void RestoreApproximated(void *ptr) {
	lp_state_type *state = (lp_state_type*)ptr;
	unsigned i = state->buffer_count;
	buffer *tmp = state->head;

	while(i--) {
            for(unsigned j = 0; j < tmp->count; j++) {
                tmp->data[j] = state->buffer_count * 10;//RandomRange(0, INT_MAX);
            }
            tmp = tmp->next;
	}

}

bool OnGVT(unsigned me, lp_state_type *snapshot) {
	if(snapshot->events >= complete_events){
#ifndef NDEBUG
		if(new_mode)
			printf("[LP %u] total_checksum = %u\n", me, snapshot->total_checksum);
#endif
		return true;
	}
	return false;
}

