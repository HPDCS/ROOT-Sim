/**
* @file mm/memtrace.c
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

#include <mm/dymelor.h>
#include <scheduler/scheduler.h>

#ifdef HAS_GCC_PLUGIN

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
__attribute__((used))
__attribute__((no_caller_saved_registers))
void __write_mem(unsigned char *address, size_t size)
{
	struct malloc_area *m_area = NULL, *cur_m_area;
	struct malloc_state *m_state;
	size_t bitmap_size, chk_size;
	int i, first_chunk, last_chunk;

	assert(current != NULL);

	switch_to_platform_mode();

	printf("(%d) Intercepting a write of size %ld\n", current->gid.to_int, size);

	// Find the correct malloc area. It could be that the write operation
	// falls outside of malloc areas, because we here intercept also
	// writes on local variables. In that case, we have to
	// return as fast as possible to the model.

	m_state = current->mm->m_state;

	for (i = 0; i < m_state->num_areas; i++) {
		cur_m_area = &m_state->areas[i];
		if(cur_m_area == NULL || cur_m_area->area == NULL)
			continue;

		printf("(%d) Comparing %p with area %d [%p, %p]\n", current->gid.to_int, address, cur_m_area->idx, cur_m_area->area, (unsigned char *)cur_m_area->area + cur_m_area->chunk_size * cur_m_area->num_chunks);
		fflush(stdout);

		if (address >= (unsigned char *)cur_m_area->area && address < (unsigned char *)cur_m_area->area + cur_m_area->chunk_size * cur_m_area->num_chunks) {
			printf("(%d) Found a write on a malloc area %d\n", current->gid.to_int, cur_m_area->idx);
			fflush(stdout);
			m_area = cur_m_area;
			break;
		}
	}

	if (m_area == NULL)
		goto out;

	// Determine the number of chunks affected by the write operation
	chk_size = UNTAGGED_CHUNK_SIZE(m_area);
	first_chunk = (int)(((char *)address - (char *)m_area->area) / chk_size);

	printf("(%d) The found malloc area handles chunks of size %ld\n", current->gid.to_int, chk_size);

	// If size == -1, then we adopt a conservative approach: dirty all the chunks from the base to the end
	// of the actual malloc area base address belongs to.
	// This has been inserted to support the wrapping of third-party libraries where the size of the
	// update (or even the actual update) cannot be statically/dynamically determined.
	if (size == SIZE_MAX)
		last_chunk = m_area->num_chunks - 1;
	else
		last_chunk = (int)(((char *)address + size - 1 - (char *)m_area->area) / chk_size);

	printf("(%d) Dirtied chunks: from %d to %d\n", current->gid.to_int, first_chunk, last_chunk);

	bitmap_size = bitmap_required_size(m_area->num_chunks);

	if (m_area->state_changed == 1) {
		if (m_area->dirty_chunks == 0)
			current->mm->m_state->dirty_bitmap_size += bitmap_size;
	} else {
		current->mm->m_state->dirty_areas++;
		current->mm->m_state->dirty_bitmap_size += bitmap_size * 2;
		m_area->state_changed = 1;
	}

	for (i = first_chunk; i <= last_chunk; i++) {

		// If it is dirtied a clean chunk, set it dirty and increase dirty object count for the malloc_area
		if (!bitmap_check(m_area->dirty_bitmap, i)) {
			bitmap_set(m_area->dirty_bitmap, i);
			current->mm->m_state->total_inc_size += chk_size;
				m_area->dirty_chunks++;
		}
	}


    out:
	switch_to_application_mode();
	return;
}

#endif
