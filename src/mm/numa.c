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
#include <numa.h>
#include <arch/atomic.h>
#include <core/core.h>
#include <mm/allocator.h>
#include <mm/dymelor.h>
#include <mm/numa.h>
#include <scheduler/process.h>


//#define NUM_DAEMONS n_cores
#define NUM_DAEMONS 32

/* these are required for move_pages - we use max-sized static memory for latency optimization */
static char *pages[PER_LP_PREALLOCATED_MEMORY / PAGE_SIZE];
static int   nodes[PER_LP_PREALLOCATED_MEMORY / PAGE_SIZE];
static int   status[PER_LP_PREALLOCATED_MEMORY / PAGE_SIZE];
static int *numa_nodes;
static map_move *moves;



void move_lid(int lid, unsigned numa_node) {
	int pagecount;
	int i;
	char *segment_addr;

	pagecount = PER_LP_PREALLOCATED_MEMORY / PAGE_SIZE;
	segment_addr = mem_areas[lid];

	for (i = 0; i < pagecount; i++){
		pages[i] = segment_addr + i * PAGE_SIZE;
		nodes[i] = numa_node;
	}

	numa_move_pages(0, pagecount, (void **)pages, nodes, status, MPOL_MF_MOVE);
}



static int numa_verify(int lid){
	int node = -1;

	if(moves[lid].need_move == 1){
		node = moves[lid].target_node;
		moves[lid].need_move = 0;
	}

	return node;
}



static void *numa_move_daemon(void *daemon_id) {
	long long lid;
	int node;

	while(1){

		sleep(SLEEP_PERIOD);

		for(lid = (long long)daemon_id; lid < n_prc; lid = lid + n_cores){

			spin_lock(&(moves[lid].spinlock));
			node = numa_verify(lid);
			if(unlikelynew(node))
				move_lid(lid,node);
			spin_unlock(&(moves[lid].spinlock));

		}
	}

	return NULL;
}

void numa_move_request(int lid, int numa_node){

	spin_lock(&(moves[lid].spinlock));
	moves[lid].need_move = 1;
	moves[lid].target_node = numa_node;
	spin_unlock(&(moves[lid].spinlock));
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



void numa_init(void){
	unsigned long i;
	pthread_t daemon_tid;
	int ret;

	moves = rsalloc(sizeof(map_move) * n_prc);

        for (i = 0; i < n_prc; i++) {
		moves[i].need_move = 0;
		moves[i].target_node = 0;
		spinlock_init(&moves[i].spinlock);
        }

	for (i = 0; i < NUM_DAEMONS; i++) {
		ret = pthread_create(&daemon_tid, NULL, numa_move_daemon, (void*)i);
		if(ret)
			rootsim_error(true, "Unable to setup NUMA movement daemons. Aborting...\n");
	}

	setup_numa_nodes();
}


#endif /* HAVE_NUMA */


