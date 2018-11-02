/**
*			Copyright (C) 2008-2018 HPDCS Group
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

#include <sys/time.h>
#include <sys/resource.h>

#include <mm/mm.h>
#include <mm/ecs.h>
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>
#include <fcntl.h>
#include <sys/types.h>

size_t __page_size = -1;

//TODO: document this magic! This is related to the pml4 index intialized in the ECS kernel module
static unsigned char *init_address = (unsigned char *)(10LL << 39);

void *get_base_pointer(GID_t gid){
//	printf("get base pointer for lid % d (gid %d) returns: %p\n",GidToLid(gid),gid,init_address + PER_LP_PREALLOCATED_MEMORY * gid);
	return init_address + PER_LP_PREALLOCATED_MEMORY * gid_to_int(gid);
}

void *get_segment(GID_t gid) {
	void *the_address;
	void *mapped;

	// Addresses are determined in the same way across all kernel instances
	the_address = init_address + PER_LP_PREALLOCATED_MEMORY * gid_to_int(gid);

	mapped = mmap(the_address, PER_LP_PREALLOCATED_MEMORY, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, 0, 0);
	if(unlikely(mapped == MAP_FAILED)) {
		rootsim_error(true, "Unable to mmap LPs memory\n");
		return NULL;
	}
	// Access the memory in write mode to force the kernel to create the page table entries
	*((char *)mapped) = 'x';

	return mapped;
}

void segment_init(void) {
	struct rlimit limit;
	size_t max_address_space = PER_LP_PREALLOCATED_MEMORY * n_prc_tot * 2;

	limit.rlim_cur = max_address_space;
	limit.rlim_max = max_address_space;
  
	if (setrlimit(RLIMIT_AS, &limit) != 0) {
		perror("Unable to set the maximum address space");
		rootsim_error(true, "Unable to pre-allocate per-LP memory. Aborting...\n");
	}
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
		if(unlikely(return_value)) {
			printf("ERROR on release value:%d\n",return_value);
			break;
		}
		mem_region[i].base_pointer = NULL;
	}
	close(ioctl_fd);

}
*/

