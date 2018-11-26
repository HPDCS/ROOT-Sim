/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
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
* @file buddy.c
* @brief
* @author Francesco Quaglia
*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include <core/core.h>
#include <mm/dymelor.h>
#include <mm/mm.h>
#include <scheduler/process.h>


// TODO: move into struct lp_struct
static struct _buddy **buddies;
static void **mem_areas;

static inline int left_child(int idx) {
    return ((idx << 1) + 1);
}

static inline int right_child(int idx) {
    return ((idx << 1) + 2);
}

static inline int parent(int idx) {
    return (((idx + 1) >> 1) - 1);
}

/** allocate a new buddy structure
 * @param num_of_fragments number of fragments of the memory to be managed
 * @return pointer to the allocated buddy structure */
static struct _buddy *buddy_new(unsigned int num_of_fragments) {
    struct _buddy *self = NULL;
    size_t node_size;

    int i;

    if (unlikely(num_of_fragments < 1 || !IS_POWEROF2(num_of_fragments))) {
        return NULL;
    }

    // Alloacte an array to represent a complete binary tree
    self = rsalloc(sizeof(struct _buddy) + 2 * num_of_fragments * sizeof(size_t));
	bzero(self, sizeof(struct _buddy) + 2 * num_of_fragments * sizeof(size_t));
    self->size = num_of_fragments;
    node_size = num_of_fragments * 2;

    // initialize *longest* array for buddy structure
    int iter_end = num_of_fragments * 2 - 1;
    for (i = 0; i < iter_end; i++) {
        if (IS_POWEROF2(i + 1)) {
            node_size >>= 1;
        }
        self->longest[i] = node_size;
    }

    return self;
}

static void buddy_destroy(struct _buddy *self) {
    rsfree(self);
}

/* choose the child with smaller longest value which is still larger
 * than *size* */
static unsigned choose_better_child(struct _buddy *self, unsigned idx, size_t size) {

    struct compound {
        size_t size;
        unsigned idx;
    } children[2];

    children[0].idx = left_child(idx);
    children[0].size = self->longest[children[0].idx];
    children[1].idx = right_child(idx);
    children[1].size = self->longest[children[1].idx];

    int min_idx = (children[0].size <= children[1].size) ? 0: 1;

    if (size > children[min_idx].size) {
        min_idx = 1 - min_idx;
    }

    return children[min_idx].idx;
}

/** allocate *size* from a buddy system *self*
 * @return the offset from the beginning of memory to be managed */
static long long buddy_alloc(struct _buddy *self, size_t size) {

    if (unlikely(self == NULL || self->size < size)) {
	rootsim_error(true,"size is %u < %u\n",self->size,size);
        return -1;
    }
    size = POWEROF2(size);

    unsigned idx = 0;
    if (unlikely(self->longest[idx] < size)) {
		rootsim_error(true,"self->longest %u < %u\n",self->longest[idx],size);
        return -1;
    }

    /* search recursively for the child */
    unsigned node_size = 0;
    for (node_size = self->size; node_size != size; node_size >>= 1) {
        idx = choose_better_child(self, idx, size);
    }

    /* update the *longest* value back */
    self->longest[idx] = 0;
    int offset = (idx + 1) * node_size - self->size;

    while (idx) {
        idx = parent(idx);
        self->longest[idx] = max(self->longest[left_child(idx)], self->longest[right_child(idx)]);
    }

    return offset;
}

static void buddy_free(struct _buddy *self, int offset) {
    if (unlikely(self == NULL || offset < 0 || offset > (int)self->size)) {
        return;
    }

    size_t node_size;
    unsigned idx;

    /* get the corresponding idx from offset */
    node_size = 1;
    idx = offset + self->size - 1;

    for (; self->longest[idx] != 0; idx = parent(idx)) {
        node_size <<= 1;    /* node_size *= 2; */

        if (idx == 0) {
            break;
        }
    }
    self->longest[idx] = node_size;

    while (idx) {
        idx = parent(idx);
        node_size <<= 1;

        size_t left_longest = self->longest[left_child(idx)];
        size_t right_longest = self->longest[right_child(idx)];

        if (left_longest + right_longest == node_size) {
            self->longest[idx] = node_size;
        } else {
            self->longest[idx] = max(left_longest, right_longest);
        }
    }
}

void *pool_get_memory(struct lp_struct *lp, size_t size) {
	long long offset,
		displacement;
	size_t fragments;

	// Get a number of fragments to contain 'size' bytes
	// The operation involves a fast positive integer round up
	fragments = 1 + ((size - 1) / BUDDY_GRANULARITY);
	offset = buddy_alloc(buddies[lp->lid.to_int], fragments);
	displacement = offset * BUDDY_GRANULARITY;

	if(unlikely(offset == -1))
		return NULL;

	return (void *)((char *)mem_areas[lp->lid.to_int] + displacement);
}



void pool_release_memory(struct lp_struct *lp, void *ptr) {
	int displacement;

	displacement = (int)((char *)ptr - (char *)mem_areas[lp->lid.to_int]);
	buddy_free(buddies[lp->lid.to_int], displacement);

}

void free_pages(void *ptr, size_t length) {
	int ret;

	ret = munmap(ptr, length);
	if(unlikely(ret < 0))
		perror("free_pages(): unable to deallocate memory");
}


void allocator_fini(void) {
	unsigned int i;
	for (i = 0; i < n_prc; i++) {
		
		buddy_destroy(buddies[i]);
		free_pages(mem_areas[i], PER_LP_PREALLOCATED_MEMORY / PAGE_SIZE);
//    segment_allocator_fini(i);
	}

	rsfree(mem_areas);
	rsfree(buddies);

}

bool allocator_init(void) {
	// These are a vector of pointers which are later initialized
	buddies = rsalloc(sizeof(struct _buddy *) * n_prc);
	mem_areas = rsalloc(sizeof(void *) * n_prc);

	// we loop over all gid's to let the underlying kernel module
	// mmap memory for all distributed LPs
	foreach_lp(lp) {
		if(find_kernel_by_gid(lp->gid) == kid) {
			// TODO: we should tread mem_areas as gid's as well, to
			// reclaim memory at the end of the simulation.
			mem_areas[lp->lid.to_int] = get_segment(lp->gid);
			buddies[lp->lid.to_int] = buddy_new(PER_LP_PREALLOCATED_MEMORY / BUDDY_GRANULARITY);
			continue;
		} else {
			(void)get_segment(lp->gid);
		}
	}

	return true;
}
