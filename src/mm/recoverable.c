/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
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
#include <mm/dymelor.h>
#include <scheduler/process.h>


/// Recoverable memory state for LPs
malloc_state **recoverable_state;

/// Maximum number of malloc_areas allocated during the simulation. This variable is used
/// for correctly restoring an LP's state whenever some areas are deallocated during the simulation.
//int max_num_areas;




/**
* This function inizializes a malloc_area
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param m_area The pointer to the malloc_area to initialize
* @param size The chunks' size of the malloc_area
* @param num_chunks The number of chunk of the new malloc_area
*/
static void malloc_area_init(bool recoverable, malloc_area *m_area, size_t size, int num_chunks){

	int bitmap_blocks;

	m_area->is_recoverable = recoverable;
	m_area->chunk_size = size;
	m_area->alloc_chunks = 0;
	m_area->dirty_chunks = 0;
	m_area->next_chunk = 0;
	m_area->num_chunks = num_chunks;
	m_area->state_changed = 0;
	m_area->last_access = -1;
	m_area->use_bitmap = NULL;
	m_area->dirty_bitmap = NULL;
	m_area->area = NULL;

	bitmap_blocks = num_chunks / NUM_CHUNKS_PER_BLOCK;
	if(bitmap_blocks < 1)
		bitmap_blocks = 1;

	m_area->prev = -1;
	m_area->next = -1;

	SET_AREA_LOCK_BIT(m_area);

}


/**
* This function inizializes a malloc_state
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param state The pointer to the malloc_state to initialize
*/
void malloc_state_init(bool recoverable, malloc_state *state){

	int i, num_chunks;
	size_t chunk_size;

	state->total_log_size = 0;
	state->total_inc_size = 0;
	state->busy_areas = 0;
	state->dirty_areas = 0;
	state->num_areas = NUM_AREAS;
	state->max_num_areas = MAX_NUM_AREAS;
	state->bitmap_size = 0;
	state->dirty_bitmap_size = 0;
	state->timestamp = -1;
	state->is_incremental = false;

	state->areas = (malloc_area*)rsalloc(state->max_num_areas * sizeof(malloc_area));
	if(state->areas == NULL)
		rootsim_error(true, "Unable to allocate memory at %s:%d", __FILE__, __LINE__);

	chunk_size = MIN_CHUNK_SIZE;
	num_chunks = MIN_NUM_CHUNKS;

	for(i = 0; i < NUM_AREAS; i++){

		malloc_area_init(recoverable, &state->areas[i], chunk_size, num_chunks);
		state->areas[i].idx = i;
		chunk_size = chunk_size << 1;
		if(num_chunks > MIN_NUM_CHUNKS) {
			num_chunks = num_chunks >> 1;
		}
	}

}

/**
* This function inizializes the dymelor subsystem
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
*/
void dymelor_init(void) {

	register unsigned int i;

	// Preallocate memory for the LPs
	lp_alloc_init();
	
	recoverable_state = rsalloc(sizeof(malloc_state *) * n_prc);

	for(i = 0; i < n_prc; i++){

		recoverable_state[i] = rsalloc(sizeof(malloc_state));
		if(recoverable_state[i] == NULL)
			rootsim_error(true, "Unable to allocate memory on malloc init");

		malloc_state_init(true, recoverable_state[i]);
	}
	
	unrecoverable_init();
}



/**
* This function finalizes the dymelor subsystem
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
*/
void dymelor_fini(void){
	unsigned int i, j;
	malloc_area *current_area;

	for(i = 0; i < n_prc; i++) {
		for (j = 0; j < (unsigned int)recoverable_state[i]->num_areas; j++) {
			current_area = &(recoverable_state[i]->areas[j]);
			if (current_area != NULL) {
				if (current_area->use_bitmap != NULL) {
					lp_free(current_area->use_bitmap);
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
			list_pop(LPS[i]->queue_states);
		}
	}

	unrecoverable_fini();
	lp_alloc_fini();
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
		chk_size,
		bitmap_blocks;

	malloc_area *m_area = get_area(base);

	if(m_area != NULL){

		chk_size = m_area->chunk_size;
		RESET_BIT_AT(chk_size, 0);
		RESET_BIT_AT(chk_size, 1);

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

		bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
                if(bitmap_blocks < 1)
                       bitmap_blocks = 1;

		if (m_area->state_changed == 1){
                        if (m_area->dirty_chunks == 0)
                                recoverable_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
                } else {
                        recoverable_state[current_lp]->dirty_areas++;
                        recoverable_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE * 2;
                        m_area->state_changed = 1;
                }

                for(i = first_chunk; i <= last_chunk; i++){

                        // If it is dirted a clean chunk, set it dirty and increase dirty object count for the malloc_area
                        if (!CHECK_DIRTY_BIT(m_area, i)){
                                SET_DIRTY_BIT(m_area, i);
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
* @param log The pointer to the log, or to the state
* @return The whole size of the state (metadata included)
*
*/
size_t get_log_size(malloc_state *logged_state){
	if (logged_state == NULL)
		return 0;

	if (is_incremental(logged_state)) {
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->dirty_areas * sizeof(malloc_area) + logged_state->dirty_bitmap_size + logged_state->total_inc_size;
	} else {
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->busy_areas * sizeof(malloc_area) + logged_state->bitmap_size + logged_state->total_log_size;
	}
}


/**
* This function corrects a size value. It changes the given value to the immediately greater power of two
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param size The size to correct
* @return The new size
*/
static size_t compute_size(size_t size){

	// TODO: cambiare in qualcosa del tipo:
	// size = (size + sizeof(size_t) + (align_to - 1)) & ~ (align_to - 1);

	// Account for the space needed to keep the pointer to the malloc area
	size += sizeof(long long);

	size_t size_new;

	size_new = MIN_CHUNK_SIZE;

	while(size_new < size)
		size_new *= 2;

	return size_new;
}



/**
* Find the smallest idx associated with a free chunk in a given malloc_area.
* This function tries to keep the chunks clustered in the beginning of the malloc_area,
* so to enchance locality
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param m_area The malloc_area to scan in order to find the next available chunk
*/
static void find_next_free(malloc_area *m_area){
	m_area->next_chunk++;

	while(m_area->next_chunk < m_area->num_chunks){

		if(!CHECK_USE_BIT(m_area, m_area->next_chunk))
			break;

		m_area->next_chunk++;
	}

}



/**
* This is the wrapper of the real stdlib malloc(). Whenever the application level software
* calls malloc, the call is redirected to this piece of code which use the memory preallocated
* by the DyMeLoR subsystem for serving the request. If the memory in the malloc_area is exhausted,
* a new one is created, relying on the stdlib malloc.
* In future releases, this wrapper will be integrated with the Memory Management subsystem,
* which is not yet ready for production.
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param ptr A memory buffer
* @return The chunk's size
*
*/
void *__wrap_malloc(size_t size) {

	malloc_area *m_area, *prev_area;
	void *ptr;
	int bitmap_blocks, num_chunks;
	size_t area_size;

	int j;

	if(rootsim_config.serial) {
		return rsalloc(size);
	}

	size = compute_size(size);

	if(size > MAX_CHUNK_SIZE){
		rootsim_error(false, "Requested a memory allocation of %d but the limit is %d. Reconfigure MAX_CHUNK_SIZE. Returning NULL.\n", size, MAX_CHUNK_SIZE);
		return NULL;
	}

	j = (int)log2(size) - (int)log2(MIN_CHUNK_SIZE);

	m_area = &recoverable_state[current_lp]->areas[(int)log2(size) - (int)log2(MIN_CHUNK_SIZE)];

	while(m_area != NULL && m_area->alloc_chunks == m_area->num_chunks){
		prev_area = m_area;
		if (m_area->next == -1)
			m_area = NULL;
		else
			m_area = &(recoverable_state[current_lp]->areas[m_area->next]);
	}

	if(m_area == NULL){

		if(recoverable_state[current_lp]->num_areas == recoverable_state[current_lp]->max_num_areas){

			malloc_area *tmp;

			if ((recoverable_state[current_lp]->max_num_areas << 1) > MAX_LIMIT_NUM_AREAS)
				return NULL;

			recoverable_state[current_lp]->max_num_areas = recoverable_state[current_lp]->max_num_areas << 1;

			tmp = (malloc_area *)rsrealloc(recoverable_state[current_lp]->areas, recoverable_state[current_lp]->max_num_areas * sizeof(malloc_area));
			if(tmp == NULL){

				/**
				* @todo can we find a better way to handle the realloc failure?
				*/
				rootsim_error(false,  "DyMeLoR: cannot reallocate yet the block of malloc_area.");

				recoverable_state[current_lp]->max_num_areas = recoverable_state[current_lp]->max_num_areas >> 1;

				return NULL;
			}

			recoverable_state[current_lp]->areas = tmp;
		}

		// Allocate a new malloc area
		m_area = &recoverable_state[current_lp]->areas[recoverable_state[current_lp]->num_areas];

		// The malloc area to be instantiated has twice the number of chunks wrt the last full malloc area for the same chunks size
		malloc_area_init(true, m_area, size, prev_area->num_chunks << 1);

		m_area->idx = recoverable_state[current_lp]->num_areas;
		recoverable_state[current_lp]->num_areas++;
		prev_area->next = m_area->idx;
		m_area->prev = prev_area->idx;

	}

	if(m_area->area == NULL){

		num_chunks = m_area->num_chunks;
		bitmap_blocks = num_chunks / NUM_CHUNKS_PER_BLOCK;
		if(bitmap_blocks < 1)
			bitmap_blocks = 1;

		if(m_area->is_recoverable) {
			area_size = bitmap_blocks * BLOCK_SIZE * 2 + num_chunks * size;

			m_area->use_bitmap = (unsigned int *)lp_malloc(area_size);

			if(m_area->use_bitmap == NULL){
				rootsim_error(true, "DyMeLoR: error allocating space for the use bitmap");
			}

			m_area->dirty_chunks = 0;
			bzero(m_area->use_bitmap, area_size);

			m_area->dirty_bitmap = (unsigned int*)((char*)m_area->use_bitmap + bitmap_blocks * BLOCK_SIZE);

			m_area->area = (void*)((char*)m_area->use_bitmap + bitmap_blocks * BLOCK_SIZE * 2);
		} else {
			m_area->area = lp_malloc(num_chunks * size);
		}
	}

	if(m_area->area == NULL) {
		rootsim_error(true, "Error while allocating memory for LP %d at %s:%d\n", current_lp, __FILE__, __LINE__);
	}

	ptr = (void*)((char*)m_area->area + (m_area->next_chunk * size));

	SET_USE_BIT(m_area, m_area->next_chunk);

	bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
	if(bitmap_blocks < 1)
		bitmap_blocks = 1;


	if(m_area->alloc_chunks == 0){
		recoverable_state[current_lp]->bitmap_size += bitmap_blocks * BLOCK_SIZE;
		recoverable_state[current_lp]->busy_areas++;
	}

	if(m_area->state_changed == 0) {
		recoverable_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
		recoverable_state[current_lp]->dirty_areas++;
	}

	m_area->state_changed = 1;

	m_area->alloc_chunks++;
	m_area->last_access = current_lvt;

	if(!CHECK_LOG_MODE_BIT(m_area)){
		if((double)m_area->alloc_chunks / (double)m_area->num_chunks > MAX_LOG_THRESHOLD){
			SET_LOG_MODE_BIT(m_area);
			recoverable_state[current_lp]->total_log_size += (m_area->num_chunks - (m_area->alloc_chunks - 1)) * size;
		} else
			recoverable_state[current_lp]->total_log_size += size;
	}

	find_next_free(m_area);

	int chk_size = m_area->chunk_size;
	RESET_BIT_AT(chk_size, 0);
	RESET_BIT_AT(chk_size, 1);

	// Keep track of the malloc_area which this chunk belongs to
	*((long long *)ptr) = (long long)m_area;
	return (void*)((char*)ptr + sizeof(long long));
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

	malloc_area * m_area;
	int idx, bitmap_blocks;
	size_t chunk_size;

	if(rootsim_config.serial) {
		rsfree(ptr);
		return;
	}

	if(ptr == NULL)
		return;

	m_area = get_area(ptr);

	chunk_size = m_area->chunk_size;
	RESET_BIT_AT(chunk_size, 0);
	RESET_BIT_AT(chunk_size, 1);
	idx = (int)((char*)ptr - (char*)m_area->area) / chunk_size;

	RESET_USE_BIT(m_area, idx);

        bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
        if(bitmap_blocks < 1)
                bitmap_blocks = 1;


	if (m_area->state_changed == 0){
                recoverable_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
                recoverable_state[current_lp]->dirty_areas++;
        }

	if(CHECK_DIRTY_BIT(m_area, idx)){
                RESET_DIRTY_BIT(m_area, idx);
                m_area->dirty_chunks--;

                if (m_area->state_changed == 1 && m_area->dirty_chunks == 0)
                        recoverable_state[current_lp]->dirty_bitmap_size -= bitmap_blocks * BLOCK_SIZE;

                recoverable_state[current_lp]->total_inc_size -= chunk_size;

                if (m_area->dirty_chunks < 0)
			rootsim_error(true, "%s:%d: Dirty chunk is negative", __FILE__, __LINE__);
        }

	m_area->state_changed = 1;

	if(idx < m_area->next_chunk)
		m_area->next_chunk = idx;


	m_area->last_access = current_lvt;
	m_area->alloc_chunks--;


	if(m_area->alloc_chunks == 0){
		recoverable_state[current_lp]->bitmap_size -= bitmap_blocks * BLOCK_SIZE;
		recoverable_state[current_lp]->busy_areas--;
	}

	if(CHECK_LOG_MODE_BIT(m_area)){
		if((double)m_area->alloc_chunks / (double)m_area->num_chunks < MIN_LOG_THRESHOLD){
			RESET_LOG_MODE_BIT(m_area);
			recoverable_state[current_lp]->total_log_size -= (m_area->num_chunks - m_area->alloc_chunks) * chunk_size;
		}
	} else
		recoverable_state[current_lp]->total_log_size -= chunk_size;

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

	if(rootsim_config.serial) {
		return rsrealloc(ptr, size);
	}

	// If ptr is NULL realloc is equivalent to the malloc
	if (ptr == NULL) {
		return __wrap_malloc(size);
	}

	// If ptr is not NULL and the size is 0 realloc is equivalent to the free
	if (size == 0) {
		__wrap_free(ptr);
		return NULL;
	}

	m_area = get_area(ptr);

	// The size could be greater than the real request, but it does not matter since the realloc specific requires that
	// is copied at least the smaller buffer size between the new and the old one
	old_size = m_area->chunk_size;

	new_buffer = __wrap_malloc(size);

	if (new_buffer == NULL)
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

	if(rootsim_config.serial) {
		return rscalloc(nmemb, size);
	}

	if (nmemb == 0 || size == 0)
		return NULL;

	buffer = __wrap_malloc(nmemb * size);
	if (buffer == NULL)
		return NULL;

	bzero(buffer, nmemb * size);

	return buffer;
}




void clean_buffers_on_gvt(unsigned int lid, simtime_t time_barrier){

	int i;
	malloc_state *state;
	malloc_area *m_area;

	state = recoverable_state[lid];

	// The first NUM_AREAS malloc_areas are placed according to their chunks' sizes. The exceeding malloc_areas can be compacted
	for(i = NUM_AREAS; i < state->num_areas; i++){
		m_area = &state->areas[i];

		if(m_area->alloc_chunks == 0 && m_area->last_access < time_barrier && !CHECK_AREA_LOCK_BIT(m_area)){

			if(m_area->use_bitmap != NULL) {

				// lp_free
				lp_free(m_area->use_bitmap);

				m_area->use_bitmap = NULL;
				m_area->dirty_bitmap = NULL;
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
					// The swapped area will now be checked
					i--;
				}

				// Decrement the expected number of areas
				state->num_areas--;
			}
		}
	}
}


