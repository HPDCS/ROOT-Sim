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
* @file vma.c
* @brief Virtual Memory Allocator
* @author Alessandro Pellegrini
*/

#include <stdlib.h>

#include <mm/mm.h>

char *allocate_pages(int num_pages) {

        char *page;

        page = (char*)mmap((void*)NULL, num_pages * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	if (page == MAP_FAILED) {
		page = NULL;
	}

	return page;
}

void free_pages(void *ptr, size_t length) {
	int ret;

	ret = munmap(ptr, length);
	if(ret < 0)
		perror("free_pages(): unable to deallocate memory");
}
