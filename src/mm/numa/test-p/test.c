#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include <unistd.h>

char* v[1024*PAGE_SIZE];
char* addrs[1024];

int main(int argc, char** argv){

	int totsobjs;
	int sobj;
	int size;
	int node;
	int ret;
	int i;
	int cycles;
	
	char* addr;

	if (argc < 6) {
		printf("bad number of arguments - usage: totsobjs sobs size(this is in 4KB - page sized)) node cycles\n");
		return -1;
	}


	totsobjs = strtol(argv[1],NULL,10);
	sobj = strtol(argv[2],NULL,10);
	size = strtol(argv[3],NULL,10);
	node = strtol(argv[4],NULL,10);
	cycles = strtol(argv[5],NULL,10);
	printf("params audi: totsobjs %d - sobj %d - size %d - node %d - cycles %d\n",totsobjs,sobj,size,node,cycles);

	ret = init_allocator(totsobjs);
	printf("INIT returned code %d\n",ret);

	audit();

	addr = (char*)allocate_segment(sobj,size*PAGE_SIZE);

	printf("segment allocation for sobj %d returned address %p\n",sobj,addr);

	memset(addr,'f',size*PAGE_SIZE);

	move_request(sobj,node);
	
	sleep(2);	

	for (i=0; i<cycles; i++){

//		memcpy(v,addr, size*PAGE_SIZE);
		memset(addr,'f',size*PAGE_SIZE);

	}

	return 0;
}
