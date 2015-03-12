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
* @file checkpoints.c
* @brief This module implements the routines to save and restore checkpoints,
*        both full and incremental
* @author Roberto Toccaceli
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @author Francesco Quaglia
*/
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <mm/dymelor.h>
#include <mm/malloc.h>
#include <core/timer.h>
#include <core/core.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <statistics/statistics.h>


/**
* This function creates a full log of the current simulation states and returns a pointer to it.
* The algorithm behind this function is based on packing of the really allocated memory chunks into
* a contiguous memory area, exploiting some threshold-based approach to fasten it up even more.
* The contiguous copy is performed precomputing the size of the full log, and then scanning it
* using a pointer for storing the relevant information.
*
* For further information, please see the paper:
* 	R. Toccaceli, F. Quaglia
* 	DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects
* 	with Generic Memory Layout
*	Proceedings of the 22nd Workshop on Principles of Advanced and Distributed Simulation
*	2008
*
* To fully understand the changes in this function to support the incremental logging as well,
* please point to the paper:
* 	A. Pellegrini, R. Vitali, F. Quaglia
* 	Di-DyMeLoR: Logging only Dirty Chunks for Efficient Management of Dynamic Memory Based
* 	Optimistic Simulation Objects
*	Proceedings of the 23rd Workshop on Principles of Advanced and Distributed Simulation
*	2009
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The logical process' local identifier
* @return A pointer to a malloc()'d memory area which contains the full log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*
* @todo must be declared static. This will entail changing the logic in gvt.c to save a state before rebuilding.
*/
void *log_full(int lid) {

	void *ptr, *ckpt;
	int i, j, k, idx, bitmap_blocks;
	size_t size, chunk_size;
	malloc_area * m_area;

	// Timers for self-tuning of the simulation platform
	timer checkpoint_timer;
	timer_start(checkpoint_timer);

	size = sizeof(malloc_state)  + sizeof(seed_type) + m_state[lid]->busy_areas * sizeof(malloc_area) + m_state[lid]->bitmap_size + m_state[lid]->total_log_size;

	// This code is in a malloc-wrapper package, so here we call the real malloc
	ckpt = rsalloc(size);

	if(ckpt == NULL)
		rootsim_error(true, "(%d) Unable to acquire memory for ckptging the current state (memory exhausted?)");


	ptr = ckpt;

	// Copy malloc_state in the ckpt
	memcpy(ptr, m_state[lid], sizeof(malloc_state));
	ptr = (void *)((char *)ptr + sizeof(malloc_state));
	((malloc_state*)ckpt)->timestamp = current_lvt;
	((malloc_state*)ckpt)->is_incremental = 0;


	// Copy the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(ptr, &LPS[lid]->seed, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));


	for(i = 0; i < m_state[lid]->num_areas; i++){

		m_area = &m_state[lid]->areas[i];

		// Copy the bitmap
		bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
		if (bitmap_blocks < 1)
			bitmap_blocks = 1;

		// Check if there is at least one chunk used in the area
		if(m_area->alloc_chunks == 0){

			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;

			if (m_area->use_bitmap != NULL) {
				for(j = 0; j < bitmap_blocks; j++)
					m_area->dirty_bitmap[j] = 0;
			}

			continue;
		}

		// Copy malloc_area into the ckpt
		memcpy(ptr, m_area, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));

		memcpy(ptr, m_area->use_bitmap, bitmap_blocks * BLOCK_SIZE);
		ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

		chunk_size = m_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);	// ckpt Mode bit
		RESET_BIT_AT(chunk_size, 1);	// Lock bit

		// Check whether the area should be completely copied (not on a per-chunk basis)
		// using a threshold-based heuristic
		if(CHECK_LOG_MODE_BIT(m_area)){

			// If the malloc_area is almost (over a threshold) full, copy it entirely
			memcpy(ptr, m_area->area, m_area->num_chunks * chunk_size);
			ptr = (void*)((char*)ptr + m_area->num_chunks * chunk_size);

		} else {
			// Copy only the allocated chunks
			for(j = 0; j < bitmap_blocks; j++){

				// Check the allocation bitmap on a per-block basis, to enhance scan speed
				if(m_area->use_bitmap[j] == 0) {
					// Empty (no-chunks-allocated) block: skip to the next
					continue;

				} else {
					// At least one chunk is allocated: per-bit scan of the block is required
					for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){

						if(CHECK_BIT_AT(m_area->use_bitmap[j], k)){

							idx = j * NUM_CHUNKS_PER_BLOCK + k;
							memcpy(ptr, (void*)((char*)m_area->area + (idx * chunk_size)), chunk_size);
							ptr = (void*)((char*)ptr + chunk_size);

						}
					}
				}
			}
		}

		// Reset Dirty Bitmap, as there is a full ckpt in the chain now
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;
		bzero((void *)m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);

	} // For each m_area in m_state

	// Sanity check
	if ((char *)ckpt + size != ptr){
		rootsim_error(false, "Actual (full) ckpt size different from the estimated one! ckpt = %p size = %x (%d), ptr = %p\n", ckpt, size, size, ptr);
	}

	m_state[lid]->dirty_areas = 0;
	m_state[lid]->dirty_bitmap_size = 0;
	m_state[lid]->total_inc_size = 0;

	int checkpoint_time = timer_value_micro(checkpoint_timer);
	statistics_post_lp_data(lid, STAT_CKPT_TIME, (double)checkpoint_time);
	statistics_post_lp_data(lid, STAT_CKPT_MEM, (double)size);

	return ckpt;
}




/**
* This function creates a partial (incremental) log of the current simulation states and returns
* a pointer to it.
* The algorithm behind this function is based on the information retrieved from the dirty bitmap,
* which is set accordingly to memory-write access patterns through the subroutine calls injected
* at compile time by the intrumentor.
* Similarly to full logs, a partial log is a contiguous memory area, holding metadata for a subsequent
* restore. Differently from it, a partial log holds only malloc_areas and chunks which have been
* modified since the last log operation.
* The contiguous copy is performed precomputing the size of the full log, and then scanning it
* using a pointer for storing the relevant information.
*
* For further information, please see the paper:
* 	A. Pellegrini, R. Vitali, F. Quaglia
* 	Di-DyMeLoR: Logging only Dirty Chunks for Efficient Management of Dynamic Memory Based
* 	Optimistic Simulation Objects
*	Proceedings of the 23rd Workshop on Principles of Advanced and Distributed Simulation
*	2009
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The logical process' local identifier
* @return A pointer to a malloc()'d memory area which contains the partial log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*
* @todo must be declared static. This will entail changing the logic in gvt.c to save a state before rebuilding.
*/
void *log_incremental(int lid) {
	void *ptr, *log;
	int i, j, k, index, bitmap_blocks, sporche = 0;
	size_t size, chunk_size;
	malloc_area *m_area;
	
	// Timers for self-tuning of the simulation platform
	//DECLARE_TIMER(checkpoint_timer);
	//TIMER_START(checkpoint_timer);
	timer checkpoint_timer;
	timer_start(checkpoint_timer);

	size = sizeof(malloc_state) + sizeof(seed_type) + m_state[lid]->dirty_areas * sizeof(malloc_area) + m_state[lid]->dirty_bitmap_size + m_state[lid]->total_inc_size;

	// This code is in a malloc-wrapper package, so here call the real malloc
//	log = __real_malloc(size);
	log = rsalloc(size);
	
	if(log == NULL){
		rootsim_error(true, "(%d) Unable to acquire memory for logging the current state (memory exhausted?)");
	}

	ptr = log;

	// Copy malloc_state into the log
	memcpy(ptr, m_state[lid], sizeof(malloc_state));
	ptr = (void*)((char*)ptr + sizeof(malloc_state));
	((malloc_state*)log)->timestamp = current_lvt;
	((malloc_state*)log)->is_incremental = 1;

	// Copy the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(ptr, &LPS[lid]->seed, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));


	for(i = 0; i < m_state[lid]->num_areas; i++){

		m_area = &m_state[lid]->areas[i];

		if (sporche == m_state[lid]->dirty_areas) {
			break;
		}

		bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
                if(bitmap_blocks < 1)
                        bitmap_blocks = 1;

		// Check if there is at least one chunk used in the area
		if(m_area->state_changed == 0){

			m_area->dirty_chunks = 0;

			if (m_area->use_bitmap != NULL) {
				for (j = 0; j < bitmap_blocks; j++)
					m_area->dirty_bitmap[j] = 0;
			}

                        continue;
                }

		sporche++;

		// Copy malloc_area into the log
		memcpy(ptr, m_area, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));

		// The area has at least one allocated chunk. Copy the bitmap.
                memcpy(ptr, m_area->use_bitmap, bitmap_blocks * BLOCK_SIZE);
                ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

                if (m_area->dirty_chunks == 0)
                        goto no_dirty;

                memcpy(ptr, m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);
                ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

		chunk_size = m_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);	// Log Mode bit
		RESET_BIT_AT(chunk_size, 1);	// Lock bit

		for(j = 0; j < bitmap_blocks; j++){

			// Check the allocation bitmap on a per-block basis, to enhance scan speed
			if(m_area->dirty_bitmap[j] == 0) {
				// Empty (no-chunks-allocated) block: skip to the next					
				continue;

			} else {
				// At least one chunk is allocated: per-bit scan of the block is required
				for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){

					if(CHECK_BIT_AT(m_area->dirty_bitmap[j], k)){

						index = j * NUM_CHUNKS_PER_BLOCK + k;
						memcpy(ptr, (void*)((char*)m_area->area + (index * chunk_size)), chunk_size);
						ptr = (void*)((char*)ptr + chunk_size);
					
						// Reset dirty bitmap
						RESET_BIT_AT(m_area->dirty_bitmap[j], k);
					}
				}
			}
		}
	    no_dirty:
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;
		bzero(m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);
		
	} // for each dirty m_area in m_state

	// Sanity check
	if ((char *)log + size != ptr){
		rootsim_error(false, "Actual (inc) log size different from the estimated one! Possible Undefined Behaviour! log = %x size = %x, ptr = %x. %d/%d + %d + %d", log, size, ptr, sporche, m_state[lid]->dirty_areas, m_state[lid]->dirty_bitmap_size, m_state[lid]->total_inc_size);
	}

        m_state[lid]->dirty_areas = 0;
	m_state[lid]->dirty_bitmap_size = 0;
	m_state[lid]->total_inc_size = 0;

	//TIMER_VALUE_MICRO(checkpoint_time, checkpoint_timer);
	int checkpoint_time = timer_value_micro(checkpoint_timer);
	//checkpoint_cost_per_byte += checkpoint_time;
	//total_checkpoints++;

	return log;
}

/**
* This function is the only log function which should be called from the simulation platform. Actually,
* it is a demultiplexer which calls the correct function depending on the current configuration of the
* platform. Note that this function only returns a pointer to a malloc'd area which contains the
* state buffers. This means that this memory area cannot be used as-is, but should be wrapped
* into a state_t structure, which gives information about the simulation state pointer (defined
* via <SetState>() by the application-level code and the lvt associated with the log.
* This is done implicitly by the <LogState>() function, which in turn connects the newly taken
* snapshot with the currencly-scheduled LP.
* Therefore, any point of the simulator which wants to take a (real) log, shouldn't call directly this
* function, rather <LogState>() should be used, after having correctly set current_lp and current_lvt.
*
* @author Alessandro Pellegrini
*
* @param lid The logical process' local identifier
* @return A pointer to a malloc()'d memory area which contains the log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*/
void *log_state(int lid) {
	statistics_post_lp_data(lid, STAT_CKPT, 1.0);
	return log_full(lid);
}




/**
* This function restores a full log in the address space where the logical process will be
* able to use it as the current state.
* Operations performed by the algorithm are mostly the opposite of the corresponding log_full
* function.
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
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
* @param lid The logical process' local identifier
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*/
void restore_full(int lid, void *ckpt) {

	void * ptr;
	int i, j, k, bitmap_blocks, idx, original_num_areas, restored_areas;
	unsigned int bitmap;
	size_t chunk_size;
	malloc_area *m_area, *new_area;

	// Timers for simulation platform self-tuning
	timer recovery_timer;
	timer_start(recovery_timer);
	restored_areas = 0;
	ptr = ckpt;
	original_num_areas = m_state[lid]->num_areas;
	new_area = m_state[lid]->areas;

	// Restore malloc_state
	memcpy(m_state[lid], ptr, sizeof(malloc_state));
	ptr = (void*)((char*)ptr + sizeof(malloc_state));

	// Restore the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(&LPS[lid]->seed, ptr, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));

	m_state[lid]->areas = new_area;

	// Scan areas and chunk to restore them
	for(i = 0; i < m_state[lid]->num_areas; i++){

		m_area = &m_state[lid]->areas[i];

		bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
		if(bitmap_blocks < 1)
			bitmap_blocks = 1;

		if(restored_areas == m_state[lid]->busy_areas || m_area->idx != ((malloc_area*)ptr)->idx){

			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			if (m_area->use_bitmap != NULL){
				for(j = 0; j < bitmap_blocks; j++) {
				  m_area->dirty_bitmap[j] = 0;
				}
			}
			m_area->alloc_chunks = 0;
			m_area->next_chunk = 0;
			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);
			if (m_area->use_bitmap != NULL) {
				for(j = 0; j < bitmap_blocks; j++)
					m_area->use_bitmap[j] = 0;
				for(j = 0; j < bitmap_blocks; j++)
					m_area->dirty_bitmap[j] = 0;
			}
			m_area->last_access = m_state[lid]->timestamp;

			continue;
		}

		// Restore the malloc_area
		memcpy(m_area, ptr, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));


		restored_areas++;

		// Restore use bitmap
		memcpy(m_area->use_bitmap, ptr, bitmap_blocks * BLOCK_SIZE);
		ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

		// Reset dirty bitmap
		bzero(m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;


		chunk_size = m_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);
		RESET_BIT_AT(chunk_size, 1);

		// Check how the area has been logged
		if(CHECK_LOG_MODE_BIT(m_area)){
			// The area has been entirely logged
			memcpy(m_area->area, ptr, m_area->num_chunks * chunk_size);
			ptr = (void*)((char*)ptr + m_area->num_chunks * chunk_size);

		} else {
			// The area was partially logged.
			// Logged chunks are the ones associated with a used bit whose value is 1
			// Their number is in the alloc_chunks counter
			for(j = 0; j < bitmap_blocks; j++){

				bitmap = m_area->use_bitmap[j];

				// Check the allocation bitmap on a per-block basis, to enhance scan speed
				if(bitmap == 0){
					// Empty (no-chunks-allocated) block: skip to the next
					continue;

				} else {
					// Scan the bitmap on a per-bit basis
					for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){

						if(CHECK_BIT_AT(bitmap, k)){

							idx = j * NUM_CHUNKS_PER_BLOCK + k;
							memcpy((void*)((char*)m_area->area + (idx * chunk_size)), ptr, chunk_size);
							ptr = (void*)((char*)ptr + chunk_size);

						}
					}
				}
			}
		}

	}


	// Check whether there are more allocated areas which are not present in the log
	if(original_num_areas > m_state[lid]->num_areas){

		for(i = m_state[lid]->num_areas; i < original_num_areas; i++){

			m_area = &m_state[lid]->areas[i];
			m_area->alloc_chunks = 0;
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->next_chunk = 0;
			m_area->last_access = m_state[lid]->timestamp;
			m_state[lid]->areas[m_area->prev].next = m_area->idx;

			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);

			if (m_area->use_bitmap != NULL) {
				bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
				if(bitmap_blocks < 1)
					bitmap_blocks = 1;

				for(j = 0; j < bitmap_blocks; j++)
					m_area->use_bitmap[j] = 0;
				for(j = 0; j < bitmap_blocks; j++)
                        	        m_area->dirty_bitmap[j] = 0;
			}
		}
		m_state[lid]->num_areas = original_num_areas;
	}

	m_state[lid]->timestamp = -1;
	m_state[lid]->is_incremental = -1;
	m_state[lid]->dirty_areas = 0;
	m_state[lid]->dirty_bitmap_size = 0;
	m_state[lid]->total_inc_size = 0;

	int recovery_time = timer_value_micro(recovery_timer);
	statistics_post_lp_data(lid, STAT_RECOVERY_TIME, (double)recovery_time);
}




/**
* This function restores a full log in the address space where the logical process will be
* able to use it as the current state.
* Operations performed by the algorithm are mostly the opposite of the corresponding log_full
* function.
*
* For further information, please see the paper:
* 	A. Pellegrini, R. Vitali, F. Quaglia
* 	Di-DyMeLoR: Logging only Dirty Chunks for Efficient Management of Dynamic Memory Based
* 	Optimistic Simulation Objects
*	Proceedings of the 23rd Workshop on Principles of Advanced and Distributed Simulation
*	2009
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The logical process' local identifier
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*/
void restore_incremental(int lid, state_t *queue_node) {

	void *ptr, *log;
	int	i, j, k, bitmap_blocks, index, num_areas,
		original_num_areas;
	unsigned int bitmap, xored_bitmap;
	unsigned int *bitmap_pointer;
	size_t chunk_size;
	malloc_area *m_area, *curr_m_area;
	state_t *curr_node;
	malloc_area *new_area;	
	int siz;

	//DECLARE_TIMER(recovery_timer);
	//TIMER_START(recovery_timer);
	timer recovery_timer;
	timer_start(recovery_timer);

	original_num_areas = m_state[lid]->num_areas;

	new_area = m_state[lid]->areas;

	// Restore malloc_state
	memcpy(m_state[lid], queue_node->log, sizeof(malloc_state));

	// Restore the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(&LPS[lid]->seed, (char *)queue_node->log + sizeof(malloc_state), sizeof(seed_type));

	m_state[lid]->areas = new_area;


	// max_areas: it's possible, scanning backwards, to find a greater number of active areas than the one
	// in the state we're rolling back to. Hence, max_areas keeps the greater number of areas ever reached
	// during the simulation
        unsigned int **already_restored = rsalloc(m_state[lid]->max_num_areas * sizeof(unsigned int *));

	for (i = 0; i < m_state[lid]->max_num_areas; i++)
		already_restored[i] = NULL;

	curr_node = queue_node;
	
	// Handle incremental logs
	while(((malloc_state *)(curr_node->log))->is_incremental > 0) {

		log = curr_node->log;
		ptr = log;
		
		// Skip malloc_state and seed
		ptr = (void*)((char*)ptr + sizeof(malloc_state) + sizeof(seed_type));
	
		// Get the number of areas in current log
		num_areas = ((malloc_state *)log)->dirty_areas;
		
		// Handle areas in current incremental log
		for(i = 0; i < num_areas; i++) {
		
			// Get current malloc_area
			curr_m_area = (malloc_area *)ptr;
			m_area = (malloc_area *)&m_state[lid]->areas[curr_m_area->idx];

			ptr = (void *)((char *)ptr + sizeof(malloc_area));
			
			chunk_size = curr_m_area->chunk_size;
			RESET_BIT_AT(chunk_size, 0);
			RESET_BIT_AT(chunk_size, 1);
			
			bitmap_blocks = curr_m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
			if(bitmap_blocks < 1)
                                bitmap_blocks = 1;


			// Check whether the malloc_area has not already been restored
                        if(already_restored[curr_m_area->idx] == NULL) {

                                // No chunks restored so far
                                already_restored[curr_m_area->idx] = rsalloc(bitmap_blocks * BLOCK_SIZE);
                                bzero(already_restored[curr_m_area->idx], bitmap_blocks * BLOCK_SIZE);

                                // Restore m_area
                                memcpy(m_area, curr_m_area, sizeof(malloc_area));

                                m_area->dirty_chunks = 0;
                                m_area->state_changed = 0;

                                // Restore use bitmap
                                memcpy(m_area->use_bitmap, ptr, bitmap_blocks * BLOCK_SIZE);

                                // Reset dirty bitmap
                                bzero((void *)m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);
                        }

                        // make ptr point to dirty bitmap
                        ptr = (void *)((char *)ptr + bitmap_blocks * BLOCK_SIZE);

                        if (curr_m_area->dirty_chunks == 0)
                                continue;

                        bitmap_pointer = (unsigned int*)ptr;

                        // make ptr point to chunks
                        ptr = (void *)((char *)ptr + bitmap_blocks * BLOCK_SIZE);


			// Check dirty bitmap for chunks to be restored
			for(j = 0; j < bitmap_blocks; j++){

				bitmap = bitmap_pointer[j];
			
				if(bitmap == 0){
					// No dirty chunks
					continue;
				} else {
					// At least one dirty chunk

					// Generate a new bitmap, telling what chunks must be restored
					xored_bitmap = already_restored[curr_m_area->idx][j] | bitmap;
					xored_bitmap = xored_bitmap ^ already_restored[curr_m_area->idx][j]; 
					
					for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){
	
						// Chunk present?
						if(CHECK_BIT_AT(bitmap, k)){
						
							// Chunk to be restored?
							if(CHECK_BIT_AT(xored_bitmap, k)) {
						
								// Restore chunk
								index = j * NUM_CHUNKS_PER_BLOCK + k;
								memcpy((void*)((char*)m_area->area + (index * chunk_size)), ptr, chunk_size);
								ptr = (void*)((char*)ptr + chunk_size);
							} else {
								// Skip chunk
								ptr = (void*)((char*)ptr + chunk_size);
							}
						}
					}
					
					// Save restored chunks
					already_restored[curr_m_area->idx][j] |= xored_bitmap;
				}
			}
		}
	
		// Handle previous log
	
		curr_node = list_next(curr_node);
		
		if(curr_node == NULL) {
			printf("PANIC!\n");
			fflush(stdout);
		}

		// Sanity check
		siz = sizeof(malloc_state) + sizeof(seed_type) + ((malloc_state *)log)->dirty_areas * sizeof(malloc_area) + ((malloc_state *)log)->dirty_bitmap_size + ((malloc_state *)log)->total_inc_size;
        	if (ptr != (char *)log + siz){
                	printf("ERROR: The incremental log size does not match\n");
	                fflush(stdout);
        	}

	}
	

	/* Full log reached */
	
	
	// Handle areas in the full log reached
	log = curr_node->log;
	ptr = log;
	
	// Skip malloc_state and seed
	ptr = (void *)((char *)ptr + sizeof(malloc_state) + sizeof(seed_type));
	
	// Get the number of busy areas in current log
	num_areas = ((malloc_state *)log)->busy_areas;
	
	for(i = 0; i < num_areas; i++) {
		

		// Get current malloc_area
		curr_m_area = (malloc_area *)ptr;

		m_area = (malloc_area *)&m_state[lid]->areas[curr_m_area->idx];
		ptr = (void *)((char *)ptr + sizeof(malloc_area));
			
		chunk_size = curr_m_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);
		RESET_BIT_AT(chunk_size, 1);
		
		bitmap_blocks = curr_m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
		if(bitmap_blocks < 1)
                                bitmap_blocks = 1;



		// Check whether the malloc_area has not already been restored
		if(already_restored[curr_m_area->idx] == NULL) {
			
			// No chunks restored so far
			already_restored[curr_m_area->idx] = rsalloc(bitmap_blocks * BLOCK_SIZE);
			bzero(already_restored[curr_m_area->idx], bitmap_blocks * BLOCK_SIZE);

			// Restore m_area
			memcpy(m_area, curr_m_area, sizeof(malloc_area));
			
			// Restore use bitmap
			memcpy(m_area->use_bitmap, ptr, bitmap_blocks * BLOCK_SIZE);
			
			// Reset dirty bitmap
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			bzero((void *)m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);
		}

		// ptr points to use bitmap
		bitmap_pointer = (unsigned int*)ptr;
		
		// ptr points to chunks
		ptr = (void *)((char *)ptr + bitmap_blocks * BLOCK_SIZE);
		
		
		if(CHECK_LOG_MODE_BIT(curr_m_area)){
			// The area has been logged completely.
			// So far, we have restored just the needed chunks. Anyway,
			// There are still some chunks to be skipped, at the tail of
			// the log.
						
			int processed_chunks = 0;
			
			for(j = 0; j < bitmap_blocks; j++) {
				//bitmap = bitmap_pointer[j] ;
				
				// bitmap = bitmap_pointer[j] | m_area->use_bitmap[j];
				bitmap = bitmap_pointer[j];

				if(bitmap == 0) {
					if(processed_chunks + NUM_CHUNKS_PER_BLOCK >= curr_m_area->num_chunks) {
						ptr = (void *)((char *)ptr + (curr_m_area->num_chunks - processed_chunks) * chunk_size);
						break;
					} else {
						ptr = (void *)((char *)ptr + NUM_CHUNKS_PER_BLOCK * chunk_size);
						processed_chunks += NUM_CHUNKS_PER_BLOCK;
						continue;
					}
				} else {
					// Generate a new bitmap, telling what chunks must be restored
					xored_bitmap = ~already_restored[curr_m_area->idx][j];

					for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){
						
						if(CHECK_BIT_AT(xored_bitmap, k)) {				
							// Restore chunk
							index = j * NUM_CHUNKS_PER_BLOCK + k;
							memcpy((void*)((char*)m_area->area + (index * chunk_size)), ptr, chunk_size);
						}
						
						ptr = (void*)((char*)ptr + chunk_size);
					}

					processed_chunks += NUM_CHUNKS_PER_BLOCK;
				}
			}
		} else {
			// Check use bitmap for chunks to be restored
			for(j = 0; j < bitmap_blocks; j++){

				bitmap = bitmap_pointer[j];	
				if(bitmap == 0){
					// No dirty chunks
					continue;
				} else {
					// At least one dirty chunk
					// Generate a new bitmap, telling what chunks must be restored
					xored_bitmap = ~already_restored[curr_m_area->idx][j];
	
					for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){
	
						// Chunk present?
						if(CHECK_BIT_AT(bitmap, k)){
						
							// Chunk to be restored?
							if(CHECK_BIT_AT(xored_bitmap, k)) {
						
								// Restore chunk
								index = j * NUM_CHUNKS_PER_BLOCK + k;
								memcpy((void*)((char*)m_area->area + (index * chunk_size)), ptr, chunk_size);
							}
							
							// Skip chunk
							ptr = (void*)((char*)ptr + chunk_size);
						}
					}
				}
			}
		}
	}

	// Sanity check
	siz = sizeof(malloc_state) + sizeof(seed_type) + ((malloc_state *)log)->busy_areas * sizeof(malloc_area) + ((malloc_state *)log)->bitmap_size + ((malloc_state *)log)->total_log_size;
	if (ptr != (char *)log + siz){
		printf("ERROR: full log size does not match\n");
		fflush(stdout);
	}

	// Vitali: realloc bug fix
	for(i = 0; i < m_state[lid]->num_areas; i++){

		if (already_restored[i] == NULL){
			m_area = &m_state[lid]->areas[i];
			m_area->alloc_chunks = 0;
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->next_chunk = 0;
			m_area->last_access = m_state[lid]->timestamp;

			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);
			bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
			if(bitmap_blocks < 1)
                       	        bitmap_blocks = 1;
			if (m_area->use_bitmap != NULL) {
				for(j = 0; j < bitmap_blocks; j++)  // Take dirty_bitmap into account as well
					m_area->use_bitmap[j] = 0;

				for(j = 0; j < bitmap_blocks; j++)
       		                        m_area->dirty_bitmap[j] = 0;
			}
		}
	}

	// Check whether there are more allocated areas which are not present in the log
	if(original_num_areas > m_state[lid]->num_areas){

		for(i = m_state[lid]->num_areas; i < original_num_areas; i++){
			m_area = &m_state[lid]->areas[i];
			m_area->alloc_chunks = 0;
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->next_chunk = 0;
			m_area->last_access = m_state[lid]->timestamp;
			m_state[lid]->areas[m_area->prev].next = m_area->idx;

			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);

			if (m_area->use_bitmap != NULL) {

				bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
				if(bitmap_blocks < 1)
	                                bitmap_blocks = 1;
				for(j = 0; j < bitmap_blocks; j++)  // Take dirty_bitmap into account as well
					m_area->use_bitmap[j] = 0;

				for(j = 0; j < bitmap_blocks; j++)
        	                        m_area->dirty_bitmap[j] = 0;
			}
		}
		m_state[lid]->num_areas = original_num_areas;
	}
	
	
	// Release data structures
	for(i = 0; i < m_state[lid]->max_num_areas ; i++) {
		if(already_restored[i] != NULL) {
			rsfree(already_restored[i]);
		}
	}
	rsfree(already_restored);

	m_state[lid]->timestamp = -1;
	m_state[lid]->is_incremental = -1;
	m_state[lid]->dirty_areas = 0;
	m_state[lid]->dirty_bitmap_size = 0;
	m_state[lid]->total_inc_size = 0;

	//TIMER_VALUE_MICRO(recovery_time, recovery_timer);
	int recovery_time = timer_value_micro(recovery_timer);
//	recovery_cost_per_byte += recovery_time;
//	total_recoveries++;
	
}




/**
* Upon the decision of performing a rollback operation, this function is invoked by the simulation
* kernel to perform a restore operation.
* This function checks the mark in the malloc_state telling whether we're dealing with a full or
* partial log, and calls the proper function accordingly
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The logical process' local identifier
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*/
void log_restore(int lid, state_t *state_queue_node) {
	statistics_post_lp_data(lid, STAT_RECOVERY, 1.0);
	restore_full(lid, state_queue_node->log);

	#if 0
	if (((malloc_state *)(queue_node->log))->is_incremental)
		restore_incremental(lid, queue_node);
	else
		restore_full(lid, queue_node->log);
	#endif
}



/**
* This function is called directly from the simulation platform kernel to delete a certain log
* during the fossil collection.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*
*/
void log_delete(void *ckpt){
	if(log != NULL) {
		rsfree(ckpt);
	}
}

