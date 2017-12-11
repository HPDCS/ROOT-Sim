/**
*			Copyright (C) 2008-2017 HPDCS Group
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
* @file segment.c
* @brief Segment Allocator
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*/

#include <mm/mm.h>

#include <mm/ecs.h>
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>
#include <fcntl.h>
#include <sys/types.h>

//TODO: document this magic! This is related to the pml4 index intialized in the ECS kernel module
static unsigned char *init_address = (unsigned char *)(10LL << 39);

void *get_base_pointer(unsigned int gid){
//	printf("get base pointer for lid % d (gid %d) returns: %p\n",GidToLid(gid),gid,init_address + PER_LP_PREALLOCATED_MEMORY * gid);
	return init_address + PER_LP_PREALLOCATED_MEMORY * gid;
}

void *get_segment(unsigned int gid) {
	int i;
	void *the_address;

	void *mmapped[NUM_MMAP];

	// Addresses are determined in the same way across all kernel instances
	the_address = init_address + PER_LP_PREALLOCATED_MEMORY * gid;

	for(i = 0; i < NUM_MMAP; i++) {
		mmapped[i] = mmap(the_address, MAX_MMAP, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, 0, 0);
		if(mmapped[i] == MAP_FAILED) {
			rootsim_error(true, "Unable to mmap LPs memory\n");
			return NULL;
		}
/*		if(i%2 == 0) {
			printf("base pointer of gid %d on kernel %d is %p\n", gid, kid, mmapped[i]);
		}
*/		// Access the memory in write mode to force the kernel to create the page table entries
		*((char *)mmapped[i]) = 'x';
		the_address = (char *)the_address + MAX_MMAP;
	}

	return mmapped[0];
}

/*
 * TODO: this should reconstruct the addresses similarly to what is done in get_segment. Anyhow, this is called at simulation shutdown and doesn't cause much harm if it's not called.
 */
/*
 * void segment_allocator_fini(unsigned int sobjs){
	unsigned int i;
	int return_value;
	for(i=0;i<sobjs;i++){
		return_value = munmap(mem_region[i].base_pointer,PER_LP_PREALLOCATED_MEMORY);
		if(return_value){
			printf("ERROR on release value:%d\n",return_value);
			break;
		}
		mem_region[i].base_pointer = NULL;
	}
	close(ioctl_fd);

}
*/

