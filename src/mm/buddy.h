/**
* @file mm/buddy.h
*
* @brief Buddy System
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
* @author Andrea Piccione
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date December 18, 2019
*/

#pragma once

#include <core/core.h>

// buddy block size expressed in 2^n, e.g.: BUDDY_BLOCK_SIZE_EXP = 4, block_size = 16 TODO make this the pagesize
#define BUDDY_BLOCK_SIZE_EXP 12

struct buddy {
	spinlock_t lock;
	size_t size;
	size_t longest[] __attribute__((aligned(sizeof(size_t)))); // an array based binary tree
};

extern struct buddy *buddy_new(size_t requested_size);
extern void buddy_destroy(struct buddy *self);
extern void *allocate_buddy_memory(struct buddy *self, void *base_mem, size_t requested_size);
extern void free_buddy_memory(struct buddy *self, void *base_mem, void *ptr);

