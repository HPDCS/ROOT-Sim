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

/**
 * This is the pointer to the mmap structure that handles all
 * the revrese windows.
 */
static revwin_mmap *map;

//! Comodity pointer to the thread's local reverse window's descriptor
//__thread revwin *current_win;


// Variables to hold the emulated stack window on the heap
// used to reverse the event without affecting the actual stack;
// This is needed since the reverse-MOVs could overwrite portions
// of what was the function's stack in the past event processing.

//__thread revwin *current_win;
__thread unsigned int revsed;

__thread cluster_cache cache;
__thread strategy_t strategy;

/*
 * Adds the passed exeutable code to the reverse window
 *
 * @author Davide Cingolani
 *
 * @param bytes Pointer to the buffer to write
 * @param size Number of bytes to write
 */
static void revwin_add_code(revwin *win, unsigned char *bytes, size_t size) {

	// Since the structure is used as a stack, it is needed to create room for the instruction
	win->top = (void *)((char *)win->top - size);

	if (win->top < win->base) {
		fprintf(stderr, "Insufficent reverse window memory heap!\n");
		exit(-ENOMEM);
	}

	// copy the instructions to the heap
	memcpy(win->top, bytes, size);
	revsed += size;

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
static void reverse_chunk(const void *address, size_t size) {
	unsigned long long addr;
	void *memory;

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
//	unsigned char *movs = code + 21;

	// Dumps the chunk content
	memory = umalloc(current_lp, size);
	if(memory == NULL) {
		printf("Error reversing a memory chunk of %d bytes at %p\n", size, address);
		abort();
	}
	memcpy(memory, address, size);

	// TODO: build the instructions
	// REP until size has been reached
	// MOVSQ

	#ifdef REVERSE_SSE_SUPPORT
	// TODO: support sse instructions
	#else
	// Copy the chunk size in RCx
	memcpy(mov_rcx+3, &size, 4);
	
	// Copy the first address
	memcpy(mov_rsi+2, &memory, 8);

	// Compute and copy the second part of the address
	addr = memory;
	addr = address + size;
	memcpy(mov_rdi+2, &addr, 8);
	#endif

	//printf("Chunk addresses reverse code generated\n");

	// Now 'code' contains the right code to add in the reverse window
	revwin_add_code(current_win, code, sizeof(code));
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
static void reverse_single(const void *address, size_t size) {
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
	revwin_add_code(current_win, code, size_code);
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


	// Search for the right cluster with prefix 'page_address'
	cluster = NULL;
	for(i = 0; i < PREFIX_HEAD_SIZE; i++) {
		if(cache.cluster[i].prefix == page_address) {
			cluster = &cache.cluster[i];
			break;
		}
	}

	if(cluster == NULL) {
		// Otherwinse, if there isn't, adds a new prefix
		for(i = 0; i < PREFIX_HEAD_SIZE; i++) {
			if(cache.cluster[i].prefix == NULL)
				break;
		}

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

	// Computes the index within the cluster cache
	address_idx = (unsigned long long)address & (CLUSTER_SIZE - 1);

	// Once the pointer to the correct address cluster is found, if any,
	// check the presence of the specific address

	entry = &cluster->cache[address_idx];
	if(entry->address != NULL) {
		entry->touches++;
		return true;
	}

	// Otherwise, if the pointer to the cluster entry is empty, this means that 'address'
	// has not been yet referenced and must be set for the first time at the indexed cell
	entry->address = address;
	entry->touches = 1;
	cluster->count++;


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


	// Check whether it is the case to switch reversing strategy
	// according to the internal fragemntation of addresses
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


void revwin_create(unsigned int tid) {
	void *address;
	size_t size;
	revwin *win;

	unsigned char code_closing[2] = {0x58, 0xc3};

	// Set the current window as the one registred
	win = rsalloc(sizeof(revwin));
	if(win == NULL) {
		perror("Failed to allocate reverse window descriptor\n");
		abort();
	}
	memset(win, 0, sizeof(revwin));

	// Registers the reverse window in the global structure
	map->map[tid] = win;

	// Initializes reverse window fields
	size = REVWIN_SIZE;
	address = (void *)((char *)map->address + (size * tid));

	win->size = size;
	win->base = address;
	win->top = (void *)((char *)address + size - 1);

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
	win->estack = umalloc(current_lp, EMULATED_STACK_SIZE);
	if(win->estack == NULL) {
		perror("Failed to allocate the emulated stack window\n");
		abort();
	}
	// The stack grows towards low addresses
	win->estack += (EMULATED_STACK_SIZE - 1);

	printf("Reverse window of thread %d has been initialized :: base at %p top at %p, %ld bytes\n", tid, win->base, win->top, win->size);
}



void revwin_free(void) {
	// TODO: implementare
}


/**
 * Initializes a the memory management of areas which contain the reverse windows.
 * Each reverse window is statically allocated once for the whole executions.
 *
 * @author Davide Cingolani
 *
 * @param num_thread The number of the threads for which to allocate the structure
 */
void reverse_init(unsigned int num_threads, size_t revwin_size) {
	size_t size_struct;
	unsigned int lid;
	void *address;
	revwin *win;
	
	if(num_threads == 0) {
		printf("Error initializing the reverse memory management system: number of threads cannot be zero\n");
		abort();
	}

	// Allocates a number of reverse window descriptor for each thread
	size_struct = sizeof(revwin_mmap) + (sizeof(void *) * num_threads);
	map = rsalloc(size_struct);
	if(map == NULL) {
		perror("Error initializing the reverse memory management system\n");
		abort();
	}
	memset(map, 0, size_struct);

	if(revwin_size == 0)
		revwin_size = REVWIN_SIZE;

	// Initializes metadata and mmap the whole reverse area
	map->size = revwin_size * num_threads;
	map->size_self = size_struct;
	map->address = mmap(NULL, map->size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	/*//Initializes each thread's revwin
	for(lid = 0; lid < num_threads; lid++) {
		// win = malloc(sizeof(revwin));
		// if(win == NULL) {
		// 	perror("Failed to allocate reverse window descriptor\n");
		// 	abort();
		// }
		// memset(win, 0, sizeof(revwin));

		// map->map[lid] = win;

		address = (void *)((char *)map->address + (revwin_size * lid));

		// win->base = address;
		// win->top = address + revwin_size - 1;
		// win->size = revwin_size;
		revwin_init(lid, address, revwin_size);
	}*/

	//printf("Reverse memory manager has been initialized: start address %p, end address = %p, size=%ld\n",
	//	map->address, (void *)((char *)map->address+map->size), map->size);
}


void reverse_fini(void) {
	unsigned int lid;

	munmap(map->address, map->size);

	/*for(lid = 0; lid < n_cores; lid++) {
		free(map->map[lid]);
	}*/
}


/*
 * Reset the reverse window intruction pointer
 */
void revwin_reset(void) {
	revwin *win;

	win = current_win;

	if (win == NULL) {
		printf("Error: null reverse window\n");
		abort();
	}

	// Resets the instruction pointer to the first byte AFTER the colsing
	// instruction at the base of the window
	win->top = (void *)(((char *)win->base) + win->size - 3);
	revsed = 0;

	// Reset the cache
	flush_cache();
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
 // TODO: rinominare: update_undo_event
void reverse_code_generator(const void *address, const size_t size) {
	void *chunk_address;
	size_t chunk_size;
	malloc_area *area;
	bool dominant;

	
	//printf("[%d] :: reverse_code_generator(%p, %ld) => %lx\n", tid, address, size, *((unsigned long *)address));

	timer t;
	timer_start(t);

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

	dominant = check_dominance(address);

	// Check whether 'address' is dominant or not, if this is the case
	// does not generate any reversing instruction
	if(dominant) {
		return;
	}

	// int blocks;
	// blocks = compute_span(area);
	// printf("In malloc_area at %p, %d bytes have been reversed so far\n", area, blocks);

	switch (strategy.current){
		case STRATEGY_CHUNK:
			// Reverse the whole malloc_area chunk passing the pointer
			// of the target memory chunk to reverse (not the malloc_area one)
			chunk_address = (unsigned long long)address & ADDRESS_PREFIX;
			chunk_size = CLUSTER_SIZE;
			reverse_chunk(chunk_address, chunk_size);
			//cache.reverse_bytes += chunk_size;
			break;

		case STRATEGY_SINGLE:
			// Reverse the single buffer access
			reverse_single(address, size);
			//cache.reverse_bytes += size;
			break;
	}

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
void execute_undo_event(void) {
	unsigned char push = 0x50;
	revwin *win = current_win;
	void *revcode;

	timer reverse_block_timer;

	timer_start(reverse_block_timer);

	// Add the complementary push %rax instruction to the top
	revwin_add_code(win, &push, sizeof(push));

	// Temporary swaps the stack pointer to use
	// the emulated one on the heap, instead

	// TODO: move to define
	// Save the original stack pointer into 'original_stack'
	// and substitute $RSP the new emulated stack pointer
	if (win->estack == NULL) {
		printf("[%d] :: Emulated stack NULL!\n", tid);
		abort();
	}
	__asm__ volatile ("movq %%rsp, %0\n\t"
					  "movq %1, %%rsp" : "=m" (win->orig_stack) : "m" (win->estack));


	// Calls the reversing function
	revcode = win->top;
	((void (*)(void))revcode) ();

	printf("===> [%d] :: undo event executed\n", tid);

	// Replace the original stack on the relative register
	__asm__ volatile ("movq %0, %%rsp" : : "m" (win->orig_stack));

	double elapsed = (double)timer_value_micro(reverse_block_timer);


	// FIXME: mi sa che qui current_lp non ècorrettamente impostato, perché viene impostato solo
	// durante la forward execution
	stat_post_lp_data(current_lp, STAT_REVERSE_EXECUTE, 1.0);
	stat_post_lp_data(current_lp, STAT_REVERSE_EXECUTE_TIME, elapsed);

	//undo_exe_time[tid] += elapsed;
	//undo_exe_count++;
	//reverse_execution_time += elapsed;
	//reverse_block_executed++;

	// Reset the reverse window
	//reset_window(w);
}

#endif /* HAVE_REVERSE */


