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
* @file lp-alloc.c
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

	void *ret = NULL;


	return ret;
}


void __ufree(unsigned int lid, void *ptr) {
	(void)ptr;

	// TODO: so far, free does not really frees the LP memory :(
	// This is not a great problem, as a lot of stuff must happen before DyMeLoR
	// really calls free, and this is unlikely to happen often...

}


void *__urealloc(unsigned int lid, void *ptr, size_t new_s) {
	(void)ptr;
	(void)new_s;

	// TODO: so far, realloc is not implemented. This is a problem with certain
	// models which are very memory-greedy. This will be implemented soon...

	return NULL;
}


void *__ucalloc(unsigned int lid, size_t nmemb, size_t size) {

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
