#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>

#include "application.h"

enum{
	OPT_OTS = 128, /// this tells argp to not assign short options
	OPT_TSD,
	OPT_MAXS,
	OPT_MINS,
	OPT_NB,
	OPT_CA,
	OPT_RC,
	OPT_WC,
	OPT_WD,
	OPT_RD,
	OPT_TAU
};

const struct argp_option model_options[] = {
		{"object-total-size", 		OPT_OTS, "INT", 0, NULL, 0},
		{"time-stamp-distribution", OPT_TSD, "INT", 0, NULL, 0},
		{"max-size", 				OPT_MAXS, "INT", 0, NULL, 0},
		{"min-size", 				OPT_MINS, "INT", 0, NULL, 0},
		{"num-buffers",				OPT_NB, "INT", 0, NULL, 0},
		{"complete-alloc", 			OPT_CA, "INT", 0, NULL, 0},
		{"read-correction", 		OPT_RC, "INT", 0, NULL, 0},
		{"write-correction", 		OPT_WC, "DOUBLE", 0, NULL, 0},
		{"write-distribution", 		OPT_WD, "DOUBLE", 0, NULL, 0},
		{"read-distribution", 		OPT_RD, "DOUBLE", 0, NULL, 0},
		{"tau", 					OPT_TAU, "DOUBLE", 0, NULL, 0},
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

		HANDLE_ARGP_CASE(OPT_OTS, 	"%d", 	object_total_size);
		HANDLE_ARGP_CASE(OPT_TSD, 	"%d", 	timestamp_distribution);
		HANDLE_ARGP_CASE(OPT_MAXS, 	"%d", 	max_size);
		HANDLE_ARGP_CASE(OPT_MINS, 	"%d", 	min_size);
		HANDLE_ARGP_CASE(OPT_NB, 	"%d", 	num_buffers);
		HANDLE_ARGP_CASE(OPT_CA, 	"%u", 	complete_alloc);
		HANDLE_ARGP_CASE(OPT_RC, 	"%d", 	read_correction);
		HANDLE_ARGP_CASE(OPT_WC, 	"%d", 	write_correction);
		HANDLE_ARGP_CASE(OPT_WD, 	"%lf", 	write_distribution);
		HANDLE_ARGP_CASE(OPT_RD, 	"%lf", 	read_distribution);
		HANDLE_ARGP_CASE(OPT_TAU, 	"%lf", 	tau);

		case ARGP_KEY_SUCCESS:
			printf("\t* ROOT-Sim's PHOLD Benchmark - Current Configuration *\n");
			printf("object_total_size: %d\n"
					"timestamp_distribution:%d\n"
					"max_size: %d\n"
					"min_size: %d\n"
					"num_buffers: %d\n"
					"complete_alloc:%d\n"
					"write_distribution: %f\n"
					"read_distribution: %f\n"
					"tau: %f\n"
					"write-correction: %d\n", object_total_size, timestamp_distribution, max_size, min_size,
					num_buffers, complete_alloc, write_distribution, read_distribution, tau, write_correction);
			printf("\n");
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef HANDLE_ARGP_CASE

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .default_geometry = TOPOLOGY_GRAPH, .write_enabled = false};
struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};

// These global variables are used to store execution configuration values
// They are initialised to some default values but then the initialization could change those
int	object_total_size = OBJECT_TOTAL_SIZE,
	timestamp_distribution = EXPO,
	max_size = MAX_SIZE,
	min_size = MIN_SIZE,
	num_buffers = NUM_BUFFERS,
	read_correction = NO_DISTR,
	write_correction = NO_DISTR;
unsigned int complete_alloc = COMPLETE_ALLOC;
double	write_distribution = WRITE_DISTRIBUTION,
	read_distribution = READ_DISTRIBUTION,
	tau = TAU;


void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {
	(void)size;

	simtime_t timestamp;
	int 	i, j,
		curr_num_buff;
	double	current_size,
		remaining_size,
		step;
	event_content_type new_event;

	lp_state_type *state_ptr = (lp_state_type*)state;


	switch (event_type) {

		case INIT:

			// Initialize LP's state
			state_ptr = (lp_state_type *)malloc(sizeof(lp_state_type));
                        if(state_ptr == NULL){
                                exit(-1);
                        }

			// Explicitly tell ROOT-Sim this is our LP's state
                        SetState(state_ptr);

			timestamp = (simtime_t) (20 * Random());


			if(1 /*|| IsParameterPresent(event_content, "traditional")*/) {

				state_ptr->traditional = true;

				/*if(!IsParameterPresent(event_content, "counter")) {
					printf("Error: cannot run a traditional PHOLD benchmark if `counter` is not set!\n");
					exit(EXIT_FAILURE);
				}*/

//				state_ptr->loop_counter = GetParameterInt(event_content, "counter");
				state_ptr->events = 0;

				if(me == 0) {
					printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP\n", LOOP_COUNT, COMPLETE_EVENTS);
				}

				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			} else {

				state_ptr->traditional = false;
				state_ptr->actual_size = 0;
				state_ptr->num_elementi = 0;
				state_ptr->total_size = 0;
				state_ptr->next_lp = 0;

				// Allocate memory for counters and pointers
				state_ptr->taglie = malloc(num_buffers * sizeof(int));
				state_ptr->elementi = malloc(num_buffers * sizeof(int));
				state_ptr->head_buffs = malloc(num_buffers * sizeof(buffers *));
				state_ptr->tail_buffs = malloc(num_buffers * sizeof(buffers *));

				if(num_buffers > 1)
					step = (max_size - min_size) / (num_buffers - 1);
				else
					step = 0; // the only element will have min_size size

				current_size = min_size;
				remaining_size = object_total_size;

				// Allocate memory for buffers
				for (i = 0; i < num_buffers; i++){

					state_ptr->head_buffs[i] = NULL;
					state_ptr->tail_buffs[i] = NULL;
					state_ptr->taglie[i] = (int)current_size;


					curr_num_buff = (int)ceil(remaining_size / num_buffers / current_size);

					if(curr_num_buff == 0)
						curr_num_buff = 1;

					state_ptr->elementi[i] = curr_num_buff;

					state_ptr->num_elementi += curr_num_buff;
					state_ptr->total_size += (current_size * curr_num_buff);

					printf("[%d, %d] ", curr_num_buff, (int)current_size);

					state_ptr->head_buffs[i] = malloc(sizeof(buffers));
					state_ptr->head_buffs[i]->prev = NULL;
					state_ptr->head_buffs[i]->next = NULL;
					state_ptr->head_buffs[i]->buffer = malloc((int)current_size);
					state_ptr->tail_buffs[i] = state_ptr->head_buffs[i];

					for(j = 0; j < curr_num_buff - 1; j++) {
						buffers *tmp = malloc(sizeof(buffers));
						tmp->prev = state_ptr->tail_buffs[i];
						tmp->buffer = malloc((int)current_size);
						tmp->next = NULL;
						state_ptr->tail_buffs[i]->next = tmp;
						state_ptr->tail_buffs[i] = tmp;
					}

					remaining_size -= current_size * curr_num_buff;
					current_size += step;

				}

				state_ptr->actual_size += current_size;


				if(me == 0) {
					ScheduleNewEvent(me, timestamp, DEALLOC, NULL, 0);
				}
			}

			break;


		case LOOP:
			for(i = 0; i < LOOP_COUNT; i++) {
				j = i;
			}
			state_ptr->events++;
			timestamp = now + (simtime_t)(Expent(TAU));
			ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			if(Random() < 0.2)
				ScheduleNewEvent(FindReceiver(), timestamp, LOOP, NULL, 0);
			break;


		case ALLOC: {

			allocation_op(state_ptr, event_content->size);

			read_op(state_ptr);
			write_op(state_ptr);

			break;
		}

		case DEALLOC: {

			// Get size of the buffer to be deallocated
			current_size = deallocation_op(state_ptr);

			unsigned int recv = state_ptr->next_lp;
			state_ptr->next_lp = (state_ptr->next_lp + (n_prc_tot / 4 + 1)) % n_prc_tot;
			if (recv >= n_prc_tot)
				recv = n_prc_tot - 1;

			switch (timestamp_distribution) {
				case UNIFORM: {
					timestamp = now + (simtime_t)(tau * Random());
					break;
				}
				case EXPO: {
					timestamp = now + (simtime_t)(Expent(tau));
					break;
				}
				default:
					timestamp = now + (simtime_t)(5 * Random());
			}

			// Is there a buffer to be deallocated?
			if(fabsf(current_size + 1) < FLT_EPSILON) {
				ScheduleNewEvent(me, timestamp, DEALLOC, NULL, 0);
				break;
			} else {

				new_event.size = current_size;

				ScheduleNewEvent(me, timestamp, ALLOC, &new_event, sizeof(event_content_type));
				ScheduleNewEvent(recv, timestamp, DEALLOC, NULL, 0);

			}

			read_op(state_ptr);
			write_op(state_ptr);

			break;
		}

		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			break;
	}
}


bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void)me;

	if(snapshot->traditional) {
		if(snapshot->events < COMPLETE_EVENTS)
			return false;
		return true;
	}


	// Did we perform enough allocations?
        if (snapshot->cont_allocation < complete_alloc)
                return false;

        return true;
}

