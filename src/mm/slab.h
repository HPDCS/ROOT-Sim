#pragma once

#include <math.h>
#include <string.h>

#include <core/core.h>
#include <mm/state.h>
#include <core/timer.h>

/**************************************
 * DyMeLoR definitions and structures *
 **************************************/

//ADDED BY MAT 0x00000200000000000
#define LP_PREALLOCATION_INITIAL_ADDRESS	(void *)0x0000008000000000

// This macro is used to retrieve a cache line in O(1)
#define GET_CACHE_LINE_NUMBER(P) ((unsigned long)((P >> 4) & (CACHE_SIZE - 1)))

#define POWEROF2(x) (1UL << (1 + (63 - __builtin_clzl((x) - 1))))
#define IS_POWEROF2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

#define PER_LP_PREALLOCATED_MEMORY (262144L * PAGE_SIZE)	// This should be power of 2 multiplied by a page size. This is 1GB per LP.
#define BUDDY_GRANULARITY PAGE_SIZE	// This is the smallest chunk released by the buddy in bytes. PER_LP_PREALLOCATED_MEMORY/BUDDY_GRANULARITY must be integer and a power of 2

extern size_t __page_size;
#define PAGE_SIZE ({ \
			if(unlikely(__page_size == 0))\
				__page_size = getpagesize();\
			__page_size;\
		  })

struct slab_header {
#ifndef NDEBUG
	atomic_t presence;
#endif
	struct slab_header *prev, *next;
	uint64_t slots;
	uintptr_t refcount;
	struct slab_header *page;
	uint8_t data[] __attribute__((aligned(sizeof(void *))));
};

struct slab_chain {
	spinlock_t lock;
	size_t itemsize, itemcount;
	size_t slabsize, pages_per_alloc;
	uint64_t initial_slotmask, empty_slotmask;
	uintptr_t alignment_mask;
	struct slab_header *partial, *empty, *full;
};
