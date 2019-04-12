/**
* @file mm/checkpoints.c
*
* @brief State saving and restore for model state buffers
*
* State saving and restore for model state buffers
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
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
* @author Roberto Toccaceli
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @author Francesco Quaglia
*/
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <mm/mm.h>
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
* @param lp A pointer to the lp_struct of the LP for which we are taking
*           a full log of the buffers keeping the current simulation state.
* @return A pointer to a malloc()'d memory area which contains the full log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*
* @todo must be declared static. This will entail changing the logic in gvt.c to save a state before rebuilding.
*/
void *log_full(struct lp_struct *lp)
{

	void *ptr = NULL, *ckpt = NULL;
	int i;
	size_t size, chunk_size, bitmap_size;
	malloc_area *m_area;

	// Timers for self-tuning of the simulation platform
	timer checkpoint_timer;
	timer_start(checkpoint_timer);

	lp->mm->m_state->is_incremental = false;
	size = get_log_size(lp->mm->m_state);

	ckpt = rsalloc(size);

	if (unlikely(ckpt == NULL)) {
		rootsim_error(true, "(%d) Unable to acquire memory for checkpointing the current state (memory exhausted?)", lp->lid.to_int);
	}

	ptr = ckpt;

	// Copy malloc_state in the ckpt
	memcpy(ptr, lp->mm->m_state, sizeof(malloc_state));
	ptr = (void *)((char *)ptr + sizeof(malloc_state));
	((malloc_state *) ckpt)->timestamp = lvt(lp);

	for (i = 0; i < lp->mm->m_state->num_areas; i++) {

		m_area = &lp->mm->m_state->areas[i];

		// Copy the bitmap

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		// Check if there is at least one chunk used in the area
		if (unlikely(m_area->alloc_chunks == 0)) {

			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;

			if (likely(m_area->use_bitmap != NULL)) {
				memset(m_area->dirty_bitmap, 0, bitmap_size);
			}

			continue;
		}
		// Copy malloc_area into the ckpt
		memcpy(ptr, m_area, sizeof(malloc_area));
		ptr = (void *)((char *)ptr + sizeof(malloc_area));

		memcpy(ptr, m_area->use_bitmap, bitmap_size);
		ptr = (void *)((char *)ptr + bitmap_size);

		chunk_size = UNTAGGED_CHUNK_SIZE(m_area);

		// Check whether the area should be completely copied (not on a per-chunk basis)
		// using a threshold-based heuristic
		if (CHECK_LOG_MODE_BIT(m_area)) {

			// If the malloc_area is almost (over a threshold) full, copy it entirely
			memcpy(ptr, m_area->area, m_area->num_chunks * chunk_size);
			ptr = (void *)((char *)ptr + m_area->num_chunks * chunk_size);

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

	}			// For each malloc area

	// Sanity check
	if (unlikely((char *)ckpt + size != ptr))
		rootsim_error(true, "Actual (full) ckpt size is wrong by %d bytes!\nlid = %d ckpt = %p size = %#x (%d), ptr = %p, ckpt + size = %p\n",
			      (char *)ckpt + size - (char *)ptr, lp->lid.to_int,
			      ckpt, size, size, ptr, (char *)ckpt + size);

	lp->mm->m_state->dirty_areas = 0;
	lp->mm->m_state->dirty_bitmap_size = 0;
	lp->mm->m_state->total_inc_size = 0;

	statistics_post_data(lp, STAT_CKPT_TIME, (double)timer_value_micro(checkpoint_timer));
	statistics_post_data(lp, STAT_CKPT_MEM, (double)size);

	return ckpt;
}

/**
* This function is the only log function which should be called from the simulation platform. Actually,
* it is a demultiplexer which calls the correct function depending on the current configuration of the
* platform. Note that this function only returns a pointer to a malloc'd area which contains the
* state buffers. This means that this memory area cannot be used as-is, but should be wrapped
* into a state_t structure, which gives information about the simulation state pointer (defined
* via SetState() by the application-level code and the lvt associated with the log.
* This is done implicitly by the LogState() function, which in turn connects the newly taken
* snapshot with the currencly-scheduled LP.
* Therefore, any point of the simulator which wants to take a (real) log, shouldn't call directly this
* function, rather LogState() should be used, after having correctly set current and current_lvt.
*
* @author Alessandro Pellegrini
*
* @param lp A pointer to the lp_struct of the LP for which we want to take
*           a snapshot of the buffers used by the model to keep state variables.
* @return A pointer to a malloc()'d memory area which contains the log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*/
void *log_state(struct lp_struct *lp)
{
	statistics_post_data(lp, STAT_CKPT, 1.0);
	return log_full(lp);
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
* @param lp A pointer to the lp_struct of the LP for which we are restoring
*           the content of simulation state buffers, taking it from the checkpoint
* @param ckpt A pointer to the checkpoint to take the previous content of
*             the buffers to be restored
*/
void restore_full(struct lp_struct *lp, void *ckpt)
{
	void *ptr;
	int i, original_num_areas, restored_areas;
	size_t chunk_size, bitmap_size;
	malloc_area *m_area, *new_area;

	// Timers for simulation platform self-tuning
	timer recovery_timer;
	timer_start(recovery_timer);
	restored_areas = 0;
	ptr = ckpt;
	original_num_areas = lp->mm->m_state->num_areas;
	new_area = lp->mm->m_state->areas;

	// Restore malloc_state
	memcpy(lp->mm->m_state, ptr, sizeof(malloc_state));
	ptr = (void *)((char *)ptr + sizeof(malloc_state));

	lp->mm->m_state->areas = new_area;

	// Scan areas and chunk to restore them
	for (i = 0; i < lp->mm->m_state->num_areas; i++) {

		m_area = &lp->mm->m_state->areas[i];

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		if (restored_areas == lp->mm->m_state->busy_areas || m_area->idx != ((malloc_area *) ptr)->idx) {

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
			m_area->last_access = lp->mm->m_state->timestamp;

			continue;
		}
		// Restore the malloc_area
		memcpy(m_area, ptr, sizeof(malloc_area));
		ptr = (void *)((char *)ptr + sizeof(malloc_area));

		restored_areas++;

		// Restore use bitmap
		memcpy(m_area->use_bitmap, ptr, bitmap_size);
		ptr = (void *)((char *)ptr + bitmap_size);

		// Reset dirty bitmap
		bzero(m_area->dirty_bitmap, bitmap_size);
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;

		chunk_size = UNTAGGED_CHUNK_SIZE(m_area);

		// Check how the area has been logged
		if (CHECK_LOG_MODE_BIT(m_area)) {
			// The area has been entirely logged
			memcpy(m_area->area, ptr, m_area->num_chunks * chunk_size);
			ptr = (void *)((char *)ptr + m_area->num_chunks * chunk_size);

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
	if (original_num_areas > lp->mm->m_state->num_areas) {

		for (i = lp->mm->m_state->num_areas; i < original_num_areas; i++) {

			m_area = &lp->mm->m_state->areas[i];
			m_area->alloc_chunks = 0;
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->next_chunk = 0;
			m_area->last_access = lp->mm->m_state->timestamp;
			lp->mm->m_state->areas[m_area->prev].next = m_area->idx;

			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);

			if (likely(m_area->use_bitmap != NULL)) {
				bitmap_size = bitmap_required_size(m_area->num_chunks);

				memset(m_area->use_bitmap, 0, bitmap_size);
				memset(m_area->dirty_bitmap, 0, bitmap_size);
			}
		}
		lp->mm->m_state->num_areas = original_num_areas;
	}

	lp->mm->m_state->timestamp = -1;
	lp->mm->m_state->is_incremental = false;
	lp->mm->m_state->dirty_areas = 0;
	lp->mm->m_state->dirty_bitmap_size = 0;
	lp->mm->m_state->total_inc_size = 0;

	statistics_post_data(lp, STAT_RECOVERY_TIME, (double)timer_value_micro(recovery_timer));
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
* @param lp A pointer to the lp_struct of the LP for which we are restoring
*           model-specific buffers keeping the simulation state
* @param state_queue_node a pointer to a node in the state queue keeping the state
*        which must be restored in the logical process live image
*/
void log_restore(struct lp_struct *lp, state_t *state_queue_node)
{
	statistics_post_data(lp, STAT_RECOVERY, 1.0);
	restore_full(lp, state_queue_node->log);
}

/**
* This function is called directly from the simulation platform kernel to delete a certain log
* during the fossil collection.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param ckpt a pointer to the simulation state which must be deleted
*
*/
void log_delete(void *ckpt)
{
	if (likely(ckpt != NULL)) {
		rsfree(ckpt);
	}
}
