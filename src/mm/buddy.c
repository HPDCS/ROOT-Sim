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
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include <core/core.h>
#include <mm/mm.h>
#include <scheduler/process.h>

static inline int left_child(int index)
{
	/* index * 2 + 1 */
	return ((index << 1) + 1);
}

static inline int right_child(int index)
{
	/* index * 2 + 2 */
	return ((index << 1) + 2);
}

static inline int parent(int index)
{
	/* (index+1)/2 - 1 */
	return (((index + 1) >> 1) - 1);
}

static inline bool is_power_of_2(int index)
{
	return !(index & (index - 1));
}

static inline size_t next_power_of_2(size_t size)
{
	/* depend on the fact that size < 2^32 */
	size |= (size >> 1);
	size |= (size >> 2);
	size |= (size >> 4);
	size |= (size >> 8);
	size |= (size >> 16);
	return size + 1;
}

/** allocate a new buddy structure
 * @param lp A pointer to the lp_struct of the LP from whose buddy we are
 *           allocating memory
 * @param num_of_fragments number of fragments of the memory to be managed
 * @return pointer to the allocated buddy structure */
struct buddy *buddy_new(struct lp_struct *lp, size_t num_of_fragments)
{
	struct buddy *self = NULL;
	size_t node_size;
	int i;

	if (num_of_fragments < 1 || !is_power_of_2(num_of_fragments)) {
		return NULL;
	}

	/* alloacte an array to represent a complete binary tree */
	(void)lp;
	//self = (struct buddy *)get_segment_memory(lp, sizeof(struct buddy) + 2 * num_of_fragments * sizeof(size_t));
	self = (struct buddy *)rsalloc(sizeof(struct buddy) + 2 * num_of_fragments * sizeof(size_t));
	bzero(self, sizeof(struct buddy) + 2 * num_of_fragments * sizeof(size_t));	// unnecessary, it is later initialized

	self->size = num_of_fragments;
	node_size = num_of_fragments * 2;

	/* initialize *longest* array for buddy structure */
	int iter_end = num_of_fragments * 2 - 1;
	for (i = 0; i < iter_end; i++) {
		if (is_power_of_2(i + 1)) {
			node_size >>= 1;
		}
		self->longest[i] = node_size;
	}

	spinlock_init(&self->lock);

	return self;
}

void buddy_destroy(struct buddy *self)
{
	rsfree(self);
}

/** allocate *size* from a buddy system *self*
 * @return the offset from the beginning of memory to be managed */
int buddy_alloc(struct buddy *self, size_t size)
{
	if (self == NULL || self->size <= size) {
		return -1;
	}

	size = next_power_of_2(size);

	size_t index = 0;
	if (self->longest[index] < size) {
		return -1;
	}

	/* search recursively for the child */
	size_t node_size = 0;
	for (node_size = self->size; node_size != size; node_size >>= 1) {
		/* choose the child with smaller longest value which is still larger
		 * than *size* */
		if (self->longest[left_child(index)] >= size) {
			index = left_child(index);
		} else {
			index = right_child(index);
		}
	}

	/* update the *longest* value back */
	self->longest[index] = 0;
	int offset = (index + 1) * node_size - self->size;

	while (index) {
		index = parent(index);
		self->longest[index] = max(self->longest[left_child(index)], self->longest[right_child(index)]);
	}

	return offset;
}

void buddy_free(struct buddy *self, size_t offset)
{
	if (self == NULL || offset >= self->size) {
		return;
	}

	size_t node_size;
	size_t index;

	/* get the corresponding index from offset */
	node_size = 1;
	index = offset + self->size - 1;

	for (; self->longest[index] != 0; index = parent(index)) {
		node_size <<= 1;

		if (index == 0) {
			break;
		}
	}

	self->longest[index] = node_size;

	while (index) {
		index = parent(index);
		node_size <<= 1;

		size_t left_longest = self->longest[left_child(index)];
		size_t right_longest = self->longest[right_child(index)];

		if (left_longest + right_longest == node_size) {
			self->longest[index] = node_size;
		} else {
			self->longest[index] = max(left_longest, right_longest);
		}
	}
}

void *allocate_lp_memory(struct lp_struct *lp, size_t size)
{
	long long offset, displacement;
	size_t fragments;

	if (size == 0)
		return NULL;

	// Get a number of fragments to contain 'size' bytes
	// The operation involves a fast positive integer round up.
	// The buddy can be accessed by multiple threads, so lock it
	fragments = 1 + ((size - 1) / BUDDY_GRANULARITY);
	spin_lock(&lp->mm->buddy->lock);
	offset = buddy_alloc(lp->mm->buddy, fragments);
	spin_unlock(&lp->mm->buddy->lock);
	displacement = offset * BUDDY_GRANULARITY;

	if (unlikely(offset == -1))
		return NULL;

	return (void *)((char *)lp->mm->segment->base + displacement);
}

void free_lp_memory(struct lp_struct *lp, void *ptr)
{
	size_t displacement;

	displacement = (int)((char *)ptr - (char *)lp->mm->segment->base);
	spin_lock(&lp->mm->buddy->lock);
	buddy_free(lp->mm->buddy, displacement);
	spin_unlock(&lp->mm->buddy->lock);
}
