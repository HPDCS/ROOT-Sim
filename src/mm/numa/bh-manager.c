#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "allocator.h"
#include <numaif.h>
#include <errno.h>
#include <pthread.h>
#include "bh-manager.h"


mem_map * bhmaps;
char fictitious[MAX_MSG_SIZE];

extern int handled_sobjs;

void set_BH_map(mem_map* argA ){
        bhmaps = argA;
}

int init_BH(){
        
        int i;
        char* addr;
        int ret;
        int sobjs;

        if(handled_sobjs < 0) return INVALID_SOBJS_COUNT; 

        sobjs = handled_sobjs;


        for (i=0; i<sobjs; i++){
                bhmaps[i].live_msgs = 0;
                bhmaps[i].live_boundary = 0;
                bhmaps[i].expired_msgs = 0;
                bhmaps[i].expired_offset = 0;
                bhmaps[i].expired_boundary = 0;
        }
       

        for (i=0; i<sobjs; i++){
                addr = allocate_page();
                if (addr == NULL) goto bad_init;
                bhmaps[i].live_bh = addr;
                addr = allocate_page();
                if (addr == NULL) goto bad_init;
                bhmaps[i].expired_bh = addr;
        }
        
        return SUCCESS;

bad_init:
        return INIT_ERROR; 


}


int insert_BH(int sobj, void* msg, int size){// this needs to be atomic per sobj - synch is left to the upper layer

	int tag;
	int needed_store;
	int residual_store;
	int offset;

	if(bhmaps[sobj].live_boundary >= BH_SIZE) goto bad_insert;

	if( (size<=0) || size > MAX_MSG_SIZE) goto bad_insert;

	if( msg == NULL ) goto bad_insert;

	tag = size;
	needed_store = tag + sizeof(tag);

	residual_store = BH_SIZE - bhmaps[sobj].live_boundary;
	
	if( residual_store < needed_store ) goto bad_insert;

	offset = bhmaps[sobj].live_boundary;

	memcpy(bhmaps[sobj].live_bh + offset, &tag, sizeof(tag));

	offset += sizeof(tag);

	memcpy(bhmaps[sobj].live_bh + offset, msg, size);

	bhmaps[sobj].live_boundary += needed_store;

	bhmaps[sobj].live_msgs += 1;
	
	return SUCCESS;

bad_insert: 

	return FAILURE;

}

void* get_BH(int sobj){// this needs to be atomic per sobj - synch is left to the upper layer

	int msg_tag;
	void* buff;
	void* msg_addr;
	int msg_offset;
	

	if(bhmaps[sobj].expired_msgs <= 0 ) {
	
		switch_bh(sobj);

	}

	
	if(bhmaps[sobj].expired_msgs <= 0 ) goto no_msg;

	msg_offset = bhmaps[sobj].expired_offset;  

	msg_addr = bhmaps[sobj].expired_bh + msg_offset;

	memcpy(&msg_tag,msg_addr,sizeof(msg_tag)); 

	buff = get_buffer(msg_tag);

	msg_addr += sizeof(msg_tag);

	memcpy(buff,msg_addr,msg_tag);

	msg_addr += msg_tag;

	bhmaps[sobj].expired_offset = (char*)msg_addr - bhmaps[sobj].expired_bh;

	bhmaps[sobj].expired_msgs -= 1;

	return buff;

no_msg:
	
	return NULL;	
}

void switch_bh(int sobj){

	char* addr;

	//atomic needed
	bhmaps[sobj].expired_msgs = bhmaps[sobj].live_msgs;
	bhmaps[sobj].expired_offset = bhmaps[sobj].live_offset;
	bhmaps[sobj].expired_boundary = bhmaps[sobj].live_boundary;

	bhmaps[sobj].live_msgs = 0;
	bhmaps[sobj].live_offset = 0;
	bhmaps[sobj].live_boundary = 0;

	addr = bhmaps[sobj].expired_bh; 
	bhmaps[sobj].expired_bh = bhmaps[sobj].live_bh;
	bhmaps[sobj].live_bh = addr;

	return;

}

void* get_buffer(int size){
	
	return fictitious;
}
