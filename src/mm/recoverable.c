/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file dymelor.c
* @brief This module implements all the logic and all the routines supporting
*        ROOT-Sim's memory manager subsystem
* @author Roberto Toccaceli
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @author Francesco Quaglia
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <core/core.h>
#include <core/init.h>
#include <mm/dymelor.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>


/// Recoverable memory state for LPs
malloc_state **recoverable_state;

/// Maximum number of malloc_areas allocated during the simulation. This variable is used
/// for correctly restoring an LP's state whenever some areas are deallocated during the simulation.
//int max_num_areas;





void recoverable_init(void) {

	register unsigned int i;

	recoverable_state = rsalloc(sizeof(malloc_state *) * n_prc);

	for(i = 0; i < n_prc; i++){

		recoverable_state[i] = rsalloc(sizeof(malloc_state));
		if(recoverable_state[i] == NULL)
			rootsim_error(true, "Unable to allocate memory on malloc init");

		malloc_state_init(true, recoverable_state[i]);
	}
}


void recoverable_fini(void) {
//	unsigned int i, j;
//	malloc_area *current_area;

	// TODO: reimplmenent with foreach
/*	for(i = 0; i < n_prc; i++) {
		for (j = 0; j < (unsigned int)recoverable_state[i]->num_areas; j++) {
			current_area = &(recoverable_state[i]->areas[j]);
			if (current_area != NULL) {
				if (current_area->self_pointer != NULL) {
					#ifdef HAVE_PARALLEL_ALLOCATOR
					pool_release_memory(i, current_area->self_pointer);
					#else
					rsfree(current_area->self_pointer);
					#endif
				}
			}
		}
		rsfree(recoverable_state[i]->areas);
		rsfree(recoverable_state[i]);
	}
	rsfree(recoverable_state);

	// Release as well memory used for remaining logs
	for(i = 0; i < n_prc; i++) {
		while(!list_empty(LPS[i]->queue_states)) {
			rsfree(list_head(LPS[i]->queue_states)->log);
			list_pop(i, LPS[i]->queue_states);
		}
	}
*/
}


/**
* This function marks a memory chunk as dirty.
* It is invoked from assembly modules invoked by calls injected by the instrumentor, and from the
* third-party library wrapper. Invocations from other parts of the kernel should be handled with
* great care.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param base The initial to the start address of the update
* @param size The number of bytes being updated
*/
void dirty_mem(void *base, int size) {


	// TODO: Quando reintegriamo l'incrementale questo qui deve ricomparire!
	(void)base;
	(void)size;


	return;

#if 0
//	unsigned long long current_cost;

	// Sanity check on passed address
/*	if(base == NULL) {
		rootsim_error(false, "Trying to access NULL. Memory interception aborted\n");
		return;
	}
*/
/*	if (rootsim_config.snapshot == AUTONOMIC_SNAPSHOT ||
	    rootsim_config.snapshot == AUTONOMIC_INC_SNAPSHOT ||
	    rootsim_config.snapshot == AUTONOMIC_FULL_SNAPSHOT)
		add_counter++;
*/
	int 	first_chunk,
		last_chunk,
		i,
		chk_size;

	size_t bitmap_size;

	malloc_area *m_area = get_area(base);

	if(m_area != NULL) {

		chk_size = UNTAGGED_CHUNK_SIZE(m_area->chunk_size);

		// Compute the number of chunks affected by the write
		first_chunk = (int)(((char *)base - (char *)m_area->area) / chk_size);

		// If size == -1, then we adopt a conservative approach: dirty all the chunks from the base to the end
		// of the actual malloc area base address belongs to.
		// This has been inserted to support the wrapping of third-party libraries where the size of the
		// update (or even the actual update) cannot be statically/dynamically determined.
		if(size == -1)
			last_chunk = m_area->num_chunks - 1;
		else
			last_chunk = (int)(((char *)base + size - 1 - (char *)m_area->area) / chk_size);

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		if (m_area->state_changed == 1){
                        if (m_area->dirty_chunks == 0)
                                recoverable_state[current_lp]->dirty_bitmap_size += bitmap_size;
                } else {
                        recoverable_state[current_lp]->dirty_areas++;
                        recoverable_state[current_lp]->dirty_bitmap_size += bitmap_size * 2;
                        m_area->state_changed = 1;
                }

                for(i = first_chunk; i <= last_chunk; i++){

                        // If it is dirtied a clean chunk, set it dirty and increase dirty object count for the malloc_area
                        if (!bitmap_check(m_area->dirty_bitmap, i)){
                        		bitmap_set(m_area->dirty_bitmap, i);
                                recoverable_state[current_lp]->total_inc_size += chk_size;

                                m_area->dirty_chunks++;
                        }
                }
	}
#endif
}



/**
* This function returns the whole size of a state. It can be used as the total size to pack a log
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param logged_state The pointer to the log, or to the state
* @return The whole size of the state (metadata included)
*
*/
size_t get_log_size(malloc_state *logged_state){
	if (unlikely(logged_state == NULL))
		return 0;

	if (is_incremental(logged_state)) {
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->dirty_areas * sizeof(malloc_area) + logged_state->dirty_bitmap_size + logged_state->total_inc_size;
	} else {
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->busy_areas * sizeof(malloc_area) + logged_state->bitmap_size + logged_state->total_log_size;
	}
}





/**
* This is the wrapper of the real stdlib malloc(). Whenever the application level software
* calls malloc, the call is redirected to this piece of code which uses the memory preallocated
* by the DyMeLoR subsystem for serving the request. If the memory in the malloc_area is exhausted,
* a new one is created, relying on the stdlib malloc.
* In future releases, this wrapper will be integrated with the Memory Management subsystem,
* which is not yet ready for production.
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param size Size of the allocation
* @return A pointer to the allocated memory
*
*/
void *__wrap_malloc(size_t size) {
	void *ptr;

	switch_to_platform_mode();

	if(unlikely(rootsim_config.serial)) {
		ptr = rsalloc(size);
		goto out;
	}

	ptr = do_malloc(current_lp, recoverable_state[lid_to_int(current_lp)], size);

    out:
	switch_to_application_mode();
	return ptr;
}

/**
* This is the wrapper of the real stdlib free(). Whenever the application level software
* calls free, the call is redirected to this piece of code which will set the chunk in the
* corresponding malloc_area as not allocated.
*
* For further information, please see the paper:
* 	R. Toccaceli, F. Quaglia
* 	DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects
* 	with Generic Memory Layout
*	Proceedings of the 22nd Workshop on Principles of Advanced and Distributed Simulation
*	2008
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param ptr A memory buffer to be free'd
*
*/
void __wrap_free(void *ptr) {
	switch_to_platform_mode();

	if(unlikely(rootsim_config.serial)) {
		rsfree(ptr);
		goto out;
	}

	do_free(current_lp, recoverable_state[lid_to_int(current_lp)], ptr);

    out:
	switch_to_application_mode();
}



/**
* This is the wrapper of the real stdlib realloc(). Whenever the application level software
* calls realloc, the call is redirected to this piece of code which rely on wrap_malloc
*
* @author Roberto Vitali
*
* @param ptr The pointer to be buffer to be reallocated
* @param size The size of the allocation
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_realloc(void *ptr, size_t size){

	void *new_buffer;
	size_t old_size;
	malloc_area *m_area;

	if(unlikely(rootsim_config.serial)) {
		return rsrealloc(ptr, size);
	}

	// If ptr is NULL realloc is equivalent to the malloc
	if (unlikely(ptr == NULL)) {
		return __wrap_malloc(size);
	}

	// If ptr is not NULL and the size is 0 realloc is equivalent to the free
	if (unlikely(size == 0)) {
		__wrap_free(ptr);
		return NULL;
	}

	m_area = get_area(ptr);

	// The size could be greater than the real request, but it does not matter since the realloc specific requires that
	// is copied at least the smaller buffer size between the new and the old one
	old_size = m_area->chunk_size;

	new_buffer = __wrap_malloc(size);

	if (unlikely(new_buffer == NULL))
		return NULL;

	memcpy(new_buffer, ptr, size > old_size ? size : old_size);
	__wrap_free(ptr);

	return new_buffer;
}



/**
* This is the wrapper of the real stdlib calloc(). Whenever the application level software
* calls calloc, the call is redirected to this piece of code which relies on wrap_malloc
*
* @author Roberto Vitali
*
* @param size The size of the allocation
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_calloc(size_t nmemb, size_t size){

	void *buffer;

	if(unlikely(rootsim_config.serial)) {
		return rscalloc(nmemb, size);
	}

	if (unlikely(nmemb == 0 || size == 0))
		return NULL;

	buffer = __wrap_malloc(nmemb * size);
	if (unlikely(buffer == NULL))
		return NULL;

	bzero(buffer, nmemb * size);

	return buffer;
}




void clean_buffers_on_gvt(LID_t lid, simtime_t time_barrier){

	int i;
	malloc_state *state;
	malloc_area *m_area;

	state = recoverable_state[lid_to_int(lid)];

	// The first NUM_AREAS malloc_areas are placed according to their chunks' sizes. The exceeding malloc_areas can be compacted
	for(i = NUM_AREAS; i < state->num_areas; i++){
		m_area = &state->areas[i];

		if(m_area->alloc_chunks == 0 && m_area->last_access < time_barrier && !CHECK_AREA_LOCK_BIT(m_area)) {

			if(m_area->self_pointer != NULL) {

				#ifdef HAVE_PARALLEL_ALLOCATOR
				pool_release_memory(lid, m_area->self_pointer);
				#else
				rsfree(m_area->self_pointer);
				#endif

				m_area->use_bitmap = NULL;
				m_area->dirty_bitmap = NULL;
				m_area->self_pointer = NULL;
				m_area->area = NULL;
				m_area->state_changed = 0;

				// Set the pointers
				if(m_area->prev != -1)
					state->areas[m_area->prev].next = m_area->next;
				if(m_area->next != -1)
					state->areas[m_area->next].prev = m_area->prev;

				// Swap, if possible
				if(i < state->num_areas - 1) {
					memcpy(m_area, &state->areas[state->num_areas - 1], sizeof(malloc_area));
					m_area->idx = i;
					if(m_area->prev != -1)
						state->areas[m_area->prev].next = m_area->idx;
					if(m_area->next != -1)
						state->areas[m_area->next].prev = m_area->idx;

					// Update the self pointer
					*(long long *)m_area->self_pointer = (long long)m_area;

					// The swapped area will now be checked
					i--;
				}

				// Decrement the expected number of areas
				state->num_areas--;
			}
		}
	}
}


