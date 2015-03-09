/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file reverse.c
* @brief This module implements the runtime generation of undo events
* @author Davide Cingolani
*/


#ifdef HAVE_MIXED_SS


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>


#define REVERSE_WIN_SIZE 1024 * 1024 * 10	//! Defalut size of the reverse window which will contain the reverse code
#define HMAP_SIZE		32768				//! Default size ot the address hash map to handle colliding mov addresses

#define HMAP_INDEX_MASK		0xffffffc0		//! Most significant 10 bits are used to index quad-word which contains address bit
#define HMAP_OFFSET_MASK	0x3f			//! Least significant 6 bits are used to intercept address presence
#define HMAP_OFF_MASK_SIZE	6

// TODO: move to header?
typedef struct _revwin {
	int size;			//! The actual size of the reverse window
	int flags;			//! Creation flags
	int prot;			//! Creation protection flags
	void *address;		//! Address to which it resides
	void *pointer;		//! Pointer to the new actual free address memory location
} revwin;


// Hash map to handle repeted MOV-targeted addresses
typedef struct _addrmap {
	unsigned long long map[HMAP_SIZE];
} addrmap;


// History of all the reverse windows allocated since the first execution
typedef struct _eras {
	revwin *era[1024];	//! Array of the windows
//	int size;			//! Current size of the history
	int last_free;		//! Index of the last available slot
} eras;


static int timestamp = 0;		//! This is the counter used to infer which instructions have to be reversed
static int current_era = -1;	//! Represents the current era to which the reverse heap refers to
static int last_era = -1;		//! Specifies the last era index. It is initialized to 1 in order to first create the window
static addrmap hashmap;			//! Map of the referenced addresses
static eras history;			//! Collects the reverse windows along the eras
static revwin *window;			//! Represents the pointer to the current active reverse window




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
static inline revwin *allocate_reverse_window (size_t size) {

//	printf("chiamo allocate_reverse\n");
	window = malloc(sizeof(revwin));

	window->size = size;
	window->address = mmap(NULL, size, PROT_EXEC | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	window->pointer = window->address + size;

//	printf("mmap returned %p\n", window->address);
//	fflush(stdout);

	if(window->address == MAP_FAILED) {
		perror("mmap failed");
		abort();
	}

	// TODO: move it out?
	bzero(&hashmap.map, sizeof(hashmap.map));


	// TODO: modificare la gestione di last_free come negli unix file descriptor
	// check if the entry is empty, if this is the case then fill it up with the new
	// window, otherwise it aborts. Note: the last free slot pointer will be incremented

	// check if space is enough, otherwise it aborts
	if(history.last_free >= sizeof(history.era)){
		perror("too much eras");
		abort();
	}

	if(history.era[history.last_free] == NULL){
		history.era[history.last_free] = window;
		history.last_free++;
	} else {
		perror("unable to get era's descriptor");
		abort();
	}


	return window;
}


/**
 * Writes the reverse instruction passed on the heap reverse window.
 *
 * @param bytes Pointer to the memory region containing the instruction's bytes
 * @param size Instruction's size (in bytes)
 */
static inline void add_reverse_insn (char *bytes, size_t size) {

	// since the structure is used as a stack, it is needed to create room for the instruction
	window->pointer -= size;

	// printf("=> Attempt to write on the window heap at <%#08lx> (%d bytes)\n", window->pointer, size);

	// TODO: probably too expensive, only for debug!
	if (window->pointer < window->address) {
		perror("insufficent reverse window memory heap!");
		exit(-ENOMEM);
	}

	// TODO: add timestamp conditional selector

	// copy the instructions to the heap
	memcpy(window->pointer, bytes, size);
}

/**
 * Private function used to create the new reversed MOV instruction and
 * manipulate properly the additional information (i.e the timestamp)
 * accordingly to maintain consistency in the reverse code.
 *
 * The timestamp is used to jump the uneeded instructions, hence the ones
 * which have not to be reversed.
 *
 * The revesre instructions have to be written into the reverse window structure
 * in reverse order, too. That is, starting from the bottom of the structure.
 * In this way, it is simply needed to call a certain instruction address to
 * reverse all the subsequant instructions using the IP natural increment.
 *
 * @param value Tha immediate value has to be emdedded into the instruction
 * @param size The size of the data value emdedded
 */
static inline void create_reverse_instruction (uint64_t value, size_t size) {
	char mov[8];		// MOV instruction bytes (at most 9 bytes)
	char mov2[8];		// second MOV in case of quadword data
	char jc[2];			// conditional JUMP to skip uneeded instructions
	char cmp[7];		// compare instruction with the timestamp
//	int flags = 0;			// TODO: are they necessary? in case move to param
	size_t mov_size;

	// create the new MOV instruction accordingly to the data size
	mov[0] = (uint64_t) 0;
	switch(size) {
		case 1:	// BYTE
			// this is the case of the movb: c6 07 00
			mov[0] = 0xc6;
			mov[1] = 0x07;
			mov[2] = (uint8_t) value;
			mov_size = 3;
			break;

		case 2:	// WORD
			// this is the case of the movw: 66 c7 07 00 00
			mov[0] = 0x66;
			mov[1] = 0xc7;
			mov[2] = 0x07;
			mov[3] = (uint16_t) value;
			mov_size = 5;
			break;

		case 4:	// LONG-WORD
			// this is the case of the movl: c7 07 00 00 00 00
			mov[0] = 0xc7;
			mov[1] = 0x07;
			mov[2] = (uint32_t) value;
			mov_size = 6;
			break;

		case 8:	// QUAD-WORD
			// this is the case of the movq, however in such a case the immediate cannot be
			// more than 32 bits width; therefore we need two successive mov instructions
			// to transfer the upper and bottom part of the value through the 32 bits immediate
			// movq: 48 c7 47 00 00 00 00 00
			mov[0] = mov2[0] = 0x48;
			mov[1] = mov2[1] = 0xc7;
			mov[2] = mov2[2] = 0x47;
			mov[3] = mov2[3] = 0x00;
			mov[4] = (uint32_t) value;

			// second part
			mov2[4] = (uint32_t) (value >> 32);
			mov_size = 8;
			break;

		default:
			// mmmm....
			return;
		}

	// generate the compare-jump couple instructions to address the issue
	// of reverse accordingly to a timestamp specification
	/*if (flags) {
		cmp[0] = 0x48;		// 64-bit prefix
		cmp[1] = 0x83;		// opcode for the cmp imm32, reg64
		cmp[2] = 0xf9;		// reg is rcx (the idea is to use timestamp as paramter
		memcpy(cmp+3, &timestamp, sizeof(timestamp));	// actual timestamp to compare with (autoincrement)
		timestamp++;

		jc[0] = 0x74;		// JZ opcode with imm8
		jc[1] = 0x00;		// displacement to the next instruction TODO: mhhh.. i don't know actually
	}*/

	// now 'mov' contains the bytes that represents the reverse MOV
	// hence it has to be written on the heap reverse window
	add_reverse_insn(mov, mov_size);
	if (size == 8) {
		add_reverse_insn(mov2, mov_size);
	}
}

void dump_revwin () {
	// print the heap in order to report reverse instrucionts
	printf("=> Dump of the current reverse heap window [%d]:\n", last_era);
	printf("\n");
}

/**
 * Check if the address is dirty by looking at the hash map. In case the address
 * is not present adds it and return 0.
 *
 * @param address The address to check
 *
 * @return 1 if the address is present, 0 otherwise
 */
static inline int is_address_referenced (void *address) {
	int index;
	char offset;

	// TODO: how to handle full map?

	index = ((unsigned long long)address & HMAP_INDEX_MASK) >> HMAP_OFF_MASK_SIZE;
	offset = (unsigned long long)address & HMAP_OFFSET_MASK;

	// Closure
	index %= HMAP_SIZE;
	offset %= 64;

	// if the address is not reference yet, insert it and return 0 (false)
	if (!hashmap.map[index] >> offset) {
		hashmap.map[index] |= (1 << offset);
		return 0;
	}

	return 1;
}


/**
 * Genereate the reverse MOV instruction staring from the knowledge of which
 * memory address will be accessed and the data width of the write.
 *
 * @param address The pointer to the memeory location which the MOV refers to
 * @param size The size of data will be written by MOV
 */
void reverse_code_generator (void *address, unsigned int size) {
	uint64_t value;

	printf("\n=== Reverse code generator ===\n");

	// check if the address is already be referenced, in that case it is not necessary
	// to compute again the reverse instruction, since the former MOV is the one
	// would be restored in future, therefore the subsequent ones are redundant (at least
	// for now...)
	if (is_address_referenced(address)) {
		return;
	}

	value = 0;
	switch(size) {
		case 1:
			value = *((uint8_t *) address);
			break;

		case 2:
			value = *((uint16_t *) address);
			break;

		case 4:
			value = *((uint32_t *) address);
			break;

		case 8:
			value = *((uint64_t *) address);
			break;
	}
//	memcpy(&value, address, size);


	// now the idea is to generate the reverse MOV instruction that will
	// restore the previous value 'value' stored in the memory address
	// based on the operand size selects the proper MOV instruction bytes
	create_reverse_instruction(value, size);

	//dump_revwin();

	//~ printf("==============================\n\n");
}

inline void trampoline_initialize () {
	if(current_era > 0) return;

	//history.size = sizeof(history.era);
	window->prot = PROT_EXEC | PROT_READ;		//TODO: change into WRITE, add the on-the-fly EXEC switch mechanism
	window->flags = MAP_PRIVATE | MAP_ANONYMOUS;

	//history.size = sizeof(history.era);
	increment_era();
}


inline int increment_era () {

	last_era++;
	allocate_reverse_window(REVERSE_WIN_SIZE);

	return last_era;
}


inline void free_last_revwin () {
	revwin *window;

	history.last_free--;
	window = history.era[history.last_free];
	munmap(window->address, window->size);
	history.era[history.last_free] = NULL;
}

#endif /* HAVE_MIXED_SS */


