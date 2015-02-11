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
* @file pool.c
* @brief Manager of the pool of memory blocks
* @author Alessandro Pellegrini
*/

#include <mm/dymelor.h>

extern void *__real_malloc(size_t);
extern void __real_free(void *);

void *pool_get_memory(unsigned int lid, size_t size) {
	return __real_malloc(size);
}

void pool_release_memory(unsigned int lid, void *ptr) {
	__real_free(ptr);
}
