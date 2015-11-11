#ifdef HAVE_REVERSE

#pragma once
#ifndef __REVERSE_H
#define __REVERSE_H

#include <sys/types.h>

//#include <mm/dymelor.h>


#define REVWIN_SIZE 1024 * 4	//! Defalut size of the reverse window which will contain the reverse code
#define REVWIN_CODE_SIZE (REVWIN_SIZE - sizeof(revwin_t))
#define REVWIN_STACK_SIZE 1024	//! Default size of the emultated reverse stack window on the heap space
#define REVWIN_RZONE_SIZE 100		//! Size of the red zone in the reverse window that will be skipped to prevent cache misses
#define RANDOMIZE_REVWIN 0 			//! Activate the rendomization of the addresses used by revwin to prevent cache misses

#define HMAP_SIZE		32768		//! Default size ot the address hash map to handle colliding mov addresses
#define HMAP_INDEX_MASK		0xffffffc0	//! Most significant 10 bits are used to index quad-word which contains address bit
#define HMAP_OFFSET_MASK	0x3f	//! Least significant 6 bits are used to intercept address presence
#define HMAP_OFF_MASK_SIZE	6

#define PAGE_SIZE 4096

//#define current_win map->map[tid]


#define CLUSTER_SIZE 64
#define ADDRESS_PREFIX -CLUSTER_SIZE
#define PREFIX_HEAD_SIZE 1024

#define REVERSE_THRESHOLD 0.15

#define STRATEGY_SINGLE 0
#define STRATEGY_CHUNK 1


/**
 * Descriptor which keeps track of the current
 * reversing strategy in use.
 */
typedef struct strategy_t {
	int current;
	int switches;
} strategy_t;



/**
 * Descriptor of a single address cluster cache line. This entry tells
 * which address it refers to and other possible interesting meta-data
 * relative it for the reverse module.
 */
typedef struct _cache_entry {
	unsigned long long address;		//! Actual address
	unsigned int touches;			//! Number of times this address has been touched so far
} cache_entry;


/**
 * A single cluster cache entry which handles a chunk of 'CLUSTER_SIZE'
 * prefixed addresses. This descriptor is structured as a hash-indexed
 * vector of cache entries, each one containing metad-ata relative to
 * a single address.
 * The prefix_head also keeps track of the internal fragmentation, used
 * to choose the reversing strategy.
 */
typedef struct _prefix_head {
	unsigned long long prefix;			//! Base address prefix of this cluster head
	cache_entry cache[CLUSTER_SIZE];	//! List of registered addresses with current prefix
	unsigned int count;					//! Number of total address in this cluster
	double fragmentation;				//! Internal fragmentation of addresses in the cluster, i.e. how much they are sparse
	unsigned int contiguous;			//! Maximum number of contiguous addresses registered
} prefix_head;


/**
 * Main descriptor of the cluster cache. This maintains the pointer
 * to a linked list of clusters of addresses, i.e. the prefix_head,
 * in order to collect infomation about memory reversing for each
 * address cluster of the specified size.
 */
typedef struct _revwin_cache {
	prefix_head cluster[PREFIX_HEAD_SIZE];
	unsigned int count;
} revwin_cache;



/**
 * Descriptor of a single reverse window
 */
 typedef struct _revwin_t {
	void *top;				//! Pointer to the first instruction byte to be executed
	void *base;				//! Pointer to the logic base of the window
	unsigned int offset;	//! A random offset used to prevent cache lines to be alligned
	size_t size;			//! The actual size of the reverse window executable portion
	void *dump;				//! This is the pointer to the memory area where chunk reversal has been dumped
	unsigned long long dummy;
	char code[];			//! Placeholder for the actual executable reverse code, i.e. from this point there is code
} revwin_t;




// ========================================= //
// ================== API ================== //


extern void reverse_init(size_t revwin_size);

extern void reverse_fini(void);

//extern void revwin_init(void);


/**
 * This will allocate a window on the HEAP of the exefutable file in order
 * to write directly on it the reverse code to be executed later on demand.
 *
 * @param size The size to which initialize the new reverse window. If the size paramter is 0, default
 * value will be used (REVERSE_WIN_SIZE)
 *
 * @return The address of the created window
 *
 * Note: mmap is invoked with both write and executable access, but actually
 * it does not to be a good idea since could be less portable and further
 * could open to security exploits.
 */
//extern void *create_new_revwin(size_t size);



/**
 * Initializes locally a new reverse window.
 *
 */
extern revwin_t *revwin_create(void);


/**
 * Free the reverse window passed as argument.
 *
 * @param window A pointer to a reverse window
 */
extern void revwin_free(unsigned int lid, revwin_t *win);


/**
 * Reset local reverse window
 *
 * @param win Pointer to the reverse window descriptor
 */
extern void revwin_reset(revwin_t *win);


/**
 * Will execute an undo event
 *
 * @param win Pointer to the reverse window descriptor
 */
extern void execute_undo_event(unsigned int lid, revwin_t *win);


/**
 * Prints some statistics of the software cache
 */
extern void print_cache_stats(void);


#endif /* __REVERSE_H */
#endif /* HAVE_REVERSE */
