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
#include <mm/allocator.h>
#include <mm/bh.h>
#include <mm/numa.h>


#ifdef HAVE_PARALLEL_ALLOCATOR

static spinlock_t segment_lock;

//mem_map maps[MAX_LPs];
//int handled_sobjs = -1;
static struct _buddy **buddies;
static void **mem_areas;

static inline int left_child(int idx) {
    return ((idx << 1) + 1); 
}

static inline int right_child(int idx) {
    return ((idx << 1) + 2);
}

static inline int parent(int idx) {
    return (((idx+1)>>1) - 1);
}

static inline bool is_power_of_2(int idx) {
    return !(idx & (idx - 1));
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
static int buddy_alloc(struct _buddy *self, size_t size) {
    if (self == NULL || self->size < size) {
        return -1;
    }
    size = POWEROF2(size);

    unsigned idx = 0;
    if (self->longest[idx] < size) {
        return -1;
    }

    /* search recursively for the child */
    unsigned node_size = 0;
    for (node_size = self->size; node_size != size; node_size >>= 1) {
        /* choose the child with smaller longest value which is still larger
         * than *size* */
        /* TODO */
        idx = choose_better_child(self, idx, size);
    }

    /* update the *longest* value back */
    self->longest[idx] = 0;
    int offset = (idx + 1)*node_size - self->size;

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














/*
void* allocate_segment(unsigned int sobj, size_t size) {

	mdt_entry* mdt;
	char* segment;
	int numpages;
	int ret;

	#ifdef NEW_ALLOCATOR
	static void *my_initial_address = (void *)(180L * 256L * 256L * 256L * PAGE_SIZE);
	#endif

	if( ((int)sobj >= handled_sobjs) )
		goto bad_allocate; 

	if(size <= 0)
		goto bad_allocate;

	numpages = (int)(size / (int)(PAGE_SIZE));

	if (size % PAGE_SIZE) numpages++;

	AUDIT
	printf("segment allocation - requested numpages is %d\n",numpages);

	if(numpages > MAX_SEGMENT_SIZE)
		goto bad_allocate;

	#ifdef HAVE_NUMA
	ret = lock(sobj);
	if(ret == FAILURE)
		goto bad_allocate;
	#endif

	mdt = get_new_mdt_entry(sobj);
	if (mdt == NULL) {
		goto bad_allocate;
	}

	AUDIT
	printf("segment allocation - returned mdt is at address %p\n",mdt);


	AUDIT
	printf("allocate segment: request for %ld bytes - actual allocation is of %d pages\n",size,numpages);


	#ifndef NEW_ALLOCATOR

	segment = (char*)mmap((void*)NULL,PAGE_SIZE*numpages, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	#else


	spin_lock(&segment_lock);

	// Update my_initial_address, keeping it aligned to large pages
	my_initial_address = (void *)((char *)my_initial_address - ( PAGE_SIZE * (int)( ceil((double)numpages / 256.0) * 256  )  ));

        segment = (char*)mmap(my_initial_address, PAGE_SIZE*numpages, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	spin_unlock(&segment_lock);

	if(segment == MAP_FAILED)
		abort();


	#endif

	AUDIT
	printf("allocate segment: actual allocation is at address %p\n",segment);

	if (segment == MAP_FAILED) {
		release_mdt_entry(sobj);
		goto bad_allocate;
	}

	mdt->addr = segment;
	mdt->numpages = numpages;

	AUDIT	
	audit_map(sobj);

	#ifdef HAVE_NUMA
	unlock(sobj);
	#endif

	return segment;

bad_allocate:
	#ifdef HAVE_NUMA
	unlock(sobj);
	#endif
	
	return NULL;

}
*/
/*
char* allocate_page(void) {
	return allocate_pages(1);
}
*/
/*
char* allocate_mdt(void) {

        char* page;

        page = allocate_pages(MDT_PAGES);

	return page;
}
*/
/*

static mdt_entry *get_new_mdt_entry(int sobj){
	
	mem_map* m_map;
	mdt_entry* mdte;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ) return NULL; 

	m_map = &maps[sobj]; 

	if (m_map->active >= m_map->size){
		goto bad_new_mdt_entry;
	}

	m_map->active += 1;

	mdte = (mdt_entry*)m_map->base + m_map->active - 1 ;

	return mdte;
	
    bad_new_mdt_entry:
	return NULL;
}

int release_mdt_entry(int sobj){
	mem_map* m_map;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ) return MDT_RELEASE_FAILURE; 

	m_map = &maps[sobj]; 

	if (m_map->active <= 0){
		goto bad_mdt_release;
	}

	m_map->active -= 1;

	return SUCCESS; 

    bad_mdt_release:

	return MDT_RELEASE_FAILURE;
}

*/


void *pool_get_memory(unsigned int lid, size_t size) {
	//return allocate_segment(lid, size);

	int displacement;
	displacement = buddy_alloc(buddies[lid], size);
	
	if(displacement == -1)
		return NULL;
	
	return (void *)((char *)mem_areas[lid] + displacement);
}


void pool_release_memory(unsigned int lid, void *ptr) {
	int displacement;
	
	displacement = (int)((char *)ptr - (char *)mem_areas[lid]);
	buddy_free(buddies[lid], displacement);
}


void allocator_fini(void) {
	unsigned int i;
	for (i = 0; i < n_prc; i++) {
		buddy_destroy(buddies[i]);
		//free_pages(mem_areas[i]);
	}
	
	rsfree(mem_areas);
	rsfree(buddies);
}

bool allocator_init(void) {
	unsigned int i;
//	char* addr;

//	if( (sobjs > MAX_LPs) )
//		return INVALID_SOBJS_COUNT; 

	//~handled_sobjs = sobjs;
	
	// These are a vector of pointers which are later initialized
	buddies = rsalloc(sizeof(struct _buddy *) * n_prc);
	mem_areas = rsalloc(sizeof(void *) * n_prc);

	for (i = 0; i < n_prc; i++){
		buddies[i] = buddy_new(TOTAL_MEMORY / BUDDY_GRANULARITY);
		mem_areas[i] = allocate_pages(TOTAL_MEMORY / PAGE_SIZE);
/*		addr = allocate_mdt();
		if (addr == NULL) goto bad_init;
		maps[i].base = addr;
		maps[i].active = 0;
		maps[i].size = MDT_ENTRIES;
		AUDIT
		printf("INIT: sobj %d - base address is %p - active are %d - MDT size is %d\n",i,maps[i].base, maps[i].active, maps[i].size);
*/	}
	
#ifdef HAVE_NUMA
	set_daemon_maps(maps, moves);
	init_move();
#endif

	//set_BH_map(maps);
	if(!BH_init())
		rootsim_error(true, "Unable to initialize bottom halves buffers\n");

#ifdef HAVE_NUMA
	setup_numa_nodes();
#endif
	spinlock_init(&segment_lock);
	return true;
}

#endif /* HAVE_PARALLEL_ALLOCATOR */




char *allocate_pages(int num_pages) {
	
        char *page;

        page = (char*)mmap((void*)NULL, num_pages * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	if (page == MAP_FAILED) {
		page = NULL;
	}

	return page;
}

