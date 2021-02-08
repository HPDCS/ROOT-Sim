#include <string.h>
#include <stdio.h>

#include "application.h"

char random_char = 0;


// This reimplementation of the memset was done to allow the ROOT-Sim compiler
// to intercept the write instructions when third-party libraries' hooking was
// not yet supported, and is left here for legacy compatibility
void *set_mem(void *s, size_t count, int c) {

	int d0, d1;
	__asm__ __volatile__ (
		"rep\n\t"
		"stosb"
		:"=&c" (d0), "=&D" (d1)
		: "a" (c), "1" (s), "0" (count)
		: "memory");
	return s;
}


// This function implements a read operation over the set of allocated buffers
void read_op(lp_state_type *state_ptr) {

	// Check if the lists are empty
	if (state_ptr->num_elementi == 0)
		return;

	unsigned int read_size, already_read = 0;
	int start_idx, i;
	buffers *pointers[num_buffers];

	for (i = 0; i < num_buffers; i++)
		pointers[i] = state_ptr->head_buffs[i];

	read_size = (unsigned int)(read_distribution * state_ptr->total_size);

	if(read_correction == UNIFORM) {
		read_size *= (unsigned int)Random();
	} else if(read_correction == EXPO) {
		read_size *= (unsigned int)Expent(read_size);
	}

	start_idx = (int)rint((num_buffers - 1) * Random());
	i = start_idx;
	while (already_read < read_size){
		if (pointers[i] != NULL){
			memcmp(pointers[i]->buffer, pointers[i]->buffer, state_ptr->taglie[i]);
			already_read += state_ptr->taglie[i];
			pointers[i] = pointers[i]->next;
		}
		i = (i + 1) % num_buffers;
	}

}

// This function implements a write operation over the set of allocated buffers
void write_op(lp_state_type *state_ptr){

	// Check whether lists are empty
	if (state_ptr->num_elementi == 0)
		return;

	unsigned int write_size, already_write = 0;
	int start_idx, i;
	buffers *pointers[num_buffers];

	for (i = 0; i < num_buffers; i++)
			pointers[i] = state_ptr->head_buffs[i];

	write_size = (unsigned int)(write_distribution * state_ptr->total_size);

	if(write_correction == UNIFORM) {
		write_size *= (unsigned int)Random();
	} else if(write_correction == EXPO) {
		write_size *= (unsigned int)Expent(write_size);
	}

        start_idx = (int)rint((num_buffers - 1) * Random());
        i = start_idx;
        while (already_write < write_size){
		if (pointers[i] != NULL){

			random_char = (random_char + 1) % 128;
			set_mem(pointers[i]->buffer, state_ptr->taglie[i], random_char);
	        	already_write += state_ptr->taglie[i];
			pointers[i] = pointers[i]->next;
		}
                i = (i + 1) % num_buffers;
        }
}

// idx is the intex returned by deallocation_op(). It is enough to
// reach that buffer and add one at tail.
void allocation_op(lp_state_type *state_ptr, int idx){

	buffers *temp = malloc(sizeof(buffers));

	if(temp == NULL) {
		printf("Unable to allocate!\n");
		fflush(stdout);
	}
	temp->buffer = malloc(state_ptr->taglie[idx]);
	temp->next = NULL;

	if (state_ptr->head_buffs[idx] == NULL){ // Is there an element in the list?
		state_ptr->head_buffs[idx] = temp;
		temp->prev = NULL;
	} else {
		temp->prev = state_ptr->tail_buffs[idx];
		state_ptr->tail_buffs[idx]->next = temp;
	}

	state_ptr->tail_buffs[idx] = temp;
	state_ptr->elementi[idx]++;
	state_ptr->num_elementi++;

	state_ptr->total_size += state_ptr->taglie[idx];

	state_ptr->actual_size += state_ptr->taglie[idx];

	// For the termination predicate
	state_ptr->cont_allocation++;
}


// This function implements
int deallocation_op(lp_state_type *state_ptr) {

	buffers *curr;
	int idx, element, i = 0 ;

	// If nothing is allocated, return immediately
	if (state_ptr->num_elementi == 0)
		return -1;

	// Select one list (with at least one element)
	do {
		idx = (int)rint((double)((num_buffers - 1) * Random()));
	} while(state_ptr->elementi[idx] == 0);

	// Victim selection
	element = (int)rint((state_ptr->elementi[idx] - 1) * Random());

	curr = state_ptr->head_buffs[idx];

	while(i < element) {
			curr = curr->next;
		i++;
	}


	// Delete buffer
	if(curr == state_ptr->head_buffs[idx]) { // Head element selected

		// Is there a next element?
		if(curr->next != NULL) {
			curr->next->prev = NULL;
			state_ptr->head_buffs[idx] = curr->next;
		} else {
			state_ptr->head_buffs[idx] = NULL;
			// Head element selected. No next element => It's tail as well
			state_ptr->tail_buffs[idx] = NULL;
		}

	} else if(curr == state_ptr->tail_buffs[idx]) { // Tail element selected

		// If there is only one element in the list, the case is handled by the previous if branch.
		// If we get executing here, there are at least 2 elements (otherwise, the element was referred
		// by state_ptr->head_buffs[idx] as well). This means that curr, in this branch, has at least
		// one previous element.

		curr->prev->next = NULL;
		state_ptr->tail_buffs[idx] = curr->prev;

	} else { // Element in the middle

		curr->next->prev = curr->prev;
		curr->prev->next = curr->next;

	}

	free(curr->buffer);
	free(curr);

	state_ptr->actual_size -= state_ptr->taglie[idx];

	state_ptr->total_size -= state_ptr->taglie[idx];
	state_ptr->num_elementi--;
	state_ptr->elementi[idx]--;

	return idx;
}

