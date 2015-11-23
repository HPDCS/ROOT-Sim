#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <mm/allocator_ecs.h>

#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_CROSS_STATE
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>
#endif

lp_mem_region mem_region[MAX_LPs];
static int ioctl_fd;

void* get_base_pointer(unsigned int sobj){
	return (void*) mem_region[sobj].base_pointer;
}

char* get_memory_ecs(unsigned int sobj, size_t size){
	lp_mem_region local_mem_region = mem_region[sobj];	

	if((local_mem_region.brk + size) - local_mem_region.base_pointer > PER_LP_PREALLOCATED_MEMORY ){
		printf("Error no enough memory");
		//TODO MN future managment of more memory
		return MAP_FAILED;
	}
	
	char* return_value = local_mem_region.brk;
	local_mem_region.brk += size;
	return return_value;
}

int allocator_ecs_init(unsigned int sobjs) {
        unsigned int i,y;
        char* addr;
	unsigned long init_addr;
	void** pml4;
	unsigned int num_mmap = sobjs * 2;
	size_t size = PER_LP_PREALLOCATED_MEMORY / 2;

	ioctl_fd = open("/dev/ktblmgr", O_RDONLY);
        if (ioctl_fd == -1) {
                rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
        }

	//ioctl(ioctl_fd, IOCTL_PGD_PRINT);

	if( (sobjs > MAX_LPs) )
                return INVALID_SOBJS_COUNT_AECS;

	y=0;
	while(y<num_mmap){
		int pml4_index = ioctl(ioctl_fd, IOCTL_GET_FREE_PML4);
		//printf("Value of pml4_index: %d\n",pml4_index);
		init_addr =(ulong) pml4_index;
		//printf("Value of initial_add_before_shift = %p\n",init_addr);
		init_addr = init_addr << 39;
		//printf("Value of convert = %p\n",PML4(init_addr));
		//printf("Value of initial_add = %p\n",init_addr);
		
		fflush(stdout);
		int allocation_counter = 0;

		for(; y < num_mmap; y++) {
		
			// map memory
			addr = mmap((void*)init_addr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,0,0);
			// Access the memory in write mode to force the kernel to create the page table entries
			addr[0] = 'x';
			addr[1] = addr[0];
		
			// Keep track of the per-LP allocated memory
			if(y % 2 == 0) {
				//printf("y=%d\n",y);
				mem_region[y/2].base_pointer = mem_region[y/2].brk = addr;
			}
				
			allocation_counter++;
			
			//printf("Y: %d allocation_counter: %d\n",y,allocation_counter);
			
			if(allocation_counter == (512*2)){
			 	y++; 
				break;
			}
			
			
			init_addr += size;
		}
			
		
		
	}
	
	
	//ioctl(ioctl_fd, IOCTL_PGD_PRINT);
	
	return SUCCESS_AECS;
}

void allocator_ecs_release(unsigned int sobjs){
	unsigned int i;
	int return_value;
	for(i=0;i<sobjs;i++){
		return_value = munmap(mem_region[i].base_pointer,PER_LP_PREALLOCATED_MEMORY);
		if(!return_value){	
			printf("ERROR on release value:%d\n",return_value);
			break;
		}
		mem_region[i].base_pointer = mem_region[i].brk = NULL;
	}

}


