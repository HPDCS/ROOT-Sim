/**
*                       Copyright (C) 2008-2018 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file platform.c
* @brief This is the ROOT-Sim implementation of the malloc library (to come...)
* @author Alessandro Pellegrini
*/

#include <stddef.h>
#include <stdlib.h>

#include <mm/dymelor.h>
#include <core/core.h>


extern void *__real_malloc(size_t);
extern void __real_free(void *);
extern void *__real_realloc(void *, size_t);
extern void *__real_calloc(size_t, size_t);


inline void *rsalloc(size_t size) {
	void *mem_block = __real_malloc(size);
	if(unlikely(mem_block == NULL)) {
		rootsim_error(true, "Error in Memory Allocation, aborting...");
	}
	return mem_block;
}


inline void rsfree(void *ptr) {
	__real_free(ptr);
}


inline void *rsrealloc(void *ptr, size_t size) {
	return __real_realloc(ptr, size);
}


inline void *rscalloc(size_t nmemb, size_t size) {
	return __real_calloc(nmemb, size);
}

