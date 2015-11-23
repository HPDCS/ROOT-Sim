#ifdef HAVE_REVERSE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>

#include <mm/reverse.h>
#include <mm/dymelor.h>
#include <core/timer.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <datatypes/slab.h>


//! Internal software cache to keep track of the reversed instructions
__thread revwin_cache cache;

//! Keeps track of the current reversing strategy
__thread strategy_t strategy;

//! Handling of the reverse windows
__thread struct slab_chain *slab_chain;


/*
 * Adds the passed exeutable code to the reverse window
 *
 * @author Davide Cingolani
 *
 * @param bytes Pointer to the buffer to write
 * @param size Number of bytes to write
 */
static void revwin_add_code(revwin_t *win, unsigned char *bytes, size_t size) {

	// Since the structure is used as a stack, it is needed to create room for the instruction
	win->top = (void *)((char *)win->top - size);

	if (win->top < win->base) {
		printf("[LP%d] :: event %d win at %p\n", current_lp, current_evt->type, win);
		fprintf(stderr, "Insufficent reverse window memory heap!\n");
		exit(-ENOMEM);
	}

	// copy the instructions to the reverse window code area
	memcpy(win->top, bytes, size);

//	printf("Added %ld bytes to the reverse window\n", size);
}


/*
 * Generates the reversing instruction for the whole chunk.
 *
 * @author Davide Cingolani
 *
 * @param address The starting address from which to copy
 * @param size The number of bytes to reverse
 */
static void reverse_chunk(revwin_t *win, const unsigned long long address, size_t size) {
	unsigned char code[36] = {
		0x48, 0xc7, 0xc1, 0x00, 0x00, 0x00, 0x00,							// mov 0x0,%rcx
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,			// movabs 0x0,%rax
		0x48, 0x89, 0xc6,													// mov %rax,%rsi
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,			// movabs 0x0,%rax
		0x48, 0x89, 0xc7,													// mov %rax,%rdi
		0xf3, 0x48, 0xa5													// rep movsq
	};

	unsigned char *mov_rcx = code;
	unsigned char *mov_rsi = code + 7;
	unsigned char *mov_rdi = code + 20;

	// Dumps the chunk content, this memory region
	// must be freed manually, if present, when necessary
	// e.g. execute_undo_event, and revwin_free
	win->dump = umalloc(current_lp, size);
	if(win->dump == NULL) {
		printf("Error reversing a memory chunk of %ld bytes at %llx\n", size, address);
		abort();
	}
	memcpy(win->dump, (void *)address, size);


	#ifdef REVERSE_SSE_SUPPORT
	// TODO: support sse instructions
	#else
	// Copy the chunk size in RCx
	memcpy(mov_rcx+3, &size, 4);
	
	// Copy the first address
	memcpy(mov_rsi+2, &win->dump, 8);

	// Compute and copy the second part of the address
	memcpy(mov_rdi+2, &address, 8);
	#endif

	//printf("Chunk addresses reverse code generated\n");

	// Now 'code' contains the right code to add in the reverse window
	revwin_add_code(win, code, sizeof(code));
}


/*
 * Generates the reversing instruction for a single addres,
 * i.e. the one passed as argument.
 *
 * @author Davide Cingolani
 *
 * @param address The starting address from which to copy
 * @param size The number of bytes to reverse
 */
static void reverse_single(revwin_t *win, const unsigned long long address, size_t size) {
	unsigned long long value, value_lower;
	unsigned char *code;
	unsigned short size_code;

	unsigned char revcode_byte[14] = {
		0x48, 0xb8, 0xda, 0xd0, 0xda, 0xd0, 0x00, 0x00, 0x00, 0x00,			// movabs $0x0, %rax
		0xc6, 0x00, 0xaa,													// movb $0x0, (%rax)
	};

	unsigned char revcode_word[15] = {
		0x48, 0xb8, 0xda, 0xd0, 0xda, 0xd0, 0x00, 0x00, 0x00, 0x00,			// movabs $0x0, %rax
		0x66, 0xc7, 0x00, 0xaa, 0xaa,										// movw $0x0, (%rax)
	};

	unsigned char revcode_longword[16] = {
		0x48, 0xb8, 0xda, 0xd0, 0xda, 0xd0, 0x00, 0x00, 0x00, 0x00,			// movabs $0x0, %rax
		0xc7, 0x00, 0xaa, 0xaa, 0xaa, 0xaa,									// movl $0x0, (%rax)
	};

	unsigned char revcode_quadword[23] = {
		0x48, 0xb8, 0xda, 0xd0, 0xda, 0xd0, 0x00, 0x00, 0x00, 0x00,			// movabs $0x0, %rax
		0xc7, 0x00, 0xd3, 0xb0, 0x00, 0x00,									// movl $0x0, (%rax)
		0xc7, 0x40, 0x04, 0xb0, 0xd3, 0x00, 0x00 							// movl $0x0, 4(%rax)
	};

	// Get the value pointed to by 'address'
	memcpy(&value, (void *)address, 8);

	switch(size) {
		case 1:
			// Byte
			code = revcode_byte;
			size_code = sizeof(revcode_byte);
			memcpy(code+12, &value, 1);
		break;

		case 2:
			// Word
			code = revcode_word;
			size_code = sizeof(revcode_word);
			memcpy(code+13, &value, 2);
		break;

		case 4:
			// Longword
			code = revcode_longword;
			size_code = sizeof(revcode_longword);
			memcpy(code+12, &value, 4);
		break;

		case 8:
			// Quadword
			code = revcode_quadword;
			size_code = sizeof(revcode_quadword);
			value_lower = ((value >> 32) & 0x0FFFFFFFF);
			memcpy(code+12, &value, 4);
			memcpy(code+19, &value_lower, 4);
		break;
	}

	// We must handle the case of a quadword with two subsequent MOVs
	// properly embedding the upper and lower parts of values
/*	if(size == 8) {
		code = revcode_quadword;
		size_code = sizeof(revcode_quadword);
		value_lower = ((value >> 32) &0x0FFFFFFFF);
		memcpy(code+19, &value_lower, 4);

	} else {
		code = revcode_longword;
		size_code = sizeof(revcode_longword);
	}
*/
	// Copy the destination address into the binary code
	// of MOVABS (first 2 bytes are the opcode)
	memcpy(code+2, &address, 8);
//	memcpy(code+12, &value, 4);

	//printf("Single address reverse code generated (%d bytes)\n", size);

	// Now 'code' contains the right code to add in the reverse window
	revwin_add_code(win, code, size_code);
}


/**
 * Print a dump of the current state of the software cache.
 *
 * @author Davide Cingolani
 */
static void dump_cluster(prefix_head *cluster) {
	int i;
	cache_entry *entry;

	for(i = 0; i < CLUSTER_SIZE; i++) {
		entry = &cluster->cache[i];

		if(entry->address != 0) {
			printf("[%d] => %llx, %d\n", i, entry->address, entry->touches);
		}
	}

	printf("Cluster prefix %llx => frag=%f, contiguous=%d\n", cluster->prefix, cluster->fragmentation, cluster->contiguous);
}


/**
 * On the basis of the cache's fragmentation it chooses the right
 * reversing strategy.
 *
 * @author Davide Cingolani
 */
static void choose_strategy(void) {
	int i;
	double frag;
	double avg_load;

	// Computes the actual average fragmentation among all clusters
	// of the software cache
	frag = 0;
	avg_load = 0;
	for(i = 0; i < PREFIX_HEAD_SIZE; i++) {
		frag += cache.cluster[i].fragmentation;
		avg_load += cache.cluster[i].load;
	}
	frag /= cache.load;

//	printf("Frag=%f, avg_load=%f\n", frag, avg_load);

	// TODO: introdurre un modello di costo più complesso

	// Choose the strategy: if the current fragmentation
	// is lower than a certain threshold it is reasonable
	// to coalesce reversing instrucitons to one chunk

	// TODO: forced to single
	if(0 && frag < FRAG_THRESHOLD) {
		if(strategy.current != STRATEGY_CHUNK)
			printf("Strategy switch to Chunk\n");
		
		strategy.current = STRATEGY_CHUNK;
	} else {
		if(strategy.current != STRATEGY_SINGLE)
			printf("Strategy switch to Single\n");

		strategy.current = STRATEGY_SINGLE;
	}
}


/**
 * Check if the address is dirty by looking at the hash map. In case the address
 * is not present adds it and return 0.
 *
 * @author Davide Cingolani
 *
 * @param address The address to check
 *
 * @return true if the reverse MOV instruction relative to 'address' would be
 * the dominant one, false otherwise
 */
static bool check_dominance(unsigned long long address) {
	int i;

	unsigned long long page_address;
	unsigned int address_idx;
	unsigned int counter;
	cache_entry *entry;
	prefix_head *cluster;


	// Computes the prefix within the head list
	page_address = address & ADDRESS_PREFIX;

	// TODO: la cache non è ordinata e richiede una ricerca lineare

	// Search for the right cluster with the prefix 'page_address'
	cluster = NULL;
	for(i = 0; i < PREFIX_HEAD_SIZE; i++) {
		if(cache.cluster[i].prefix == page_address) {
			// The right cluster has been found
			cluster = &cache.cluster[i];
			break;
		}
	}

	if(cluster == NULL) {
		// Otherwinse, if no cluster has been found
		// a new one must be addes with this prefix
		for(i = 0; i < PREFIX_HEAD_SIZE; i++) {
			if(cache.cluster[i].prefix == 0)
				break;
		}

		// Check whether the cache is full
		if (i == PREFIX_HEAD_SIZE) {
			printf("Reverse cache is full!\n");
			// TODO: gestire diversamente questo caso!
			abort();
		}

		cache.cluster[i].prefix = page_address;
		cache.load++;
		cluster = &cache.cluster[i];

		//printf("Added prefix %p at cluster %d\n", page_address, i);
	}

	// Computes the index within the cluster
	address_idx = address & CLUSTER_PREFIX;

	// Once the pointer to the correct address cluster is found, if any,
	// check the presence of the specific address
	entry = &cluster->cache[address_idx];
	if(entry->address != 0) {
		entry->touches++;
		return true;
	}

	// If the pointer to the cluster entry is empty, this means that 'address'
	// has not been yet referenced and must be set for the first time at the indexed cell
	entry->address = address;
	entry->touches = 1;
	cluster->load++;


	// A new address which wasn't in cache has been touched and added
	// therefore it is necessary to udpate the cluster's metadata

	// Now, updates cluster's statistics
	cluster->fragmentation = 0;
	counter = 0;

	for(i = 0; i < CLUSTER_SIZE; i++) {
		entry = &cluster->cache[i];

		if(entry->address == 0) {
			if(counter > cluster->contiguous)
				cluster->contiguous = counter;

			// Reset the counter of contiguous addresses and increment
			// the count of NULL fragmentation slots
			counter = 0;
			cluster->fragmentation++;

			continue;
		}

		cluster->load++;
		counter++;
	}

	cluster->fragmentation /= CLUSTER_SIZE;



	// Check whether to switch to another reversing strategy
	// according to the internal fragemntation of addresses
	// within the cache
	choose_strategy();

	// This address has been yet referenced previously, therefore
	// it is not necessary to reverse it again since the value would
	// be overwritten anyway by the first reversal.
	return false;
}


void revwin_flush_cache(void) {
	memset(&cache, 0, sizeof(revwin_cache));
}
/*void revwin_flush_cache(void) {
	int c, i;
	prefix_head *cluster;

	for(c = 0; c < PREFIX_HEAD_SIZE; c++) {
		cluster = &cache.cluster[c];

		memset(cluster, 0, sizeof(prefix_head));
//		cluster->prefix = 0;
//		cluster->load = 0;
//		cluster->fragmentation = 0;

//		for(i = 0; i < CLUSTER_SIZE; i++) {
//			memset(cluster->cache[i], 0, sizeof(cache_entry));
//			cluster->cache[i].address = 0;
//			cluster->cache[i].touches = 0;
//		}
	}

	cache.load = 0;
}*/


revwin_t *revwin_create(void) {
	revwin_t *win;

	unsigned char code_closing[2] = {0x58, 0xc3};

	// Query the slab allocator to retrieve a new reverse window
	// executable area. The address represents the base address of
	// the revwin descriptor which contains the reverse code itself.
	win = slab_alloc(slab_chain);
	if(win == NULL) {
		printf("Unable to allocate a new reverse window: SLAB failure\n");
		abort();
	}
	memset(win, 0, REVWIN_SIZE);


	// Initialize reverse window's fields
	win->size = REVWIN_CODE_SIZE;
	win->base = win->code;
	win->top = (void *)((char *)win->base + win->size - 1);

	// Allocate a new slot in the reverse mapping, accorndigly to
	// the number of yet allocated windows
#if RANDOMIZE_REVWIN
	win->offset = rand() % REVWIN_RZONE_SIZE;
	win->base = (void *)((char *)win->base + win->offset);
#endif


	// Initialize the executable code area with the closing
	// instructions at the end of the actual window.
	// In this way we are sure the exection will correctly returns
	// once the whole revwin has been reverted.
	revwin_add_code(win, code_closing, sizeof(code_closing));

	return win;
}


void revwin_free(unsigned int lid, revwin_t *win) {

	// Sanity check
	if (win == NULL) {
		return;
	}

	// Check whether the dump chunk area is not NULL
	if (win->dump != NULL) {
		ufree(lid, win->dump);
	}

	// Free the slab area
	slab_free(slab_chain, win);
}


/**
 * Initializes a the reverse memory region of executables reverse windows. Each slot
 * is managed by a slab allocator and represents a reverse window to be executed.
 * Reverse widnows are bound to events by a pointer in their message descriptor.
 *
 * @author Davide Cingolani
 *
 * @param revwin_size The size of the reverse window
 */
void reverse_init(size_t revwin_size) {

	// Allocate the structure needed by the slab allocator
	slab_chain = rsalloc(sizeof(struct slab_chain));
	if(slab_chain == NULL) {
		printf("Unable to allocate memory for the SLAB structure\n");
		abort();
	}

	// In this step we should initialize the slab allocator in order
	// to fast handle allocation and deallocation of reverse windows
	// which will be created by each event indipendently.
	slab_init(slab_chain, revwin_size);

	// Reset the cluster cache
	revwin_flush_cache();
}


void reverse_fini(void) {
	// TODO: implementare
	// Free each revwin still allocated ?

	// Destroy the SLAB allocator
	slab_destroy(slab_chain);
}


/*
 * Reset the reverse window intruction pointer
 */
void revwin_reset(unsigned int lid, revwin_t *win) {

	// Sanity check
	if (win == NULL) {
		// We dont care about NULL revwin
		return;
	}

	// Resets the instruction pointer to the first byte AFTER the closing
	// instruction at the base of the window (which counts 2 bytes)
	win->top = (void *)(((char *)win->base) + win->size - 3);

	// Reset the cache
	// TODO: quando resettare la cache??
	// flush_cache();

	// Reset also the chunk dumping area, if present
	if(win->dump != NULL) {
		ufree(lid, win->dump);
	}
}


/**
 * Prints some usage statistics of the software cache
 *
 * @author Davide Cingolani
 */
void print_cache_stats(void) {
	int c;
	double utilization;
	prefix_head *cluster;

	utilization = 0;
	for(c = 0; c < PREFIX_HEAD_SIZE; c++) {
		cluster = &cache.cluster[c];

		if(cluster->prefix != 0) {
			utilization++;
			dump_cluster(cluster);
			printf("=======================\n");
		}
	}

	printf("Cache utilization factor=%f (%d used, %d free)\n", utilization/PREFIX_HEAD_SIZE, (int)utilization, PREFIX_HEAD_SIZE);
	printf("=========================================================\n");
}



/**
 * Adds new reversing instructions to the current reverse window.
 * Genereate the reverse MOV instruction staring from the knowledge of which
 * memory address will be accessed and the data width of the write.
 * 
 * @author Davide Cingolani
 *
 * @param address The address of the memeory location to which the MOV refers
 * @param size The size of data will be written by MOV
 */

#define SIMULATED_INCREMENTAL_CKPT if(0)

void reverse_code_generator(const unsigned long long address, const size_t size) {
	unsigned long long chunk_address;
	size_t chunk_size;
	bool dominant;
	revwin_t *win;

	//SIMULATED_INCREMENTAL_CKPT return;

	// We have to retrieve the current event structure bound to this LP
	// in order to bind this reverse window to it.
	win = current_evt->revwin;
	if(win == NULL) {
		printf("No revwin has been defined for the event\n");
		abort();
	}

	timer t;
	timer_start(t);


	// Check whether the current address' update dominates over some other
	// update on the same memory region. If so, we can return earlier.
	
	dominant = check_dominance(address);
//	dominant = false;
	if(dominant) {
		// If the current address is dominated by some other update,
		// then there is no need to generate any reversing instruction
		return;
	}

	// Act accordingly to the currrent selected reversing strategy
	switch (strategy.current){
		case STRATEGY_CHUNK:
			// Reverse the whole malloc_area chunk passing the pointer
			// of the target memory chunk to reverse (not the malloc_area one)
			chunk_address = address & ADDRESS_PREFIX;
			chunk_size = CLUSTER_SIZE;
			reverse_chunk(win, chunk_address, chunk_size);
			break;

		case STRATEGY_SINGLE:
			// Reverse the single buffer access
			reverse_single(win, address, size);
			break;
	}


	// Gather statistics data
	double elapsed = (double)timer_value_micro(t);
	statistics_post_lp_data(current_lp, STAT_REVERSE_GENERATE, 1.0);
	statistics_post_lp_data(current_lp, STAT_REVERSE_GENERATE_TIME, elapsed);

	//printf("[%d] :: Reverse MOV instruction generated to save value %lx\n", tid, *((unsigned long *)address));
}


size_t revwin_size(revwin_t *win) {
	if(win == NULL)
		return 0;
	return (((unsigned long long)win->base + (unsigned long long)win->size - 3) - (unsigned long long)win->top);
}

/**
 * Executes the code actually present in the reverse window
 *
 * @author Davide Cingolani
 *
 * @param w Pointer to the actual window to execute
 */
void execute_undo_event(unsigned int lid, revwin_t *win) {
	unsigned char push = 0x50;
	void *revcode;
	int err;


	// Sanity check
	if (win == NULL) {
		// There is nothing to execute, actually
		return;
	}

	// Statistics
	timer reverse_block_timer;
	timer_start(reverse_block_timer);


	// Add the complementary push %rax instruction to the top
	revwin_add_code(win, &push, sizeof(push));

	// Register the pointer to the reverse code function
	revcode = win->top;

	// Calls the reversing function
	((void (*)(void))revcode) ();


	double elapsed = (double)timer_value_micro(reverse_block_timer);
	statistics_post_lp_data(lid, STAT_REVERSE_EXECUTE, 1.0);
	statistics_post_lp_data(lid, STAT_REVERSE_EXECUTE_TIME, elapsed);


	printf("===> [%d] :: undo event executed (size = %ld bytes)\n", tid, revwin_size(win));

	// Check if the revwin is a chunk reversal, then
	// we have to free also the dump memory area
	if(win->dump != NULL) {
		ufree(lid, win->dump);
	}

	// Reset the reverse window
	// TODO: ?
	//reset_window(w);
}

#endif /* HAVE_REVERSE */


