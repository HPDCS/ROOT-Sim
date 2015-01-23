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




/// Used to monitor the internal cost to checkpoint one byte of memory
double checkpoint_cost_per_byte;

/// Used to monitor the internal cost to restore one byte of memory
double recovery_cost_per_byte;

/// Keeps track of how many checkpointing operations have been invoked so far
unsigned long total_checkpoints;

/// Keeps track of how many recovery operations have been invoked so far
unsigned long total_recoveries;

double checkpoint_bytes_total;



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

	void *ptr, *log;
	int i, j, k, idx, bitmap_blocks;
	size_t size, chunk_size;
	malloc_area * m_area;
	
	// Timers for self-tuning of the simulation platform
	timer checkpoint_timer;
	timer_start(checkpoint_timer);

	size = sizeof(malloc_state)  + sizeof(seed_type) + m_state[lid]->busy_areas * sizeof(malloc_area) + m_state[lid]->bitmap_size + m_state[lid]->total_log_size;

	// This code is in a malloc-wrapper package, so here we call the real malloc
	log = rsalloc(size);
	
	if(log == NULL)
		rootsim_error(true, "(%d) Unable to acquire memory for logging the current state (memory exhausted?)");


	ptr = log;

	// Copy malloc_state in the log
	memcpy(ptr, m_state[lid], sizeof(malloc_state));
	ptr = (void *)((char *)ptr + sizeof(malloc_state));
	((malloc_state*)log)->timestamp = current_lvt;
	((malloc_state*)log)->is_incremental = 0;


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

		// Copy malloc_area into the log
		memcpy(ptr, m_area, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));

		memcpy(ptr, m_area->use_bitmap, bitmap_blocks * BLOCK_SIZE);
		ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

		chunk_size = m_area->chunk_size;
		RESET_BIT_AT(chunk_size, 0);	// Log Mode bit
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

		// Reset Dirty Bitmap, as there is a full log in the chain now
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;
		bzero((void *)m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);

	} // For each m_area in m_state

	// Sanity check
	if ((char *)log + size != ptr){
		rootsim_error(false, "Actual (full) log size different from the estimated one! log = %p size = %x (%d), ptr = %p\n", log, size, size, ptr);
	}

	m_state[lid]->dirty_areas = 0;
	m_state[lid]->dirty_bitmap_size = 0;
	m_state[lid]->total_inc_size = 0;

	int checkpoint_time = timer_value_micro(checkpoint_timer);
	checkpoint_cost_per_byte += checkpoint_time;
	total_checkpoints++;
	checkpoint_bytes_total += size;

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
void restore_full(int lid, void *log) {

	void * ptr;
	int i, j, k, bitmap_blocks, idx, original_num_areas, restored_areas;
	unsigned int bitmap;
	size_t chunk_size;
	malloc_area *m_area, *new_area;

	// Timers for simulation platform self-tuning
	timer recovery_timer;
	timer_start(recovery_timer);
	restored_areas = 0;
	ptr = log;
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
	recovery_cost_per_byte += recovery_time;
	total_recoveries++;

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
	restore_full(lid, state_queue_node->log);
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
void log_delete(void *log){
	if(log != NULL) {
		rsfree(log);
	}
}

