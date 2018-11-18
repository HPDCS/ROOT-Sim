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
void *log_full(LID_t the_lid) {

	void *ptr = NULL, *ckpt = NULL;
	int i;
	size_t size, chunk_size, bitmap_size;
	malloc_area *m_area;
	unsigned int lid = lid_to_int(the_lid);

	// Timers for self-tuning of the simulation platform
	timer checkpoint_timer;
	timer_start(checkpoint_timer);

	recoverable_state[lid]->is_incremental = false;
	size = get_log_size(recoverable_state[lid]);

	ckpt = rsalloc(size);

	if(unlikely(ckpt == NULL)) {
		rootsim_error(true, "(%d) Unable to acquire memory for checkpointing the current state (memory exhausted?)");
	}

	ptr = ckpt;

	// Copy malloc_state in the ckpt
	memcpy(ptr, recoverable_state[lid], sizeof(malloc_state));
	ptr = (void *)((char *)ptr + sizeof(malloc_state));
	((malloc_state*)ckpt)->timestamp = current_lvt;

	// Copy the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(ptr, &LPS(the_lid)->seed, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));

	for(i = 0; i < recoverable_state[lid]->num_areas; i++){

		m_area = &recoverable_state[lid]->areas[i];

		// Copy the bitmap

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		// Check if there is at least one chunk used in the area
		if(unlikely(m_area->alloc_chunks == 0)) {

			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;

			if (likely(m_area->use_bitmap != NULL)) {
				memset(m_area->dirty_bitmap, 0, bitmap_size);
			}

			continue;
		}

		// Copy malloc_area into the ckpt
		memcpy(ptr, m_area, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));

		memcpy(ptr, m_area->use_bitmap, bitmap_size);
		ptr = (void*)((char*)ptr + bitmap_size);

		chunk_size = UNTAGGED_CHUNK_SIZE(m_area);

		// Check whether the area should be completely copied (not on a per-chunk basis)
		// using a threshold-based heuristic
		if(CHECK_LOG_MODE_BIT(m_area)) {

			// If the malloc_area is almost (over a threshold) full, copy it entirely
			memcpy(ptr, m_area->area, m_area->num_chunks * chunk_size);
			ptr = (void*)((char*)ptr + m_area->num_chunks * chunk_size);

		} else {

#define copy_from_area(x) ({\
			memcpy(ptr, (void*)((char*)m_area->area + ((x) * chunk_size)), chunk_size);\
			ptr = (void*)((char*)ptr + chunk_size);})

			// Copy only the allocated chunks
			bitmap_foreach_set(m_area->use_bitmap, bitmap_size, copy_from_area);

#undef copy_from_area
		}

		// Reset Dirty Bitmap, as there is a full ckpt in the chain now
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;
		bzero((void *)m_area->dirty_bitmap, bitmap_size);

	} // For each m_area in recoverable_state

	// Sanity check
	if (unlikely((char *)ckpt + size != ptr))
		rootsim_error(true, "Actual (full) ckpt size is wrong by %d bytes!\nlid = %d ckpt = %p size = %#x (%d), ptr = %p, ckpt + size = %p\n", (char *)ckpt + size - (char *)ptr, lid, ckpt, size, size, ptr, (char *)ckpt + size);

	recoverable_state[lid]->dirty_areas = 0;
	recoverable_state[lid]->dirty_bitmap_size = 0;
	recoverable_state[lid]->total_inc_size = 0;

	statistics_post_data(the_lid, STAT_CKPT_TIME, (double)timer_value_micro(checkpoint_timer));
	statistics_post_data(the_lid, STAT_CKPT_MEM, (double)size);

	return ckpt;
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
void *log_state(LID_t lid) {
	statistics_post_data(lid, STAT_CKPT, 1.0);
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
void restore_full(LID_t the_lid, void *ckpt) {

	void * ptr;
	int i, original_num_areas, restored_areas;
	size_t chunk_size, bitmap_size;
	malloc_area *m_area, *new_area;
	unsigned int lid = lid_to_int(the_lid);

	// Timers for simulation platform self-tuning
	timer recovery_timer;
	timer_start(recovery_timer);
	restored_areas = 0;
	ptr = ckpt;
	original_num_areas = recoverable_state[lid]->num_areas;
	new_area = recoverable_state[lid]->areas;

	// Restore malloc_state
	memcpy(recoverable_state[lid], ptr, sizeof(malloc_state));
	ptr = (void*)((char*)ptr + sizeof(malloc_state));

	// Restore the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(&LPS(the_lid)->seed, ptr, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));

	recoverable_state[lid]->areas = new_area;

	// Scan areas and chunk to restore them
	for(i = 0; i < recoverable_state[lid]->num_areas; i++){

		m_area = &recoverable_state[lid]->areas[i];

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		if(restored_areas == recoverable_state[lid]->busy_areas || m_area->idx != ((malloc_area*)ptr)->idx){

			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->alloc_chunks = 0;
			m_area->next_chunk = 0;
			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);
			
			if (likely(m_area->use_bitmap != NULL)) {
				memset(m_area->use_bitmap, 0, bitmap_size);
				memset(m_area->dirty_bitmap, 0, bitmap_size);
			}
			m_area->last_access = recoverable_state[lid]->timestamp;

			continue;
		}

		// Restore the malloc_area
		memcpy(m_area, ptr, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));


		restored_areas++;

		// Restore use bitmap
		memcpy(m_area->use_bitmap, ptr, bitmap_size);
		ptr = (void*)((char*)ptr + bitmap_size);

		// Reset dirty bitmap
		bzero(m_area->dirty_bitmap, bitmap_size);
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;


		chunk_size = UNTAGGED_CHUNK_SIZE(m_area);

		// Check how the area has been logged
		if(CHECK_LOG_MODE_BIT(m_area)) {
			// The area has been entirely logged
			memcpy(m_area->area, ptr, m_area->num_chunks * chunk_size);
			ptr = (void*)((char*)ptr + m_area->num_chunks * chunk_size);

		} else {
			// The area was partially logged.
			// Logged chunks are the ones associated with a used bit whose value is 1
			// Their number is in the alloc_chunks counter

#define copy_to_area(x) ({\
		memcpy((void*)((char*)m_area->area + ((x) * chunk_size)), ptr, chunk_size);\
		ptr = (void*)((char*)ptr + chunk_size);})

			bitmap_foreach_set(m_area->use_bitmap, bitmap_size, copy_to_area);

#undef copy_to_area
		}

	}


	// Check whether there are more allocated areas which are not present in the log
	if(original_num_areas > recoverable_state[lid]->num_areas) {

		for(i = recoverable_state[lid]->num_areas; i < original_num_areas; i++) {

			m_area = &recoverable_state[lid]->areas[i];
			m_area->alloc_chunks = 0;
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->next_chunk = 0;
			m_area->last_access = recoverable_state[lid]->timestamp;
			recoverable_state[lid]->areas[m_area->prev].next = m_area->idx;

			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);

			if (likely(m_area->use_bitmap != NULL)) {
				bitmap_size = bitmap_required_size(m_area->num_chunks);

				memset(m_area->use_bitmap, 0, bitmap_size);
				memset(m_area->dirty_bitmap, 0, bitmap_size);
			}
		}
		recoverable_state[lid]->num_areas = original_num_areas;
	}

	recoverable_state[lid]->timestamp = -1;
	recoverable_state[lid]->is_incremental = false;
	recoverable_state[lid]->dirty_areas = 0;
	recoverable_state[lid]->dirty_bitmap_size = 0;
	recoverable_state[lid]->total_inc_size = 0;

	int recovery_time = timer_value_micro(recovery_timer);
	statistics_post_data(the_lid, STAT_RECOVERY_TIME, (double)recovery_time);
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
void log_restore(LID_t lid, state_t *state_queue_node) {
	statistics_post_data(lid, STAT_RECOVERY, 1.0);
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
void log_delete(void *ckpt){
	if(likely(ckpt != NULL)) {
		rsfree(ckpt);
	}
}

