/**
* @file mm/dymelor.c
*
* @brief Dynamic Memory Logger and Restorer (DyMeLoR)
*
* LP's memory manager.
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
*
* @date April 02, 2008
*/

#include <core/init.h>
#include <mm/mm.h>
#include <scheduler/scheduler.h>

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
static void malloc_area_init(malloc_area * m_area, size_t size, int num_chunks)
{
	m_area->chunk_size = size;
	m_area->alloc_chunks = 0;
	m_area->dirty_chunks = 0;
	m_area->next_chunk = 0;
	m_area->num_chunks = num_chunks;
	m_area->state_changed = 0;
	m_area->last_access = -1;
	m_area->use_bitmap = NULL;
	m_area->dirty_bitmap = NULL;
	m_area->self_pointer = NULL;
	m_area->area = NULL;

#ifndef NDEBUG
	atomic_set(&m_area->presence, 0);
#endif

	m_area->prev = -1;
	m_area->next = -1;

	SET_AREA_LOCK_BIT(m_area);

}

/**
* This function inizializes a malloc_state.
*/
malloc_state *malloc_state_init(void)
{
	int i, num_chunks;
	size_t chunk_size;
	malloc_state *state;

	state = rsalloc(sizeof(malloc_state));

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

	state->areas = (malloc_area *) rsalloc(state->max_num_areas * sizeof(malloc_area));
	if (unlikely(state->areas == NULL)) {
		rootsim_error(true, "Unable to allocate memory.\n");
	}

	chunk_size = MIN_CHUNK_SIZE;
	num_chunks = MIN_NUM_CHUNKS;

	for (i = 0; i < NUM_AREAS; i++) {

		malloc_area_init(&state->areas[i], chunk_size, num_chunks);
		state->areas[i].idx = i;
		chunk_size = chunk_size << 1;
	}

	return state;
}

void malloc_state_wipe(malloc_state **state_ptr)
{
	int i;
	malloc_state *state = *state_ptr;

	for (i = 0; i < NUM_AREAS; i++) {
		rsfree(state->areas[i].self_pointer); // TODO: when reintroducing the buddy, this must be changed
	}

	rsfree(*state_ptr);
	*state_ptr = NULL;
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
static size_t compute_size(size_t size)
{
	// Account for the space needed to keep the pointer to the malloc area
	size += sizeof(long long);
	size_t size_new;

	size_new = POWEROF2(size);
	if (unlikely(size_new < MIN_CHUNK_SIZE))
		size_new = MIN_CHUNK_SIZE;

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
static void find_next_free(malloc_area * m_area)
{
	m_area->next_chunk++;

	while (m_area->next_chunk < m_area->num_chunks) {
		if (!bitmap_check(m_area->use_bitmap, m_area->next_chunk))
			break;

		m_area->next_chunk++;
	}

}

void *do_malloc(struct lp_struct *lp, size_t size)
{
	malloc_area *m_area, *prev_area = NULL;
	void *ptr;
	size_t area_size, bitmap_size;

	size = compute_size(size);

	if (unlikely(size > MAX_CHUNK_SIZE)) {
		rootsim_error(false, "Requested a memory allocation of %d but the limit is %d. Reconfigure MAX_CHUNK_SIZE. Returning NULL.\n",
			      size, MAX_CHUNK_SIZE);
		return NULL;
	}

	m_area = &lp->mm->m_state->areas[(int)log2(size) -
					 (int)log2(MIN_CHUNK_SIZE)];

#ifndef NDEBUG
	atomic_inc(&m_area->presence);
	assert(atomic_read(&m_area->presence) == 1);
#endif

	while (m_area != NULL && m_area->alloc_chunks == m_area->num_chunks) {
		prev_area = m_area;
		if (m_area->next == -1) {
			m_area = NULL;
		} else {
			m_area = &(lp->mm->m_state->areas[m_area->next]);
#ifndef NDEBUG
			atomic_inc(&m_area->presence);
			assert(atomic_read(&m_area->presence) == 1);
#endif
		}
	}

#ifndef NDEBUG
	if (prev_area != NULL) {
		atomic_dec(&prev_area->presence);
	}
#endif

	if (m_area == NULL) {

		printf("Initializing an additional area\n");
		fflush(stdout);

		if (lp->mm->m_state->num_areas == lp->mm->m_state->max_num_areas) {

			malloc_area *tmp = NULL;

			if ((lp->mm->m_state->max_num_areas << 1) > MAX_LIMIT_NUM_AREAS) {
#ifndef NDEBUG
				atomic_dec(&m_area->presence);
#endif
				return NULL;
			}

			lp->mm->m_state->max_num_areas <<= 1;

			rootsim_error(true, "To reimplement\n");
//                      tmp = (malloc_area *)pool_realloc_memory(lp->mm->m_state->areas, lp->mm->m_state->max_num_areas * sizeof(malloc_area));
			if (tmp == NULL) {

				/**
				* @todo can we find a better way to handle the realloc failure?
				*/
				rootsim_error(false,
					      "DyMeLoR: cannot reallocate the block of malloc_area.\n");
				lp->mm->m_state->max_num_areas >>= 1;

#ifndef NDEBUG
				atomic_dec(&m_area->presence);
#endif
				return NULL;
			}

			lp->mm->m_state->areas = tmp;
		}
		// Allocate a new malloc area
		m_area = &lp->mm->m_state->areas[lp->mm->m_state->num_areas];

		// The malloc area to be instantiated has twice the number of chunks wrt the last full malloc area for the same chunks size
		malloc_area_init(m_area, size, prev_area->num_chunks << 1);

#ifndef NDEBUG
		atomic_inc(&m_area->presence);
		assert(atomic_read(&m_area->presence) == 1);
#endif

		m_area->idx = lp->mm->m_state->num_areas;
		lp->mm->m_state->num_areas++;
		prev_area->next = m_area->idx;
		m_area->prev = prev_area->idx;

	}

	if (m_area->area == NULL) {

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		area_size = sizeof(malloc_area *) + bitmap_size * 2 + m_area->num_chunks * size;

//              m_area->self_pointer = (malloc_area *)allocate_lp_memory(lp, area_size);
		m_area->self_pointer = rsalloc(area_size);
		bzero(m_area->self_pointer, area_size);

		if (unlikely(m_area->self_pointer == NULL)) {
			rootsim_error(true, "Error while allocating memory.\n");
		}

		m_area->dirty_chunks = 0;
		*(unsigned long long *)(m_area->self_pointer) =
		    (unsigned long long)m_area;

		m_area->use_bitmap =
		    ((unsigned char *)m_area->self_pointer +
		     sizeof(malloc_area *));

		m_area->dirty_bitmap =
		    ((unsigned char *)m_area->use_bitmap + bitmap_size);

		m_area->area =
		    (void *)((char *)m_area->dirty_bitmap + bitmap_size);
	}

	if (unlikely(m_area->area == NULL)) {
		rootsim_error(true, "Error while allocating memory.\n");
	}
#ifndef NDEBUG
	if (bitmap_check(m_area->use_bitmap, m_area->next_chunk)) {
		rootsim_error(true, "Error: reallocating an already allocated chunk at %s:%d\n");
	}
#endif

	ptr = (void *)((char *)m_area->area + (m_area->next_chunk * size));

	bitmap_set(m_area->use_bitmap, m_area->next_chunk);

	bitmap_size = bitmap_required_size(m_area->num_chunks);

	if (m_area->alloc_chunks == 0) {
		lp->mm->m_state->bitmap_size += bitmap_size;
		lp->mm->m_state->busy_areas++;
	}

	if (m_area->state_changed == 0) {
		lp->mm->m_state->dirty_bitmap_size += bitmap_size;
		lp->mm->m_state->dirty_areas++;
	}

	m_area->state_changed = 1;
	m_area->last_access = lvt(current);

	if (!CHECK_LOG_MODE_BIT(m_area)) {
		if ((double)m_area->alloc_chunks / (double)m_area->num_chunks > MAX_LOG_THRESHOLD) {
			SET_LOG_MODE_BIT(m_area);
			lp->mm->m_state->total_log_size += (m_area->num_chunks - (m_area->alloc_chunks - 1)) * size;
		} else
			lp->mm->m_state->total_log_size += size;
	}
	//~ int chk_size = m_area->chunk_size;
	//~ RESET_BIT_AT(chk_size, 0);
	//~ RESET_BIT_AT(chk_size, 1);

	m_area->alloc_chunks++;
	find_next_free(m_area);

	// TODO: togliere
	memset(ptr, 0xe8, size);

	// Keep track of the malloc_area which this chunk belongs to
	*(unsigned long long *)ptr = (unsigned long long)m_area->self_pointer;

#ifndef NDEBUG
	atomic_dec(&m_area->presence);
#endif

	return (void *)((char *)ptr + sizeof(long long));
}

void do_free(struct lp_struct *lp, void *ptr)
{
	(void)lp;

	malloc_area *m_area = NULL;
	int idx;
	size_t chunk_size, bitmap_size;

	if (unlikely(rootsim_config.serial)) {
		rsfree(ptr);
		return;
	}

	if (unlikely(ptr == NULL)) {
		rootsim_error(false, "Invalid pointer during free\n");
		return;
	}

	m_area = get_area(ptr);
	if (unlikely(m_area == NULL)) {
		rootsim_error(false,
			      "Invalid pointer during free: malloc_area NULL\n");
		return;
	}
#ifndef NDEBUG
	atomic_inc(&m_area->presence);
	assert(atomic_read(&m_area->presence) == 1);
#endif

	chunk_size = UNTAGGED_CHUNK_SIZE(m_area);

	idx = (int)((char *)ptr - (char *)m_area->area) / chunk_size;

	if (!bitmap_check(m_area->use_bitmap, idx)) {
		rootsim_error(false, "double free() corruption or address not malloc'd\n");
		abort();
	}
	bitmap_reset(m_area->use_bitmap, idx);

	bitmap_size = bitmap_required_size(m_area->num_chunks);

	m_area->alloc_chunks--;

	if (m_area->alloc_chunks == 0) {
		lp->mm->m_state->bitmap_size -= bitmap_size;
		lp->mm->m_state->busy_areas--;
	}

	if (m_area->state_changed == 0) {
		lp->mm->m_state->dirty_bitmap_size += bitmap_size;
		lp->mm->m_state->dirty_areas++;
	}

	if (bitmap_check(m_area->dirty_bitmap, idx)) {
		bitmap_reset(m_area->dirty_bitmap, idx);
		m_area->dirty_chunks--;

		if (m_area->state_changed == 1 && m_area->dirty_chunks == 0)
			lp->mm->m_state->dirty_bitmap_size -= bitmap_size;

		lp->mm->m_state->total_inc_size -= chunk_size;

		if (unlikely(m_area->dirty_chunks < 0)) {
			rootsim_error(true, "negative number of chunks\n");
		}
	}

	m_area->state_changed = 1;

	m_area->last_access = lvt(current);

	if (CHECK_LOG_MODE_BIT(m_area)) {
		if ((double)m_area->alloc_chunks / (double)m_area->num_chunks < MIN_LOG_THRESHOLD) {
			RESET_LOG_MODE_BIT(m_area);
			lp->mm->m_state->total_log_size -= (m_area->num_chunks - m_area->alloc_chunks) * chunk_size;
		}
	} else {
		lp->mm->m_state->total_log_size -= chunk_size;
	}

	if (idx < m_area->next_chunk)
		m_area->next_chunk = idx;

#ifndef NDEBUG
	atomic_dec(&m_area->presence);
#endif
	// TODO: when do we free unrecoverable areas?
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
void dirty_mem(void *base, int size)
{

	// TODO: Quando reintegriamo l'incrementale questo qui deve ricomparire!
	(void)base;
	(void)size;

	return;

#if 0
//      unsigned long long current_cost;

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
	int first_chunk, last_chunk, i, chk_size;

	size_t bitmap_size;

	malloc_area *m_area = get_area(base);

	if (m_area != NULL) {

		chk_size = UNTAGGED_CHUNK_SIZE(m_area->chunk_size);

		// Compute the number of chunks affected by the write
		first_chunk =
		    (int)(((char *)base - (char *)m_area->area) / chk_size);

		// If size == -1, then we adopt a conservative approach: dirty all the chunks from the base to the end
		// of the actual malloc area base address belongs to.
		// This has been inserted to support the wrapping of third-party libraries where the size of the
		// update (or even the actual update) cannot be statically/dynamically determined.
		if (size == -1)
			last_chunk = m_area->num_chunks - 1;
		else
			last_chunk = (int)(((char *)base + size - 1 - (char *)m_area->area) / chk_size);

		bitmap_size = bitmap_required_size(m_area->num_chunks);

		if (m_area->state_changed == 1) {
			if (m_area->dirty_chunks == 0)
				lp->mm->m_state->dirty_bitmap_size += bitmap_size;
		} else {
			lp->mm->m_state->dirty_areas++;
			lp->mm->m_state->dirty_bitmap_size += bitmap_size * 2;
			m_area->state_changed = 1;
		}

		for (i = first_chunk; i <= last_chunk; i++) {

			// If it is dirtied a clean chunk, set it dirty and increase dirty object count for the malloc_area
			if (!bitmap_check(m_area->dirty_bitmap, i)) {
				bitmap_set(m_area->dirty_bitmap, i);
				lp->mm->m_state->total_inc_size += chk_size;

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
size_t get_log_size(malloc_state * logged_state)
{
	if (unlikely(logged_state == NULL))
		return 0;

	if (is_incremental(logged_state)) {
		return sizeof(malloc_state) +
		    logged_state->dirty_areas * sizeof(malloc_area) +
		    logged_state->dirty_bitmap_size +
		    logged_state->total_inc_size;
	} else {
		return sizeof(malloc_state) +
		    logged_state->busy_areas * sizeof(malloc_area) +
		    logged_state->bitmap_size + logged_state->total_log_size;
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
void *__wrap_malloc(size_t size)
{
	void *ptr;

	switch_to_platform_mode();

	if (unlikely(rootsim_config.serial)) {
		ptr = rsalloc(size);
		goto out;
	}

	ptr = do_malloc(current, size);

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
void __wrap_free(void *ptr)
{
	switch_to_platform_mode();

	if (unlikely(rootsim_config.serial)) {
		rsfree(ptr);
		goto out;
	}

	do_free(current, ptr);

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
void *__wrap_realloc(void *ptr, size_t size)
{
	void *new_buffer;
	size_t old_size;
	size_t copy_size;
	malloc_area *m_area;

	if (unlikely(rootsim_config.serial)) {
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
	old_size = UNTAGGED_CHUNK_SIZE(m_area);

	new_buffer = __wrap_malloc(size);

	if (unlikely(new_buffer == NULL))
		return NULL;

	copy_size = min(size, old_size);
	memcpy(new_buffer, ptr, copy_size);
	__wrap_free(ptr);

	return new_buffer;
}

/**
* This is the wrapper of the real stdlib calloc(). Whenever the application level software
* calls calloc, the call is redirected to this piece of code which relies on wrap_malloc
*
* @author Roberto Vitali
*
* @param nmemb The number of elements to be allocated
* @param size The size of each allocated member
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_calloc(size_t nmemb, size_t size)
{
	void *buffer;

	if (unlikely(rootsim_config.serial)) {
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

void clean_buffers_on_gvt(struct lp_struct *lp, simtime_t time_barrier)
{
	int i;
	malloc_area *m_area;
	malloc_state *state = lp->mm->m_state;

	// The first NUM_AREAS malloc_areas are placed according to their chunks' sizes. The exceeding malloc_areas can be compacted
	for (i = NUM_AREAS; i < state->num_areas; i++) {
		m_area = &state->areas[i];

		if (m_area->alloc_chunks == 0
		    && m_area->last_access < time_barrier
		    && !CHECK_AREA_LOCK_BIT(m_area)) {

			if (m_area->self_pointer != NULL) {

				//free_lp_memory(lp, m_area->self_pointer);
				rsfree(m_area->self_pointer);

				m_area->use_bitmap = NULL;
				m_area->dirty_bitmap = NULL;
				m_area->self_pointer = NULL;
				m_area->area = NULL;
				m_area->state_changed = 0;

				// Set the pointers
				if (m_area->prev != -1)
					state->areas[m_area->prev].next = m_area->next;
				if (m_area->next != -1)
					state->areas[m_area->next].prev = m_area->prev;

				// Swap, if possible
				if (i < state->num_areas - 1) {
					memcpy(m_area, &state->areas[state->num_areas - 1], sizeof(malloc_area));
					m_area->idx = i;
					if (m_area->prev != -1)
						state->areas[m_area->prev].next = m_area->idx;
					if (m_area->next != -1)
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
