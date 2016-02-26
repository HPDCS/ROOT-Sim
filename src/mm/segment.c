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
* @file segment.c
* @brief Segment Allocator
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*/


#include <mm/mm.h>

#ifdef HAVE_CROSS_STATE

static lp_mem_region mem_region[MAX_LPs];
static int ioctl_fd;

void *get_base_pointer(unsigned int sobj) {
	return (void*) mem_region[sobj].base_pointer;
}

char *get_memory_ecs(unsigned int sobj, size_t size) {
	lp_mem_region local_mem_region = mem_region[sobj];

	if((local_mem_region.brk + size) - local_mem_region.base_pointer > PER_LP_PREALLOCATED_MEMORY) {
		printf("Error no enough memory");
		//TODO MN future managment of more memory
		return MAP_FAILED;
	}

	char* return_value = local_mem_region.brk;
	local_mem_region.brk += size;
	mem_region[sobj] = local_mem_region;
	return return_value;
}

int allocator_ecs_init(unsigned int sobjs) {
	unsigned int y;
	char* addr;
	unsigned long init_addr;
	unsigned int num_mmap = sobjs * 2;
	size_t size = PER_LP_PREALLOCATED_MEMORY / 2;
	int allocation_counter, pml4_index;

	ioctl_fd = open("/dev/ktblmgr", O_RDONLY);
	if (ioctl_fd == -1) {
			rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
	}

	//ioctl(ioctl_fd, IOCTL_PGD_PRINT);

	if( (sobjs > MAX_LPs) )
                return INVALID_SOBJS_COUNT_AECS;

	y=0;
//	pml4_index = 9;
	while(y<num_mmap){
		pml4_index = ioctl(ioctl_fd, IOCTL_GET_FREE_PML4);
//		pml4_index++;
		init_addr =(ulong) pml4_index;
		init_addr = init_addr << 39;
		allocation_counter = 0;

		printf("pml4_idx: %d\n", pml4_index);

		for(; y < num_mmap; y++) {

			// map memory
			addr = mmap((void*)init_addr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,0,0);
			if(addr == MAP_FAILED) {
				printf("NOOOOOOOOOOOOOOOOOOOOOOOOO!\n");
				abort();
			}
			// Access the memory in write mode to force the kernel to create the page table entries
			addr[0] = 'x';
			addr[1] = addr[0];

			// Keep track of the per-LP allocated memory
			if(y % 2 == 0){
				mem_region[y/2].base_pointer = mem_region[y/2].brk = addr;
//				printf("LP[%d] address:%p\n",y/2,addr);
			}

			allocation_counter++;

			if(allocation_counter == (512*2)){
				y++;
				break;
			}


			init_addr += size;
		}



	}

	return SUCCESS_AECS;
}

void allocator_ecs_fini(unsigned int sobjs){
	unsigned int i;
	int return_value;
	for(i=0;i<sobjs;i++){
		return_value = munmap(mem_region[i].base_pointer,PER_LP_PREALLOCATED_MEMORY);
		if(return_value){
			printf("ERROR on release value:%d\n",return_value);
			break;
		}
		mem_region[i].base_pointer = mem_region[i].brk = NULL;
	}
	close(ioctl_fd);

}

#endif /* HAVE_CROSS_STATE */
