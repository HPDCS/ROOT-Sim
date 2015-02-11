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
* @file mapmove.c
* @brief 
* @author Francesco Quaglia
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "allocator.h"
#include <numaif.h>
#include <errno.h>
#include <pthread.h>
#include "mapmove.h"

#define AUDIT if(1)

/* these are required for move_pages - we use max-sized static memory for latency optimization */
char*      pages[MAX_SEGMENT_SIZE];
unsigned   nodes[MAX_SEGMENT_SIZE];
int        status[MAX_SEGMENT_SIZE];



extern int handled_sobjs;
mem_map* daemonmaps;
map_move* daemonmoves;

int __thread lastlocked;

void set_daemon_maps(mem_map* argA, map_move* argB){
	daemonmaps = argA;
	daemonmoves = argB;
}


int init_move(int sobjs){
	
	int i;
        char* addr;
	int ret;
	pthread_t daemon_tid;

        if( (sobjs <= 0)||(sobjs>MAX_SOBJS) ) return INVALID_SOBJS_COUNT; 


        for (i=0; i<sobjs; i++){
		pthread_spin_init(&(daemonmoves[i].spinlock),0);
		daemonmoves[i].need_move = 0;
		daemonmoves[i].target_node = 0;
        }

	ret = pthread_create(&daemon_tid, NULL, background_work, NULL);	

	if( ret ) goto bad_init;
        
        return SUCCESS;

bad_init:

        return INIT_ERROR; 
}


void move_sobj(int sobj, unsigned numa_node){

	mem_map* mmap;
        mdt_entry* mdte;
        int i;
	int pagecount;
	int j;
	char* segment_addr;
	int k;
	int retry, alreadytried;
	int totpages;
	int ret;
                
	AUDIT
	printf("audit on maps: %p - handled objs is %d\n",daemonmaps,handled_sobjs);

        if( (sobj < 0)||(sobj>=handled_sobjs) ){
                printf("move request on invalid sobj\n");
                return ; 
        }

        mmap = &daemonmaps[sobj]; 

	k = 0;
	totpages = 0;

        for(i=0;i<mmap->active;i++){
                mdte = (mdt_entry*)mmap->base + i;
                printf("moving segment in mdt entry %d - content: addr is %p - num pages is %d\n",i,mdte->addr,mdte->numpages);

		move_segment(mdte,numa_node);
        }

	move_BH(sobj,numa_node);

        return ;
}




void move_segment(mdt_entry *mdte, unsigned numa_node){
	int pagecount;
	int i;
	char* segment_addr;
	int ret;
	int retry, alreadytried;


	retry =0;
	alreadytried= 0;

	pagecount = mdte->numpages;
	segment_addr = mdte->addr;

	for (i=0;i<pagecount;i++){
		pages[i] = segment_addr + i*PAGE_SIZE; 
		nodes[i] = numa_node;
	}	

move_operation:
	ret = numa_move_pages(0, pagecount, pages, nodes, status, MPOL_MF_MOVE);
	printf("PAGE MOVE operation (page count is %d target node is %u) returned %d\n",pagecount,numa_node,ret);

/*
	for (i=0;i<pagecount;i++){
		printf("     details: status[%i] is %d\n",i,status[i]);
		if (status[i] != numa_node) retry = 1;
	}	

	if (retry && !alreadytried){
		alreadytried = 1;
		 goto move_operation;
	}
*/

}


void move_BH(int sobj, unsigned numa_node){

	char**pages;
	int i;
	int ret;
	int pagecount = 2; //1 page per bh (live and expired) 

	pages = daemonmaps[sobj].actual_bh_addresses;

	for (i=0;i<pagecount;i++){
		nodes[i] = numa_node;
	}	

	ret = numa_move_pages(0, pagecount, pages, nodes, status, MPOL_MF_MOVE);

	AUDIT
	printf("HB move - sobj %d - return value is %d\n",sobj,ret);

}

int lock(int sobj){

        if( (sobj < 0)||(sobj>=handled_sobjs) ) goto bad_lock; 

	pthread_spin_lock(&(daemonmoves[sobj].spinlock));

	lastlocked = sobj;

	return SUCCESS;

bad_lock:

	return FAILURE;
}

int unlock(int sobj){

        if( (sobj < 0)||(sobj>=handled_sobjs) ) goto bad_unlock; 

	if( sobj != lastlocked ) goto bad_unlock;

	pthread_spin_unlock(&(daemonmoves[sobj].spinlock));

	return SUCCESS;

bad_unlock:

	return FAILURE;
}

void * background_work(void * dummy){

	int sobj;
	int node;

	while(1){

		sleep(SLEEP_PERIOD);
		AUDIT
		printf("RS numa daemon wakeup\n");


		for(sobj=0; sobj<handled_sobjs; sobj++){

			lock(sobj);
			node = verify(sobj);
			if(unlikelynew(node)) move_sobj(sobj,node);
			unlock(sobj);

	 	}	
	}

}
 int verify(int sobj){

	int node;

	if(daemonmoves[sobj].need_move == 1){
		node = daemonmoves[sobj].target_node;
		daemonmoves[sobj].need_move = 0;
		return node;
	}
	else{
		return -1;
	}
}

int move_request(int sobj, int numa_node){


        if( (sobj < 0)||(sobj>=MAX_SOBJS) ) return FAILURE;

        if( (numa_node < 0)||(numa_node>=NUMA_NODES) ) return FAILURE;

	lock(sobj);
	daemonmoves[sobj].need_move = 1;
	daemonmoves[sobj].target_node = numa_node;
	unlock(sobj);

	return SUCCESS;
}
