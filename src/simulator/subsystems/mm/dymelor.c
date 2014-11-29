/**
*			Copyright (C) 2008-2014 HPDCS Group
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
#include <mm/malloc.h>
#include <scheduler/process.h>


/// Current per-logical process state
malloc_state *m_state[MAX_LPs];



/// Maximum number of malloc_areas allocated during the simulation. This variable is used
/// for correctly restoring an LP's state whenever some areas are deallocated during the simulation.
int max_num_areas;


// This variable is used in the software cache to determine if an entry in the cache is still valid.
int era = 0;


/// Software Cache for holding a reference between addressess and malloc_areas, for reverse queries
__thread cache_line cache_chunk_address[CACHE_SIZE]; // __attribute__ ((section (".data")));


/// Flag to tell DyMeLoR to take a full log upon next invocation of log_state, independently of any other configuration/current state
int force_full[MAX_LPs];


/// This global variable counts the number of write access to memory for the statistic needed by the autonic layer
extern unsigned int add_counter;


/// This variable is used to count the overhead to mark a memory area as updated
unsigned long long ticks_counter;


static void malloc_state_init(malloc_state *);
static void malloc_area_init(malloc_area *, size_t, int);
static void load_cache_line(void *, void *, unsigned int);
static malloc_area *get_area(void *);
static size_t compute_size(size_t);
static void find_next_free(malloc_area*);


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

	for(i = 0; i < n_prc; i++){

//		m_state[i] = (malloc_state*)__real_malloc(sizeof(malloc_state));
		m_state[i] = (malloc_state*)rsalloc(sizeof(malloc_state));
		if(m_state[i] == NULL) 
			rootsim_error(true, "Unable to allocate memory on malloc init");

		malloc_state_init(m_state[i]);
		
		// Next (first) log must be taken, and taken full!
		force_LP_checkpoint(i);
		force_full[i] = FORCE_FULL_NEXT;
	}

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
		for (j = 0; j < (unsigned int)m_state[i]->num_areas; j++) {
			current_area = &(m_state[i]->areas[j]);
			if (current_area != NULL) {
				if (current_area->use_bitmap != NULL) {
					lp_free(current_area->use_bitmap);
				}
			}
		}
		rsfree(m_state[i]->areas);
		rsfree(m_state[i]);
	}
	
	// Release as well memory used for remaining logs
	for(i = 0; i < n_prc; i++) {
		while(!list_empty(LPS[i]->queue_states)) {
			rsfree(list_head(LPS[i]->queue_states)->log);
			list_pop(LPS[i]->queue_states);
		}
	}
	
	lp_alloc_fini();
}



/**
* This function inizializes a malloc_area
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param m_area The pointer to the malloc_area to initialize
* @param size The chunks' size of the malloc_area
* @param num_chunks The number of chunk of the new malloc_area
*/
static void malloc_area_init(malloc_area *m_area, size_t size, int num_chunks){

	int bitmap_blocks;

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
static void malloc_state_init(malloc_state *state){

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
	state->is_incremental = -1;

	state->areas = (malloc_area*)rsalloc(state->max_num_areas * sizeof(malloc_area));
	if(state->areas == NULL)
		rootsim_error(true, "Unable to allocate memory on state init");

	chunk_size = MIN_CHUNK_SIZE;
	num_chunks = MIN_NUM_CHUNKS;

	for(i = 0; i < NUM_AREAS; i++){

		malloc_area_init(&state->areas[i], chunk_size, num_chunks);
		state->areas[i].idx = i;
		chunk_size = chunk_size << 1;
		if(num_chunks > MIN_NUM_CHUNKS) {
			num_chunks = num_chunks >> 1;
		}
	}

}




/**
* This function should be used to force DyMeLoR to take a full log upon invocation of log_state, or to follow
* its internal policy to determine which checkpointing routine should be invoked.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The Logical Process Id
* @param value The value to be assigned to the variable. Can take the following values:
*        FORCE_FULL_NEXT: upon next invocation of log_state on lid, DyMeLoR will be forced to take a full snapshot
*        FORCE_FULL: force DyMeLoR to take a always a full snapshot
*        NO_FORCE_FULL: upon next invocation of log_state on lid, DyMeLoR will follow the strategy specified by the configuration to decide the log to take
*/
void set_force_full(unsigned int lid, int value){
	force_full[lid] = value;
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
	return;


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
                                m_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
                } else {
                        m_state[current_lp]->dirty_areas++;
                        m_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE * 2;
                        m_area->state_changed = 1;
                }

                for(i = first_chunk; i <= last_chunk; i++){

                        // If it is dirted a clean chunk, set it dirty and increase dirty object count for the malloc_area
                        if (!CHECK_DIRTY_BIT(m_area, i)){
                                SET_DIRTY_BIT(m_area, i);
                                m_state[current_lp]->total_inc_size += chk_size;

                                m_area->dirty_chunks++;
                        }
                }
	}

}



/**
* This function inserts an entry into the chunk cache.
* To assign a cache line, we compute log2(CACHE_SIZE).
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param init_address The initial address of the chunk
* @param final_address The final address of the chunk
* @param malloc_area_idx The idx of the malloc area owns the chunk
*/
static void load_cache_line(void *init_address, void *final_address, unsigned int malloc_area_idx) {

	int line_number = GET_CACHE_LINE_NUMBER((unsigned long)init_address);

        cache_chunk_address[line_number].chunk_init_address = init_address;
        cache_chunk_address[line_number].chunk_final_address = final_address;
        cache_chunk_address[line_number].malloc_area_idx = malloc_area_idx;
        cache_chunk_address[line_number].lid = current_lp;
        cache_chunk_address[line_number].valid_era = era;

}




/**
* This function returns the whole size of a LP's state. It can be used as the total size to pack a log
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lp The Logical Process Id
* @return The whole size of the state (included the metadata)
*/
size_t get_state_size(int lp){
	return sizeof(malloc_state) + sizeof(seed_type) +  m_state[lp]->busy_areas * sizeof(malloc_area) + m_state[lp]->bitmap_size + m_state[lp]->total_log_size;
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
size_t get_log_size(void *log){
	if (log == NULL)
		return 0;
	malloc_state *logged_state = (malloc_state *)log;
	if (is_incremental(log))
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->dirty_areas * sizeof(malloc_area) + logged_state->dirty_bitmap_size + logged_state->total_inc_size;
	else
		return sizeof(malloc_state) + sizeof(seed_type) + logged_state->busy_areas * sizeof(malloc_area) + logged_state->bitmap_size + logged_state->total_log_size;
}



/**
* This function returns the whole size of an incremental log (metadata included)
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param log The pointer to the log, or to the state
* @return The whole size of an incremental log (metadata included)
*/
size_t get_inc_log_size(void *log){

	if (log == NULL)
		return 0;

	malloc_state *logged_state = (malloc_state *)log;
	return sizeof(malloc_state) + sizeof(seed_type) + logged_state->dirty_areas * sizeof(malloc_area) + logged_state->dirty_bitmap_size + logged_state->total_inc_size;
}



/**
* This function retrieves the malloc_area a buffer belongs to.
* If enabled, this function relies on the software cache.
* Cache lineas compute log2(CACHE_SIZE). If the cache size is not a power of 2 (which must be not!),
* the line number corresponding to ptr is given by:
*    chunk_address & 2^(floor(log2(CACHE_SIZE)))
* To load a line:
*    - in case of miss, as the original get_area()
*    - when a chunk is allocated, the line is loaded into cache
*    - when a line is loaded into cache, time_value must be updated
* To retrieve a line:
*    - Go to the corresponding line
*    - Check if chunk_address == ptr
*    - Check if time_value in the line is the same as the one of the cache
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param ptr A memory buffer
*/
static malloc_area *get_area(void *ptr) {

	int i, line_number;
	malloc_area *m_area;
	size_t chunk_size;


	#ifdef NO_CACHE
	goto miss;
	#endif

	line_number = GET_CACHE_LINE_NUMBER((unsigned long)ptr);

	// Check whether the assigned line contains an entry for the passed pointer
	if(cache_chunk_address[line_number].chunk_init_address == ptr) {

		// Check whether the current process id was the one who put the line in the cache
		if(cache_chunk_address[line_number].lid != current_lp) {

			// The line MUST be invalid
			if(cache_chunk_address[line_number].valid_era == era)
				rootsim_error(true, "Error get_area: valid entry for different lid: %d get_area: %lx - lid: %d\n",
					current_lp, (unsigned long)cache_chunk_address[line_number].chunk_init_address, cache_chunk_address[line_number].lid);

			goto miss;
		}

		// The line could be invalid
		if(cache_chunk_address[line_number].valid_era != era)
 			goto miss;

	   	// Hit
		return &m_state[current_lp]->areas[cache_chunk_address[line_number].malloc_area_idx];
	}

    miss:

	for(i = 0; i < m_state[current_lp]->num_areas; i++){

		m_area = &m_state[current_lp]->areas[i];
		chunk_size = m_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);
		RESET_BIT_AT(chunk_size, 1);

		if(m_area->area != NULL && (m_area->area <= ptr && (void *)((char *)m_area->area + chunk_size * m_area->num_chunks) > ptr)){

			// Add a line in the chunk address cache
			load_cache_line(ptr, (char *)ptr + chunk_size, (unsigned int)i);

			return m_area;
		}

	}

	return NULL;
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

	unsigned int check;

	m_area->next_chunk++;

	while(m_area->next_chunk < m_area->num_chunks){

		check = CHECK_USE_BIT(m_area, m_area->next_chunk);

		if(!check)
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
	void *ptr, *final_address;
	int bitmap_blocks, num_chunks, malloc_area_idx;
	
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

	m_area = &m_state[current_lp]->areas[(int)log2(size) - (int)log2(MIN_CHUNK_SIZE)];

	while(m_area != NULL && m_area->alloc_chunks == m_area->num_chunks){
		prev_area = m_area;
		if (m_area->next == -1)
			m_area = NULL;
		else
			m_area = &(m_state[current_lp]->areas[m_area->next]);
	}

	if(m_area == NULL){

		if(m_state[current_lp]->num_areas == m_state[current_lp]->max_num_areas){

			malloc_area *tmp;

			if ((m_state[current_lp]->max_num_areas << 1) > MAX_LIMIT_NUM_AREAS)
				return NULL;

			m_state[current_lp]->max_num_areas = m_state[current_lp]->max_num_areas << 1;

			tmp = (malloc_area *)rsrealloc(m_state[current_lp]->areas, m_state[current_lp]->max_num_areas * sizeof(malloc_area));
			if(tmp == NULL){
				
				/**
				* @todo can we find a better way to handle the realloc failure?
				*/
				rootsim_error(false,  "DyMeLoR: cannot reallocate the block of malloc_area.");

				m_state[current_lp]->max_num_areas = m_state[current_lp]->max_num_areas >> 1;
				
				return NULL;
			}

			m_state[current_lp]->areas = tmp;
		}

		// Allocate a new malloc area
		m_area = &m_state[current_lp]->areas[m_state[current_lp]->num_areas];

		// The malloc area to be instantiated has twice the number of chunks wrt the last full malloc area for the same chunks size
		malloc_area_init(m_area, size, prev_area->num_chunks << 1);

		m_area->idx = m_state[current_lp]->num_areas;
		m_state[current_lp]->num_areas++;
		prev_area->next = m_area->idx;
		m_area->prev = prev_area->idx;

	}

	if(m_area->area == NULL){

		num_chunks = m_area->num_chunks;
		bitmap_blocks = num_chunks / NUM_CHUNKS_PER_BLOCK;
		if(bitmap_blocks < 1)
			bitmap_blocks = 1;

		// lp_malloc
		m_area->use_bitmap = (unsigned int *)lp_malloc(bitmap_blocks * BLOCK_SIZE * 2 + num_chunks * size);

		if(m_area->use_bitmap == NULL){
			rootsim_error(true, "DyMeLoR: error allocating space for the use bitmap");
		}

		for(j = 0; j < (bitmap_blocks * 2); j++)
			m_area->use_bitmap[j] = 0;
		m_area->dirty_chunks = 0;

		m_area->dirty_bitmap = (unsigned int*)((char*)m_area->use_bitmap + bitmap_blocks * BLOCK_SIZE);

		m_area->area = (void*)((char*)m_area->use_bitmap + bitmap_blocks * BLOCK_SIZE * 2);
	}

	if(m_area->area == NULL)
		printf("(%d) LP %d PTR NULL\n", kid, current_lp);

	ptr = (void*)((char*)m_area->area + (m_area->next_chunk * size));

	SET_USE_BIT(m_area, m_area->next_chunk);
	
	bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
	if(bitmap_blocks < 1)
		bitmap_blocks = 1;


	if(m_area->alloc_chunks == 0){
		m_state[current_lp]->bitmap_size += bitmap_blocks * BLOCK_SIZE;
		m_state[current_lp]->busy_areas++;
	}
	
	if(m_area->state_changed == 0) {
		m_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
		m_state[current_lp]->dirty_areas++;
	}

	m_area->state_changed = 1;

	m_area->alloc_chunks++;
	m_area->last_access = current_lvt;

	if(!CHECK_LOG_MODE_BIT(m_area)){
		if((double)m_area->alloc_chunks / (double)m_area->num_chunks > MAX_LOG_THRESHOLD){
			SET_LOG_MODE_BIT(m_area);
			m_state[current_lp]->total_log_size += (m_area->num_chunks - (m_area->alloc_chunks - 1)) * size;
		} else
			m_state[current_lp]->total_log_size += size;
	}
	
	find_next_free(m_area);

	int chk_size = m_area->chunk_size;
	RESET_BIT_AT(chk_size, 0);
	RESET_BIT_AT(chk_size, 1);

	// Add a line in chunk address cache
	malloc_area_idx = m_area - (m_state[current_lp]->areas);
	final_address = (char *)ptr + chk_size;
	load_cache_line(ptr, final_address, malloc_area_idx);

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

	malloc_area * m_area;
	int idx, bitmap_blocks;
	size_t chunk_size;
	
	if(rootsim_config.serial) {
		rsfree(ptr);
		return;
	}
	
	if(ptr == NULL){
		rootsim_error(false, "Invalid pointer during free");
		return;
	}


	m_area = get_area(ptr);
	if(m_area == NULL){
		rootsim_error(false, "Invalid pointer during free: malloc_area NULL");
		return;
	}

	chunk_size = m_area->chunk_size;
	RESET_BIT_AT(chunk_size, 0);
	RESET_BIT_AT(chunk_size, 1);
	idx = (int)((char*)ptr - (char*)m_area->area) / chunk_size;

	RESET_USE_BIT(m_area, idx);

        bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
        if(bitmap_blocks < 1)
                bitmap_blocks = 1;


	if (m_area->state_changed == 0){
                m_state[current_lp]->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
                m_state[current_lp]->dirty_areas++;
        }

	if(CHECK_DIRTY_BIT(m_area, idx)){
                RESET_DIRTY_BIT(m_area, idx);
                m_area->dirty_chunks--;

                if (m_area->state_changed == 1 && m_area->dirty_chunks == 0)
                        m_state[current_lp]->dirty_bitmap_size -= bitmap_blocks * BLOCK_SIZE;

                m_state[current_lp]->total_inc_size -= chunk_size;

                if (m_area->dirty_chunks < 0)
			rootsim_error(true, "Dirty chunk is negative");
        }

	m_area->state_changed = 1;

	if(idx < m_area->next_chunk)
		m_area->next_chunk = idx;


	m_area->last_access = current_lvt;
	m_area->alloc_chunks--;


	if(m_area->alloc_chunks == 0){

		m_state[current_lp]->bitmap_size -= bitmap_blocks * BLOCK_SIZE;
		m_state[current_lp]->busy_areas--;
	}

	if( CHECK_LOG_MODE_BIT(m_area)){
		if((double)m_area->alloc_chunks / (double)m_area->num_chunks < MIN_LOG_THRESHOLD){
			RESET_LOG_MODE_BIT(m_area);
			m_state[current_lp]->total_log_size -= (m_area->num_chunks - m_area->alloc_chunks) * chunk_size;
		}
	} else 
		m_state[current_lp]->total_log_size -= chunk_size;
	
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
	if (m_area == NULL) {
		return NULL;
	}

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
* calls calloc, the call is redirected to this piece of code which rely on wrap_malloc
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




/**
* This function resets the state of the currently scheduled LP. Must be necessarily invoked after the
* simulation 
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*/
void reset_state(void){

	int i, j;
	malloc_area * m_area;

	m_state[current_lp]->total_log_size = 0;
	m_state[current_lp]->total_inc_size = 0;
	m_state[current_lp]->bitmap_size = 0;
	m_state[current_lp]->dirty_bitmap_size = 0;
	m_state[current_lp]->busy_areas = 0;
	m_state[current_lp]->dirty_areas = 0;
	m_state[current_lp]->num_areas = NUM_AREAS;
	m_state[current_lp]->max_num_areas = MAX_NUM_AREAS;
	m_state[current_lp]->timestamp = -1;

	for(i = 0; i < m_state[current_lp]->num_areas; i++){

		m_area = &m_state[current_lp]->areas[i];
		m_area->alloc_chunks = 0;
		m_area->dirty_chunks = 0;
		m_area->next_chunk = 0;
		m_area->state_changed = 0;
		m_area->last_access = current_lvt;
		m_area->prev = -1;
		m_area->next = -1;
		RESET_LOG_MODE_BIT(m_area);
		RESET_AREA_LOCK_BIT(m_area);

		for(j = 0; j < (m_area->num_chunks / NUM_CHUNKS_PER_BLOCK) * 2; j++)
			m_area->use_bitmap[j] = 0;

	}

}


/**
* This function whether a log is incremental or not
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param log Pointer to a valid log (no sanity check is performed on log, for efficiency purposes)
* @return true if log points to a partial log, false otherwise. Note that if log points 
*/
bool is_incremental(void *log) {
	if (((malloc_state *)log)->is_incremental == 1)
		return 1;
	return 0;
}



/**
* This function is used to statistically check if a malloc area has been dirtied since the last log.
*
* @author Roberto Vitali
*
* @param lid The Logical Process id
* @param last_state the state to perform the check against
* @param checked_bytes 
*/
size_t dirty_size(unsigned int lid, void *last_state, double *checked_bytes) {

	char *ptr, *check_address;
	malloc_area *curr_old_area, *curr_area;
	int i, j, k, bitmap_blocks, num_old_areas, chunk_size, big_check_size;
	unsigned int *old_bitmap, cmp_bitmap;
	int mem_check;
	int dirty_chunks;
	size_t dirty_memory = 0, dirty_area;
	int big_size_offset;
	int j_blocks_skip;

	*checked_bytes = 0;

	num_old_areas = ((malloc_state *)last_state)->busy_areas;

	ptr = ((char *)last_state) + sizeof(malloc_state);
	ptr = ptr + sizeof(seed_type);

	dirty_memory += sizeof(malloc_state);
	dirty_memory += sizeof(seed_type);

	for(i = 0; i < num_old_areas; i++) {
		dirty_area = 0;
		dirty_chunks = 0;

		curr_old_area = (malloc_area *)ptr;

		chunk_size = curr_old_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);
		RESET_BIT_AT(chunk_size, 1);
		big_check_size = chunk_size * CHECK_SIZE;
		big_size_offset = (int)((3./4) * chunk_size) - big_check_size; 

		curr_area = &m_state[lid]->areas[curr_old_area->idx];

		if (curr_old_area->idx < 0 || curr_old_area->idx > num_old_areas)
			break;

		ptr += sizeof(malloc_area);
		old_bitmap = (unsigned int *)ptr;

		bitmap_blocks = curr_old_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
		if(bitmap_blocks < 1)
			bitmap_blocks = 1;

		ptr += BLOCK_SIZE * bitmap_blocks;
		for(j = 0; j < bitmap_blocks; j++) {
			cmp_bitmap = old_bitmap[j] & curr_area->use_bitmap[j];

			j_blocks_skip = j * NUM_CHUNKS_PER_BLOCK;

			for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++) {

				if(CHECK_BIT_AT(cmp_bitmap, k)) {

					if(chunk_size <= LITTLE_SIZE) {

						check_address = (char *)curr_area->area + (j_blocks_skip + k) * chunk_size; 

						mem_check = memcmp(ptr, check_address, chunk_size);
						if (mem_check != 0) {
							dirty_chunks++;
							dirty_memory += chunk_size;
							dirty_area = 1;
							*checked_bytes += chunk_size;
						}
					} else {

						// Begin
						check_address = (char *)curr_area->area + (j_blocks_skip + k) * chunk_size; 
						mem_check = memcmp(ptr, check_address, big_check_size);
						if (mem_check != 0) {
							dirty_chunks++;
							dirty_area = 1;
							dirty_memory += chunk_size;
							*checked_bytes += big_check_size;
							goto check_next_chunk;
						}

						// 3/4
						check_address = (char *)curr_area->area + (j_blocks_skip + k) * chunk_size + big_size_offset;
						mem_check = memcmp(ptr + big_size_offset, check_address, big_check_size);
						if (mem_check != 0) {
							dirty_chunks++;
							dirty_area = 1;
							dirty_memory += chunk_size;
							*checked_bytes += big_check_size;
						}
					}
				}
				else {
					if(CHECK_BIT_AT(curr_area->use_bitmap[j], k))
						dirty_memory += chunk_size;
				}
				check_next_chunk:

				if(CHECK_BIT_AT(old_bitmap[j], k))
					ptr += chunk_size; 
			}
		}
		if (dirty_area == 1) {
			dirty_memory += sizeof(malloc_area) + 2 * BLOCK_SIZE * bitmap_blocks;
		}
	}

	*checked_bytes /= m_state[lid]->total_log_size;
	return  dirty_memory;

}



void clean_buffers_on_gvt(unsigned int lid, simtime_t time_barrier){

	int i;
	int increment_era = 0;
	malloc_state *state;
	malloc_area *m_area;

	state = m_state[lid];

	// The first NUM_AREAS malloc_areas are placed accordind to their chunks' sizes. The exceeding malloc_areas can be compacted
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
				// Era has to be incremented
				increment_era = 1;
			}
		}
	}

	if(increment_era)
		era++;
}


