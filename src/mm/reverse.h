#ifdef HAVE_REVERSE

#pragma once
#ifndef __REVERSE_H
#define __REVERSE_H

#include <sys/types.h>

//#include <mm/dymelor.h>


#define REVWIN_SIZE 1024 * 32	//! Defalut size of the reverse window which will contain the reverse code
#define REVWIN_CODE_SIZE (REVWIN_SIZE - sizeof(revwin_t))
#define REVWIN_RZONE_SIZE 100		//! Size of the red zone in the reverse window that will be skipped to prevent cache misses
#define RANDOMIZE_REVWIN 0 			//! Activate the rendomization of the addresses used by revwin to prevent cache misses

#define PAGE_SIZE 4096

#define CLUSTER_SIZE 64
#define ADDRESS_PREFIX (-CLUSTER_SIZE)
#define CLUSTER_PREFIX (CLUSTER_SIZE - 1)
#define PREFIX_HEAD_SIZE 1024

#define FRAG_THRESHOLD 0.15
#define LOAD_THRESHOLD 0.15

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
	unsigned int load;					//! Number of total address in this cluster (current cluster load)
	unsigned int contiguous;			//! Maximum number of contiguous addresses registered
	double fragmentation;				//! Internal fragmentation of addresses in the cluster, i.e. how much they are sparse
	cache_entry cache[CLUSTER_SIZE];	//! List of registered addresses with current prefix
} prefix_head;


/**
 * Main descriptor of the cluster cache. This maintains the pointer
 * to a linked list of clusters of addresses, i.e. the prefix_head,
 * in order to collect infomation about memory reversing for each
 * address cluster of the specified size.
 */
typedef struct _revwin_cache {
	unsigned int load;							//! The overall load factor of the software cache
	prefix_head cluster[PREFIX_HEAD_SIZE];		//! Array of cache clusters
} revwin_cache;



/**
 * Descriptor of a single reverse window
 */
 typedef struct _revwin_t {
	struct _revwin_t *prev;			//! Pointer to the previous reverse window
	void *top;				//! Pointer to the first instruction byte to be executed
	void *base;				//! Pointer to the logic base of the window
	unsigned int offset;	//! A random offset used to prevent cache lines to be alligned
	size_t size;			//! The actual size of the reverse window executable portion
	void *dump;				//! This is the pointer to the memory area where chunk reversal has been dumped
	char code[];			//! Placeholder for the actual executable reverse code, i.e. the point where the executable code is
} revwin_t;




// ========================================= //
// ================== API ================== //

/**
 * Initialize a thread local reverse manager to build and populate reverse windows
 * for the simulation events. This manager leans on a SLAB allocator to fast handle
 * creation and destruction of reverse windows.
 *
 * @author Davide Cingolani
 *
 * @param revwin_sise The size the SLAB will allocate for each reverse window
 */
extern void reverse_init(size_t revwin_size);

/** 
 * Finalize the reverse manager. It must be called at the end of the overall execution
 * in order to clean up the internal allocator and free the resources.
 *
 * @author Davide Cingolani
 */
extern void reverse_fini(void);


/**
 * Initializes locally a new reverse window.
 *
 * @author Davide Cingolani
 */
extern revwin_t *revwin_create(void);


/**
 * Free the reverse window passed as argument.
 *
 * @author Davide Cingolani 
 *
 * @param window A pointer to a reverse window
 */
extern void revwin_free(unsigned int lid, revwin_t *win);


/**
 * Reset local reverse window
 *
 * @author Davide Cingolani 
 *
 * @param lid The ID of the LP issuing the execution
 * @param win Pointer to the reverse window descriptor
 */
extern void revwin_reset(unsigned int lid, revwin_t *win);


/**
 * Prompt the execution of the specified reverse window.
 *
 * @author Davide Cingolani 
 *
 * @param lid The ID of the LP issuing the execution
 * @param win Pointer to the reverse window descriptor
 */
extern void execute_undo_event(unsigned int lid, revwin_t *win);


/**
 * Prints some statistics of the software 
 *
 * @author Davide Cingolani 
 */
extern void print_cache_stats(void);


/**
 * Computes the actual size of the passed reverse window.
 *
 * @author Davide Cingolani
 *
 * @param win Pointer to the reverse window descriptor
 *
 * @returns A size_t representing the size of the passed reverse window
 */
extern size_t revwin_size(revwin_t *win);


/**
 * Clean up the software cache used to keep track of the reversed addresses.
 * Upon this cache is built the model to choose the reversing strategy to
 * apply.
 *
 * @author Davide Cingolani
 *
 */
extern void revwin_flush_cache(void);


#endif /* __REVERSE_H */
#endif /* HAVE_REVERSE */

