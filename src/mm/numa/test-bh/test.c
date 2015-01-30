#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include <unistd.h>
#include <pthread.h>

char* v[1024*PAGE_SIZE];
char* addrs[1024];

char msg[128];

int glob_sobj;
pthread_t tid;

void* GetThread(voi){	
	char* addr;
while (1){
	addr = get_BH(glob_sobj);
	if(addr) printf("%s",addr);
	else {
		printf("got null\n");
		break;
	}
}
	return addr;
}

int main(int argc, char** argv){

	int totsobjs;
	int sobj;
	int size;
	int batch;
	int ret;
	int i,j;
	int cycles;
	
	char* addr;

	if (argc < 6) {
		printf("bad number of arguments - usage: totsobjs sobs size(this is in 4KB - page sized)) batch cycles\n");
		return -1;
	}


	totsobjs = strtol(argv[1],NULL,10);
	sobj = strtol(argv[2],NULL,10);
	size = strtol(argv[3],NULL,10);
	batch = strtol(argv[4],NULL,10);
	cycles = strtol(argv[5],NULL,10);
	printf("params audi: totsobjs %d - sobj %d - size %d - batch %d - cycles %d\n",totsobjs,sobj,size,batch,cycles);

	ret = init_allocator(totsobjs);
	printf("INIT returned code %d\n",ret);

	audit();

	addr = (char*)allocate_segment(sobj,size*PAGE_SIZE);

	printf("segment allocation for sobj %d returned address %p\n",sobj,addr);

	memset(addr,'f',size*PAGE_SIZE);

	move_request(sobj,0);
	
	sleep(2);

	glob_sobj = sobj;
	pthread_create(&tid, NULL, GetThread, NULL);	
	pthread_create(&tid, NULL, GetThread, NULL);	

	for (i=0; i<cycles; i++){

		sprintf(msg,"round message %i\n",i);

		for(j=0;j<batch;j++)
		insert_BH(sobj,msg,strlen(msg)+1);

	

/*
		while (1){
			addr = get_BH(sobj);
			if(addr) printf("%s",addr);
			else {
				printf("got null\n");
				break;
			}
		}
*/
	}


	sleep(1);
	return 0;
}
