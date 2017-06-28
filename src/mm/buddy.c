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
* @file allocator.c
* @brief
* @author Francesco Quaglia
*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include <mm/dymelor.h>
#include <mm/mm.h>


static struct _buddy **buddies;
// This vector is accessed by the numa.c module to migrate pages
#ifndef HAVE_NUMA
static
#endif
void **mem_areas;
long long *displacements;

static inline int left_child(int idx) {
    return ((idx << 1) + 1);
}

static inline int right_child(int idx) {
    return ((idx << 1) + 2);
}

static inline int parent(int idx) {
    return (((idx + 1) >> 1) - 1);
}

void *get_pages(unsigned int lid){
   //int displacement = displacements[lid]; //get offset to current max memory address allocated by buddy */

  /* int num_pages = displacement/PAGE_SIZE; //page size is the same of buddy granularity */
  /* printf("\nLP[%d] allocated %d pages\n",lid,num_pages); */

  /* void * curr_addr=old_bs; */
  /* void* brk = get_brk(lid); */
  /* while (curr_addr != curr_bs){ */
  /*   curr_addr = (void *)((char *)curr_addr + BUDDY_GRANULARITY); //TODO: save those addresses to be returned. */
  /*   num_pages--;//debugging */
  /*   printf("\n\tDisplaced base address is: %p brk is: %p current page address is: %p remaining pages: %d\n",old_bs,brk,curr_addr,num_pages); */
  /* } */
  size_t node_size;
  void *current_bs;
//  void *final_bs;
  unsigned idx;
  struct _buddy *self;
  int offset;
  void* old_bs;
  int num_pages;

  old_bs = mem_areas[lid];
  self = buddies[lid];
  idx = displacements[lid] + self->size -1;
  node_size = 1;

  for (; idx!=0; idx = parent(idx)) {
      node_size <<= 1;    /* node_size *= 2; */

      if (self->longest[idx] == 0) {
          /* printf("longest[%d] = %zu with size: %zu\n", idx,self->longest[idx],node_size); */

          offset = (idx + 1) * node_size - self->size;
          current_bs = (void*)((char*)old_bs + offset*BUDDY_GRANULARITY);
          num_pages = node_size/PAGE_SIZE +1;
//          final_bs = (void*)((char*)old_bs + offset*BUDDY_GRANULARITY + (node_size/PAGE_SIZE + 1)*BUDDY_GRANULARITY);

          while (num_pages != 0){
            current_bs = (void *)((char *)current_bs + BUDDY_GRANULARITY); //TODO: save those addresses to be returned. */
            /* printf("\n\tCurrent page address is: %p target page address: %p, num of pages %d\n",old_bs,curr_bs,node_size/PAGE_SIZE); */
            /* printf("\t\nallocated pages: %d  with address %p page size is: %d\n",num_pages,old_bs, PAGE_SIZE); */
            //printf("\t\n base pointer is: %p, current address %p  ending address: %p, allocated size: %d, num of pages: %d\n", mem_areas[lid], current_bs, final_bs, node_size,num_pages);
            num_pages--;
          }
     }
  }

  return NULL; // TODO: finish to implement this function

}
/** allocate a new buddy structure
 * @param num_of_fragments number of fragments of the memory to be managed
 * @return pointer to the allocated buddy structure */
static struct _buddy *buddy_new(unsigned int num_of_fragments) {
    struct _buddy *self = NULL;
    size_t node_size;

    int i;

    if (num_of_fragments < 1 || !IS_POWEROF2(num_of_fragments)) {
        return NULL;
    }

    // Alloacte an array to represent a complete binary tree
    self = rsalloc(sizeof(struct _buddy) + 2 * num_of_fragments * sizeof(size_t));
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
    if (self == NULL || self->size < size) {
        return -1;
    }
    size = POWEROF2(size);

    //if(size > 15000000)
    //  printf("Buddy is mapping the request to %d bytes of memory\n", size);
    fflush(stdout);

    unsigned idx = 0;
    if (self->longest[idx] < size) {
    //    printf("NON C'HO MEMORIAAAAA\n");
        fflush(stdout);
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

    //if(size > 15000000)
 //     printf("occupied idx: %u with size: %zu, offset is: %d\n", idx,node_size,offset);
    while (idx) {
        idx = parent(idx);
        self->longest[idx] = max(self->longest[left_child(idx)], self->longest[right_child(idx)]);
    }

    return offset;
}

static void buddy_free(struct _buddy *self, int offset) {
    if (self == NULL || offset < 0 || offset > (int)self->size) {
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

void *pool_get_memory(unsigned int lid, size_t size) {
	long long offset, 
            displacement;
	size_t fragments;

  //printf("Requesting %d bytes of memory\n", size);

	// Get a number of fragments to contain 'size' bytes
	// The operation involves a fast positive integer round up
	fragments = 1 + ((size - 1) / BUDDY_GRANULARITY);
  offset = buddy_alloc(buddies[lid], fragments);
	displacement = offset * BUDDY_GRANULARITY;

  if(offset == -1)
		return NULL;

/*
  void * ptr = (void *)((char *)mem_areas[lid] + displacement);
  printf("(%d)[%d] pool_get_memory returns %p\n", tid, lid, ptr);
  if(ptr < 0x50000000000 || 1) {
    printf("\tsize: %lu\n", size);
    printf("\tfragments: %lu\n", fragments);
    printf("\toffset: %llu\n", offset);
    printf("\tdisplacement: %llu\n", displacement);
    printf("\tdisplacements[%d]: %llu\n", lid, displacements[lid]);
    printf("\tmem_areas[%d]: %p\n", lid, mem_areas[lid]);
  }
*/
  //if(size > 15000000)
  //  abort();

  if(displacements[lid] < offset){
    displacements[lid] = offset;
  }

  return (void *)((char *)mem_areas[lid] + displacement);
}



void pool_release_memory(unsigned int lid, void *ptr) {
	int displacement;

	displacement = (int)((char *)ptr - (char *)mem_areas[lid]);
	buddy_free(buddies[lid], displacement);

  if(displacements[lid] > displacement)
    displacements[lid] = displacement;
}

void free_pages(void *ptr, size_t length) {
	int ret;

	ret = munmap(ptr, length);
	if(ret < 0)
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
	unsigned int i;

	// These are a vector of pointers which are later initialized
	buddies = rsalloc(sizeof(struct _buddy *) * n_prc);
	mem_areas = rsalloc(sizeof(void *) * n_prc);
  displacements = rsalloc(sizeof(long long) *n_prc);
  //printf("Initializing LP memory allocator... ");

  segment_allocator_init(n_prc); //ADDED BY MATTEO.

  for (i = 0; i < n_prc; i++){
    displacements[i] = 0;
		buddies[i] = buddy_new(PER_LP_PREALLOCATED_MEMORY / BUDDY_GRANULARITY);
		mem_areas[i] = get_base_pointer(i);
    if(mem_areas[i] == NULL)
			rootsim_error(true, "Unable to initialize memory for LP %d. Aborting...\n",i);
	}

	//printf("done\n");

#ifdef HAVE_NUMA
	numa_init();
#endif

	//set_BH_map(maps);
	if(!BH_init())
		rootsim_error(true, "Unable to initialize bottom halves buffers\n");

	return true;
}


