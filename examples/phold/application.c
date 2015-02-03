#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>

#include "application.h"


// These global variables are used to store execution configuration values
// They will be set by every LP on every Simulation Kernel upon INIT execution
int	object_total_size,
	timestamp_distribution,
	max_size,
	min_size,
	num_buffers,
	complete_alloc,
	read_correction,
	write_correction;
double	write_distribution,
	read_distribution,
	tau;


void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

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

			// Store predefined values
			object_total_size = OBJECT_TOTAL_SIZE;
			timestamp_distribution = EXPO;
			max_size = MAX_SIZE;
			min_size = MIN_SIZE;
			num_buffers = NUM_BUFFERS;
			complete_alloc = COMPLETE_ALLOC;
			write_distribution = WRITE_DISTRIBUTION;
			read_distribution = READ_DISTRIBUTION;
			tau = TAU;

			// Read runtime parameters
			if(IsParameterPresent(event_content, "object_total_size"))
				object_total_size = GetParameterInt(event_content, "object_total_size");

			if(IsParameterPresent(event_content, "timestamp_distribution"))
				timestamp_distribution = GetParameterInt(event_content, "timestamp_distribution");

			if(IsParameterPresent(event_content, "max_size"))
				max_size = GetParameterInt(event_content, "max_size");

			if(IsParameterPresent(event_content, "min_size"))
				min_size = GetParameterInt(event_content, "min_size");

			if(IsParameterPresent(event_content, "num_buffers"))
				num_buffers = GetParameterInt(event_content, "num_buffers");

			if(IsParameterPresent(event_content, "complete_alloc"))
				complete_alloc = GetParameterInt(event_content, "complete_alloc");

			if(IsParameterPresent(event_content, "read_correction"))
				read_correction = GetParameterInt(event_content, "read_correction");

			if(IsParameterPresent(event_content, "write_correction"))
				write_correction = GetParameterInt(event_content, "write_correction");

			if(IsParameterPresent(event_content, "write_distribution"))
				write_distribution = GetParameterDouble(event_content, "write_distribution");

			if(IsParameterPresent(event_content, "read_distribution"))
				read_distribution = GetParameterDouble(event_content, "read_distribution");

			if(IsParameterPresent(event_content, "tau"))
				tau = GetParameterDouble(event_content, "tau");

			// Print out current configuration (only once)
			if(me == 0) {
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
					"write-correction: %d\n",
					object_total_size,
					timestamp_distribution,
					max_size,
					min_size,
					num_buffers,
					complete_alloc,
					write_distribution,
					read_distribution,
					tau,
					write_correction);
				 printf("\n");
			}

			// Initialize LP's state
			state_ptr = (lp_state_type *)malloc(sizeof(lp_state_type));
                        if(state_ptr == NULL){
                                exit(-1);
                        }

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

			// Explicitly tell ROOT-Sim this is our LP's state
                        SetState(state_ptr);

			timestamp = (simtime_t) (20 * Random());
			if(me == 0) {
				ScheduleNewEvent(me, timestamp, DEALLOC, NULL, 0);
			}

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

			int recv = state_ptr->next_lp;
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
			if(current_size == -1) {
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


	// Did we perform enough allocations?
        if (snapshot->cont_allocation < complete_alloc)
                return false;

        return true;
}

