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
* @file numa.c
* @brief This module implements all the NUMA-oriented facilities of ROOT-Sim
* @author Francesco Quaglia
*/

#ifdef HAVE_NUMA

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <numaif.h>
#include <mm/mapmove.h>
#include <arch/atomic.h>
#include <core/core.h>
#include <mm/allocator.h>
#include <mm/numa.h>
#include <mm/bh-manager.h>


//#define NUM_DAEMONS n_cores
#define NUM_DAEMONS 32

/* these are required for move_pages - we use max-sized static memory for latency optimization */
char *pages[MAX_SEGMENT_SIZE];
char *bh_pages[2 * BH_PAGES];
int   nodes[MAX_SEGMENT_SIZE];
int   bh_nodes[2 * BH_PAGES];
int   status[MAX_SEGMENT_SIZE];
static int *numa_nodes;
map_move moves[MAX_SOBJS];


extern int handled_sobjs;
mem_map  *daemonmaps;
map_move *daemonmoves;

__thread int lastlocked;

void set_daemon_maps(mem_map* argA, map_move* argB){
	daemonmaps = argA;
	daemonmoves = argB;
}


int init_move(void){
	
	unsigned long i;
	int ret;
	pthread_t daemon_tid;

        if( (sobjs <= 0)||(sobjs>MAX_SOBJS) ) return INVALID_SOBJS_COUNT; 


        for (i = 0; i < sobjs; i++){
		pthread_spin_init(&(daemonmoves[i].spinlock),0);
		daemonmoves[i].need_move = 0;
		daemonmoves[i].target_node = 0;
        }

	for (i=0; i<NUM_DAEMONS; i++){

		ret = pthread_create(&daemon_tid, NULL, background_work, (void*)i);	

		if( ret ) goto bad_init;
       	} 
        return SUCCESS;

bad_init:

        return INIT_ERROR; 
}


void move_sobj(int sobj, unsigned numa_node){

	mem_map* m_map;
        mdt_entry* mdte;
        int i;
	int k;
	int totpages;
                
	AUDIT
	printf("audit on maps: %p - handled objs is %d\n",daemonmaps,handled_sobjs);

        if( (sobj < 0)||(sobj>=handled_sobjs) ){
                printf("move request on invalid sobj\n");
                return ; 
        }

        m_map = &daemonmaps[sobj]; 

	k = 0;
	totpages = 0;

        for(i=0;i<m_map->active;i++){
                mdte = (mdt_entry*)m_map->base + i;
                
                AUDIT
                printf("moving segment in mdt entry %d - content: addr is %p - num pages is %d\n",i,mdte->addr,mdte->numpages);

		move_segment(mdte,numa_node);
        }

//	move_BH(sobj,numa_node);

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

//~ move_operation:
	ret = numa_move_pages(0, pagecount, (void **)pages, nodes, status, MPOL_MF_MOVE);
	AUDIT
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

	int i;
	int ret;
	int pagecount = 2 * BH_PAGES; // live and expired
	
	for(i = 0; i < BH_PAGES; i++) {
		bh_pages[i] = ((char *)daemonmaps[sobj].actual_bh_addresses[0] + i * PAGE_SIZE);
	}
	
	for(i = BH_PAGES; i < 2*BH_PAGES; i++) {
		bh_pages[i] = ((char *)daemonmaps[sobj].actual_bh_addresses[1] + i * PAGE_SIZE);
	}
	
	for (i = 0; i < pagecount; i++) {
		bh_nodes[i] = numa_node;
	}	

	ret = numa_move_pages(0, pagecount, (void **)bh_pages, bh_nodes, status, MPOL_MF_MOVE);

	AUDIT
	printf("HB move - sobj %d - return value is %d\n",sobj,ret);

}


void *background_work(void *me) {
	long long sobj;
	int node;


	while(1){

		sleep(SLEEP_PERIOD);

		AUDIT
		printf("RS numa daemon wakeup\n");


		for(sobj = (long long)me; sobj < handled_sobjs; sobj = sobj + n_cores){

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




static int query_numa_node(int id){
        #define NUMA_INFO_FILE "./numa_info"
        #define BUFF_SIZE 1024

        FILE *numa_info;

        char buff[BUFF_SIZE];
        char temp[BUFF_SIZE];

        int i;
        int core_id;
        char* p;

        system("numactl --hardware | grep cpus > numa_info");

        numa_info = fopen(NUMA_INFO_FILE,"r");

        i = 0;
        while( fgets(buff, BUFF_SIZE, numa_info)){
                sprintf(temp,"node %i cpus:",i);

                p = strtok(&buff[strlen(temp)]," ");

                while(p){
                        core_id = strtol(p,NULL, 10);
                        if (core_id == id) 
				return i;
                        p = strtok(NULL," ");
                }
                i++;
        }

	fclose(numa_info);

	unlink("numa_info");
       
        return -1;
	#undef NUMA_INFO_FILE
	#undef BUFF_SIZE
}

static void setup_numa_nodes(void) {

	unsigned int i;

	numa_nodes = rsalloc(sizeof(int) * n_cores);

	for(i = 0; i < n_cores; i++) {
		numa_nodes[i] = query_numa_node(i);
	}

}


int get_numa_node(int core) {
	return numa_nodes[core];
}



#endif /* HAVE_NUMA */


