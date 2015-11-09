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


// Variables to hold the emulated stack window on the heap
// used to reverse the event without affecting the actual stack;
// This is needed since the reverse-MOVs could overwrite portions
// of what was the function's stack in the past event processing.
// Prevents that possible reverse instructions that affect the old stack
// will stain the current one
__thread char estack[REVWIN_STACK_SIZE];
__thread void *orig_stack;

// Internal software cache to keep track of the reversed instructions
__thread revwin_cache cache;

// Keeps track of the current reversing strategy
__thread strategy_t strategy;


// Handling of the reverse windows
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
		fprintf(stderr, "Insufficent reverse window memory heap!\n");
		exit(-ENOMEM);
	}

	// copy the instructions to the heap
	memcpy(win->top, bytes, size);

	//printf("Added %ld bytes to the reverse window\n", size);
}


/*
 * Generates the reversing instruction for the whole chunk.
 *
 * @author Davide Cingolani
 *
 * @param address The starting address from which to copy
 * @param size The number of bytes to reverse
 */
static void reverse_chunk(revwin_t *win, const void *address, size_t size) {
	//void *dump;

	// unsigned char code[24] = {
	// 	0x48, 0xc7, 0xc1, 0x00, 0x00, 0x00, 0x00,
	// 	0x48, 0xc7, 0xc6, 0x00, 0x00, 0x00, 0x00,
	// 	0x48, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00,
	// 	0xf3, 0x48, 0xa5
	// };
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
		printf("Error reversing a memory chunk of %d bytes at %p\n", size, address);
		abort();
	}
	memcpy(win->dump, address, size);


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
static void reverse_single(revwin_t *win, const void *address, size_t size) {
	unsigned long value, value_lower;
	unsigned char *code;
	unsigned short size_code;

	unsigned char revcode_longword[17] = {
		0x48, 0xb8, 0xda, 0xd0, 0xda, 0xd0, 0x00, 0x00, 0x00, 0x00,			// movabs $0x0, %rax
		0x48, 0xc7, 0x00, 0xd3, 0xb0, 0x00, 0x00,							// mov $0x0, (%rax)
	};

	unsigned char revcode_quadword[25] = {
		0x48, 0xb8, 0xda, 0xd0, 0xda, 0xd0, 0x00, 0x00, 0x00, 0x00,			// movabs $0x0, %rax
		0x48, 0xc7, 0x00, 0xd3, 0xb0, 0x00, 0x00,							// mov $0x0, (%rax)
		0x48, 0xc7, 0x40, 0x04, 0xb0, 0xd3, 0x00, 0x00 						// mov $0x0, 4(%rax)
	};

	// Get the value pointed to by 'address'
	memcpy(&value, address, size);

	// We must handle the case of a quadword with two subsequent MOVs
	// properly embedding the upper and lower parts of values
	if(size == 8) {
		code = revcode_quadword;
		size_code = sizeof(revcode_quadword);
		value_lower = ((value >> 32) &0x0FFFFFFFF);
		memcpy(code+21, &value_lower, 4);

	} else {
		code = revcode_longword;
		size_code = sizeof(revcode_longword);
	}

	// Copy the destination address into the binary code
	// of MOVABS (first 2 bytes are the opcode)
	memcpy(code+2, &address, 8);
	memcpy(code+13, &value, 4);

	//printf("Single address reverse code generated\n");

	// Now 'code' contains the right code to add in the reverse window
	revwin_add_code(win, code, size_code);
}


/**
 * Reset the cluster cache.
 *
 * @author Davide Cingolani
 */
static void flush_cache() {
	int c, i;
	prefix_head *cluster;

	for(c = 0; c < PREFIX_HEAD_SIZE; c++) {
		cluster = &cache.cluster[c];

		cluster->prefix = NULL;

		for(i = 0; i < CLUSTER_SIZE; i++) {
			cluster->cache[i].address = 0;
			cluster->cache[i].touches = 0;
		}
	}
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

		if(entry->address != NULL) {
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
static void choose_strategy() {
	int i;
	double frag;

	// Computes the actual average fragmentation among all clusters
	// of the software cache
	frag = 0;
	for(i = 0; i < PREFIX_HEAD_SIZE; i++) {
		frag += cache.cluster[i].fragmentation;
	}
	frag /= cache.count;


	// Choose the strategy: if the current fragmentation
	// is lower than a certain threshold it is reasonable
	// to coalesce reversing instrucitons to one chunk
	if(frag < REVERSE_THRESHOLD) {
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
static bool check_dominance(void *address) {
	int line_number;
	malloc_area *area;
	char bit;
	int i;

	unsigned long long page_address;
	unsigned int address_idx;
	unsigned int counter;
	cache_entry *entry;
	prefix_head *cluster;

	// line_number = GET_CACHE_LINE_NUMBER((unsigned long)address);
	// area = get_area_from_ptr(address);

	// if(area == NULL) {
	// 	printf("Failed to get malloc_area\n");
	// 	abort();
	// }

	//printf("Check dominance for %p\n", address);

	// Computes the prefix within the head list
	page_address = (unsigned long long)address & ADDRESS_PREFIX;

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
			if(cache.cluster[i].prefix == NULL)
				break;
		}

		// Check whether the cache is full
		if (i == PREFIX_HEAD_SIZE) {
			printf("Cache full, no entry found\n");
			// TODO: gestire diversamente questo caso!
			abort();
		}

		cache.cluster[i].prefix = page_address;
		cache.count++;
		cluster = &cache.cluster[i];

		//printf("Added prefix %p at cluster %d\n", page_address, i);
	}

	// Computes the index within the cluster
	address_idx = (unsigned long long)address & (CLUSTER_SIZE - 1);

	// Once the pointer to the correct address cluster is found, if any,
	// check the presence of the specific address

	entry = &cluster->cache[address_idx];
	if(entry->address != NULL) {
		entry->touches++;
		return true;
	}

	// If the pointer to the cluster entry is empty, this means that 'address'
	// has not been yet referenced and must be set for the first time at the indexed cell
	entry->address = address;
	entry->touches = 1;
	cluster->count++;


	// A new address which wasn't in cache has been touched and added
	// therefore it is necessary to udpate the cluster's metadata

	// Now, updates cluster's statistics
	cluster->fragmentation = 0;

	for(i = 0; i < CLUSTER_SIZE; i++) {
		entry = &cluster->cache[i];

		if(entry->address == NULL) {
			if(counter > cluster->contiguous)
				cluster->contiguous = counter;

			// Reset the counter of contiguous addresses and increment
			// the count of NULL fragmentation slots
			counter = 0;
			cluster->fragmentation++;

			continue;
		}

		cluster->count++;
		counter++;
	}

	cluster->fragmentation /= CLUSTER_SIZE;


	// Check whether to switch to another reversing strategy
	// according to the internal fragemntation of addresses
	// within the cache
	choose_strategy();


	//printf("Checking address dominance for %p :: cache line %d malloc_area address %p\n",
	//	address, line_number, area);

	// Check whether i-th bit is dirty
	/*if(CHECK_DIRTY_BIT(area, bit) == 0){
		// The address is not referenced yet, therefore we it must be tracked
		// and its relative dirty bit set.

		SET_DIRTY_BIT(area, bit);

		return true;
	}*/

	// The address is has been yet referenced previously, therefore
	// it is not necessary to reverse it again since the value would
	// be overwritten anyway by the first reversal.
	return false;
}


/**
 * Computes how many bytes have been touched per single malloc_area based
 * on the knoledge of ts dirty bitmap.
 *
 * @author Davide Cingolani
 *
 * @param area Pointer to the malloc_area in exam
 * 
 * @return the number of bytes actually reversed per single malloc_area
 */
static int compute_span(malloc_area *area) {
	int bit;
	unsigned int dirty;

	dirty = 0;
	for(bit = 0; bit < area->chunk_size; bit++) {
		if(CHECK_DIRTY_BIT(area, bit))
			dirty++;
	}

	return dirty;
}


revwin_t *revwin_create(void) {
	revwin_t *win;

	unsigned char code_closing[2] = {0x58, 0xc3};

	// Set the current window as the one registred
/*	win = rsalloc(sizeof(revwin));
	if(win == NULL) {
		perror("Failed to allocate reverse window descriptor\n");
		abort();
	}
	memset(win, 0, sizeof(revwin));*/

	// Query the slab allocator to retrieve a new reverse window
	// executable area. The address represents the base address of
	// the revwin descriptor which contains the reverse code itself.
	win = slab_alloc(slab_chain);
	if(win == NULL) {
		printf("Unable to allocate a new reverse window: SLAB failure\n");
		abort();
	}
	memset(win, 0, REVWIN_SIZE);


	// Register the reverse window in the global structure
	//map->map[tid] = win;

	// Initialize reverse window's fields
	//size = REVWIN_SIZE;
	//address = (void *)((char *)map->address + (size * tid));

	win->size = REVWIN_CODE_SIZE;
	//win->base = (void *)((char *)win + sizeof(revwin));
	win->base = win->code;
	win->top = (void *)((char *)win->base + win->size - 1);

	// Allocate a new slot in the reverse mapping, accorndigly to
	// the number of yet allocated windows
#if RANDOMIZE_REVWIN
	win->offset = rand() % REVWIN_RZONE_SIZE;
	win->base = (void *)((char *)win->base + win->offset);
#endif


	// Adds the closing instructions to the end of the window
	revwin_add_code(win, code_closing, sizeof(code_closing));

	/**
	 * Allocate a new stack window in order to not modify anything
	 * on the real one, otherwise during the reversible undo event
	 * processing, the old stack will overwrite the current one which
	 * collide with the execute_undo_event() function
	 */

	// Allocate 1024 bytes for a limited memory which emulates
	// the future stack of the execute_undo_event()'s stack
	// TODO: da sistemare!!!!
/*	win->estack = umalloc(current_lp, EMULATED_STACK_SIZE);
	if(win->estack == NULL) {
		perror("Failed to allocate the emulated stack window\n");
		abort();
	}
	// The stack grows towards low addresses
	win->estack += (EMULATED_STACK_SIZE - 1);*/

	printf("Reverse window of thread %d has been initialized :: base at %p top at %p, %ld bytes\n", tid, win->base, win->top, win->size);

	return win;
}


void revwin_free(revwin_t *win) {

	// Check whether the dump chunk area is not NULL
	if (win->dump != NULL) {
		ufree(win->dump);
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
void revwin_reset(revwin_t *win) {

	if (win == NULL) {
		printf("Error: null reverse window\n");
		abort();
	}

	// Resets the instruction pointer to the first byte AFTER the colsing
	// instruction at the base of the window
	win->top = (void *)(((char *)win->base) + win->size - 3);

	// Reset the cache
	// TODO: quando resettare la cache??
	// flush_cache();

	// TODO: Should be reset also the chunk dump area, if present?
}


/**
 * Prints some usage statistics of the software cache
 *
 * @author Davide Cingolani
 */
void print_cache_stats() {
	int c;
	double utilization;
	prefix_head *cluster;

	utilization = 0;
	for(c = 0; c < PREFIX_HEAD_SIZE; c++) {
		cluster = &cache.cluster[c];

		if(cluster->prefix != NULL) {
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
 * @param address The pointer to the memeory location which the MOV refers to
 * @param size The size of data will be written by MOV
 */
void reverse_code_generator(const void *address, const size_t size) {
	void *chunk_address;
	size_t chunk_size;
	malloc_area *area;
	bool dominant;
	revwin_t *win;

	
	//printf("[%d] :: reverse_code_generator(%p, %ld) => %lx\n", tid, address, size, *((unsigned long *)address));

	timer t;
	timer_start(t);

	// We have to retrieve the current event structure bound to this LP
	// in order to bind this reverse window to it.
	msg_t *event = LPS[current_lp]->bound;
	win = event->revwin;
	if(win == NULL) {
		printf("No revwin has been defined for the event\n");
		abort();
	}

	// In order to dump the whole chunk reverse, now we inquire
	// dymelor to get the malloc_area struct which contains the
	// current address.

	// TODO: get dymelor's malloc_area to dump the whole chunk
	// area = get_area_from_ptr(address);
	// if(area == NULL) {
	// 	printf("Returned a NULL malloc_area\n");
	// 	abort();
	// }
	// chunk_size = area->chunk_size;
	// chunk_address = area->area;

	//cache[]

	// Check whether the current address' update dominates over some other
	// update on the same memory region. If so, we can return earlier.
	dominant = check_dominance(address);
	if(dominant) {
		// If the current address is dominated by some other update,
		// then there is no need to generate any reversing instruction
		return;
	}

	// int blocks;
	// blocks = compute_span(area);
	// printf("In malloc_area at %p, %d bytes have been reversed so far\n", area, blocks);

	// Act accordingly to the currrent selected reversing strategy
	switch (strategy.current){
		case STRATEGY_CHUNK:
			// Reverse the whole malloc_area chunk passing the pointer
			// of the target memory chunk to reverse (not the malloc_area one)
			chunk_address = (unsigned long long)address & ADDRESS_PREFIX;
			chunk_size = CLUSTER_SIZE;
			reverse_chunk(win, chunk_address, chunk_size);
			//cache.reverse_bytes += chunk_size;
			break;

		case STRATEGY_SINGLE:
			// Reverse the single buffer access
			reverse_single(win, address, size);
			//cache.reverse_bytes += size;
			break;
	}

	// Gather statistics data
	double elapsed = (double)timer_value_micro(t);
	stat_post_lp_data(current_lp, STAT_REVERSE_GENERATE, 1.0);
	stat_post_lp_data(current_lp, STAT_REVERSE_GENERATE_TIME, elapsed);

	//printf("[%d] :: Reverse MOV instruction generated to save value %lx\n", tid, *((unsigned long *)address));


	// TODO: evaluate how many bytes have been touched so far

	// TODO: use dymelor's dirty bitmap to track the reversed addresses

	// Checks whether this address has been previously touched
	// by looking at the dirty bitmap
	// TODO: check dirty bit
}


/**
 * Executes the code actually present in the reverse window
 *
 * @author Davide Cingolani
 *
 * @param w Pointer to the actual window to execute
 */
void execute_undo_event(revwin_t *win) {
	unsigned char push = 0x50;
	revwin_t *win;
	void *revcode;

	timer reverse_block_timer;

	timer_start(reverse_block_timer);

	// Add the complementary push %rax instruction to the top
	revwin_add_code(win, &push, sizeof(push));

	// Retrieve the reverse window associeted to this event
	//win = LPS[current_lp]->bound->revwin;

	// Temporary swaps the stack pointer to use
	// the emulated one on the heap, instead

	// Save the original stack pointer into 'original_stack'
	// and substitute $RSP the new emulated stack pointer
	if (estack == NULL) {
		printf("[%d] :: Emulated stack NULL!\n", tid);
		abort();
	}
	__asm__ volatile ("movq %%rsp, %0\n\t"
					  "movq %1, %%rsp" : "=m" (orig_stack) : "m" (estack));

	// TODO: move to define; this is not necessary, actually
	// emulated stack prevents only that possible spurious
	// stack-referencing instuctions could hurt the real stack
	// by reversing old data referring to the old stack


	// Calls the reversing function
	revcode = win->top;
	((void (*)(void))revcode) ();

	printf("===> [%d] :: undo event executed\n", tid);

	// Replace the original stack on the relative register
	__asm__ volatile ("movq %0, %%rsp" : : "m" (orig_stack));

	double elapsed = (double)timer_value_micro(reverse_block_timer);


	// FIXME: mi sa che qui current_lp non ècorrettamente impostato, perché viene impostato solo
	// durante la forward execution
	stat_post_lp_data(current_lp, STAT_REVERSE_EXECUTE, 1.0);
	stat_post_lp_data(current_lp, STAT_REVERSE_EXECUTE_TIME, elapsed);


	// Check if the revwin is a chunk reversal, then
	// we have to free also the dump memory area
	if(win->dump != NULL) {
		ufree(win->dump);
	}

	// Reset the reverse window
	//reset_window(w);
}

#endif /* HAVE_REVERSE */


