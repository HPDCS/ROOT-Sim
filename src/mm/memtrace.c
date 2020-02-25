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

#include <stdlib.h>

#include <mm/dymelor.h>
#include <mm/mm.h>
#include <scheduler/scheduler.h>

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
void __write_mem(void *address, size_t size)
{
	void *stack = __builtin_frame_address(0);

	malloc_area *m_area;
	malloc_state *m_state;
	size_t bitmap_size, chk_size;
	int chunk;

	if(address > stack)
		return;

	assert(current != NULL);

	switch_to_platform_mode();

	m_state = current->mm->m_state;
	m_area = malloc_area_get(address, &chunk);

	chk_size = UNTAGGED_CHUNK_SIZE(m_area);
	bitmap_size = bitmap_required_size(m_area->num_chunks);

	if (m_area->state_changed == 1) {
		if (m_area->dirty_chunks == 0)
			m_state->total_inc_size += bitmap_size;
	} else {
		m_state->total_inc_size += sizeof(malloc_area) + bitmap_size * 2;
		m_area->state_changed = 1;
	}

	if (!bitmap_check(m_area->dirty_bitmap, chunk)) {
		bitmap_set(m_area->dirty_bitmap, chunk);
		m_state->total_inc_size += chk_size;
		m_area->dirty_chunks++;
	}
	switch_to_application_mode();
	return;
}

