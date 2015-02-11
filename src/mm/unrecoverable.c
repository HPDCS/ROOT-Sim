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
* @file unrecoverable.c
* @brief LP's memory pre-allocator. This layer stands below DyMeLoR, and is the
* 		connection point to the Linux Kernel Module for Memory Management, when
* 		activated.
* @author Alessandro Pellegrini
*/

#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(OS_LINUX)
#include <stropts.h>
#endif


#include <core/core.h>
#include <mm/dymelor.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <arch/ult.h>


/// Unrecoverable memory state for LPs
static malloc_state **unrecoverable_state;


void unrecoverable_init(void) {
	unsigned int i;

	unrecoverable_state = rsalloc(sizeof(malloc_state *) * n_prc);

	for(i = 0; i < n_prc; i++){
		unrecoverable_state[i] = rsalloc(sizeof(malloc_state));
		if(unrecoverable_state[i] == NULL)
			rootsim_error(true, "Unable to allocate memory on malloc init");

		malloc_state_init(false, unrecoverable_state[i]);
	}
}


void unrecoverable_fini(void) {
	unsigned int i, j;
	malloc_area *current_area;
	
	for(i = 0; i < n_prc; i++) {
		for (j = 0; j < (unsigned int)unrecoverable_state[i]->num_areas; j++) {
			current_area = &(unrecoverable_state[i]->areas[j]);
			if (current_area != NULL) {
				if (current_area->use_bitmap != NULL) {
					ufree(current_area->use_bitmap);
				}
			}
		}
		rsfree(unrecoverable_state[i]->areas);
		rsfree(unrecoverable_state[i]);
	}
	rsfree(unrecoverable_state);
}



void *__umalloc(unsigned int lid, size_t s) {
	return do_malloc(unrecoverable_state[lid], s);
}


void __ufree(unsigned int lid, void *ptr) {
	do_free(unrecoverable_state[lid], ptr);
}


void *__urealloc(unsigned int lid, void *ptr, size_t new_s) {
	
	void *new_buffer;
	size_t old_size;
	malloc_area *m_area;
	
	// If ptr is NULL realloc is equivalent to the malloc
	if (ptr == NULL) {
		return __umalloc(lid, new_s);
	}

	// If ptr is not NULL and the size is 0 realloc is equivalent to the free
	if (new_s == 0) {
		__ufree(lid, ptr);
		return NULL;
	}

	m_area = get_area(ptr);

	// The size could be greater than the real request, but it does not matter since the realloc specific requires that
	// is copied at least the smaller buffer size between the new and the old one
	old_size = m_area->chunk_size;

	new_buffer = __umalloc(lid, new_s);

	if (new_buffer == NULL)
		return NULL;

	memcpy(new_buffer, ptr, new_s > old_size ? new_s : old_size);
	__ufree(lid, ptr);

	return new_buffer;
}


void *__ucalloc(unsigned int lid, size_t nmemb, size_t size) {
	void *buffer;

	if (nmemb == 0 || size == 0)
		return NULL;

	buffer = __umalloc(lid, nmemb * size);
	if (buffer == NULL)
		return NULL;

	bzero(buffer, nmemb * size);

	return buffer;
}
