#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include <unistd.h>



int main(int argc, char** argv){

	int sobjs;
	int ret;
	int i,j;
	char* addr;
	int numentries;
	int target;
	int success;
	int failure;

	if (argc < 3) {
		printf("bad number of arguments\n");
		return -1;
	}

	audit();

	sobjs = strtol(argv[1],NULL,10);

	ret = init_allocator(sobjs);
	printf("init allocator returned code: %d\n",ret);

	if(!strcmp(argv[2],"pagesize")){

		for(i=0;i<sobjs;i++){
	 		addr = (char*)allocate_segment(i,PAGE_SIZE);
			printf("page allocation for sobj %d returned address %p\n",i,addr);
		}	

		return 0;
	}

	if(!strcmp(argv[2],"singletable")){

	  target = strtol(argv[3],NULL,10);
	  numentries = strtol(argv[4],NULL,10);
	 
	  for(i=0;i<numentries;i++){
	 	addr = (char*)allocate_segment(target,PAGE_SIZE);
		printf("page allocation %i for sobj %d returned address %p\n",i,target,addr);
          }	
	  
		return 0;
	}

	if(!strcmp(argv[2],"largesegments")){

	  target = strtol(argv[3],NULL,10);
	  numentries = strtol(argv[4],NULL,10);

	  success=0;
	  failure=0;
	 
	  for(i=0;i<numentries;i++){
	 	addr = (char*)allocate_segment(target,i*PAGE_SIZE);
		printf("segment allocation %i for sobj %d asking for %d bytes returned address %p\n",i,target,i*PAGE_SIZE,addr);
		if(addr != NULL){
			success++;
			printf("trying to memset ..\n");
			memset(addr,'f',i*PAGE_SIZE);
			printf("memset done\n");
		}
		else{
			failure++;
		}
          }	
	  
	  printf("total allocations are %d - failures are %d - successses are %d\n",numentries,failure,success);
		return 0;
	}

	if(!strcmp(argv[2],"largesegmentsall")){

	  //target = strtol(argv[3],NULL,10);
	  numentries = strtol(argv[3],NULL,10);

	  success=0;
	  failure=0;
	
	for (j=0;j<sobjs;j++){ 

	  success=0;
	  failure=0;

	  for(i=0;i<numentries;i++){
	 	addr = (char*)allocate_segment(j,i*PAGE_SIZE);
		printf("segment allocation %i for sobj %d asking for %d bytes returned address %p\n",i,j,i*PAGE_SIZE,addr);
		if(addr == NULL){//retry
			usleep(1000);
	 		addr = (char*)allocate_segment(j,i*PAGE_SIZE);
		}
		if(addr != NULL){
			success++;
			printf("trying to memset ..\n");
			memset(addr,'f',i*PAGE_SIZE);
			printf("memset done\n");
		}
		else{
			failure++;
		}
          }	
	  
	  printf("total allocations for sobj %d are %d - failures are %d - successses are %d\n",j,numentries,failure,success);
	}
		return 0;
	}

	if(!strcmp(argv[2],"final")){

	  //target = strtol(argv[3],NULL,10);
	  numentries = strtol(argv[3],NULL,10);

	  success=0;
	  failure=0;
	
	for (j=0;j<sobjs;j++){ 

	  success=0;
	  failure=0;

	  for(i=0;i<numentries;i++){
	 	addr = (char*)allocate_segment(j,(i*PAGE_SIZE)+1);
		printf("segment allocation %i for sobj %d asking for %d bytes returned address %p\n",i,j,(i*PAGE_SIZE)+1,addr);
		if(addr == NULL){//retry
			usleep(1000);
	 		addr = (char*)allocate_segment(j,(i*PAGE_SIZE)+1);
		}
		if(addr != NULL){
			success++;
			printf("trying to memset ..\n");
			memset(addr,'f',(i*(PAGE_SIZE))+1);
			printf("memset done\n");
		}
		else{
			failure++;
		}
	 } 

	  printf("total allocations for sobj %d are %d - failures are %d - successses are %d\n",j,numentries,failure,success);
	  audit_map(j);

	}
	return 0;

	}
}
