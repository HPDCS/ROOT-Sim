/**
* @file mm/buddy.c
*
* @brief
*
* A Buddy System implementation
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
* @author
*/

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include <mm/dymelor.h>
#include <mm/mm.h>
#include <datatypes/bitmap.h>

static inline int left_child(int i)
{
	return ((i << 1) + 1);
}

static inline int right_child(int i)
{
	return ((i << 1) + 2);
}

static inline int parent(int i)
{
	return (((i + 1) >> 1) - 1);
}

static inline bool is_power_of_2(size_t i)
{
	return !(i & (i - 1));
}

static inline size_t next_power_of_2(size_t size)
{
	size--;
	size |= (size >> 1);
	size |= (size >> 2);
	size |= (size >> 4);
	size |= (size >> 8);
	size |= (size >> 16);
	size |= (size >> 32);
	return size + 1;
}


/** allocate a new buddy structure
 * @param lp A pointer to the lp_struct of the LP from whose buddy we are
 *           allocating memory
 * @param requested_size size in bytes of the memory area to manage
 * @return pointer to the allocated buddy structure */
struct buddy *buddy_new(size_t requested_size)
{
	if (!requested_size) {
		return NULL;
	}

	size_t fragments = (requested_size >> BUDDY_BLOCK_SIZE_EXP) + (B_CTZ(requested_size) < BUDDY_BLOCK_SIZE_EXP);
	size_t nodes = 2 * fragments - 1;
	struct buddy *self = rsalloc(sizeof(struct buddy) + nodes * sizeof(size_t));

	spinlock_init(&self->lock);
	self->size = fragments;

	size_t i, node_size = fragments << 1; // little hack to have a correct iteration at i == 0
	for (i = 0; i < nodes; i++) {
		if (is_power_of_2(i + 1)) {
			node_size >>= 1;
		}
		self->longest[i] = node_size;
	}
	return self;
}

void buddy_destroy(struct buddy *self)
{
	rsfree(self);
}

/** allocate *size* from a buddy system *self*
 * @return the offset from the beginning of memory to be managed */
static size_t buddy_alloc(struct buddy *self, size_t requested_blocks)
{
	// some sanity checks are run in the parent function allocate_buddy_memory()
	requested_blocks = next_power_of_2(requested_blocks);
	spin_lock(&self->lock);

	if (self->longest[0] < requested_blocks) {
		spin_unlock(&self->lock);
		return SIZE_MAX;
	}

	/* search recursively for the child */
	int i = 0;
	size_t node_size;
	for (node_size = self->size; node_size > requested_blocks; node_size >>= 1) {
		/* choose the child with smaller longest value which is still large at least *size* */
		if (self->longest[left_child(i)] >= requested_blocks) {
			i = left_child(i);
		} else {
			i = right_child(i);
		}
	}

	/* update the *longest* value back */
	self->longest[i] = 0;

	size_t offset = (i + 1) * node_size - self->size;

	while (i) {
		i = parent(i);
		self->longest[i] = max(self->longest[left_child(i)], self->longest[right_child(i)]);
	}
	spin_unlock(&self->lock);
	return offset;
}

static void buddy_free(struct buddy *self, size_t offset)
{
	// some sanity checks are run in the parent function free_buddy_memory()
	size_t node_size = 1;
	int i = offset + self->size - 1;

	spin_lock(&self->lock);

	for (; i && self->longest[i]; i = parent(i)) {
		node_size <<= 1;
	}

	self->longest[i] = node_size;

	while (i) {
		i = parent(i);
		node_size <<= 1;

		size_t left_longest = self->longest[left_child(i)];
		size_t right_longest = self->longest[right_child(i)];

		if (left_longest + right_longest == node_size) {
			self->longest[i] = node_size;
		} else {
			self->longest[i] = max(left_longest, right_longest);
		}
	}
	spin_unlock(&self->lock);
}

void* allocate_buddy_memory(struct buddy* self, void* base_mem, size_t requested_size)
{
	if (unlikely(!requested_size || self == NULL))
		return NULL;

	size_t requested_blocks = (requested_size >> BUDDY_BLOCK_SIZE_EXP) + (B_CTZ(requested_size) < BUDDY_BLOCK_SIZE_EXP);
	if (unlikely(requested_blocks > self->size))
		return NULL;

	size_t offset = buddy_alloc(self, requested_blocks);
	if(unlikely(offset == SIZE_MAX))
		return NULL;

	return (char *)base_mem + (offset << BUDDY_BLOCK_SIZE_EXP);
}

void free_buddy_memory(struct buddy *self, void *base_mem, void *ptr)
{
	if (unlikely(self == NULL))
		return;

	size_t offset = ((char *)ptr - (char *)base_mem) >> BUDDY_BLOCK_SIZE_EXP;
	if(unlikely(offset >= self->size))
		return;

	buddy_free(self, offset);
}
