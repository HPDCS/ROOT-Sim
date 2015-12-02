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
* @file allocator.c
* @brief 
* @author Francesco Quaglia
*/


#define NEW_ALLOCATOR

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#ifdef HAVE_NUMA
#include <numaif.h>
#include <mm/mapmove.h>
#endif

#include <mm/dymelor.h>
#include <mm/allocator.h>
#include <mm/allocator_ecs.h>


extern void *__real_malloc(size_t);
extern void __real_free(void *);

static int *numa_nodes;

static spinlock_t segment_lock;


#define AUDIT if(0)

mem_map maps[MAX_SOBJS];
map_move moves[MAX_SOBJS];
int handled_sobjs = -1;


char *allocate_pages(int num_pages) {
	
        char* page;

        page = (char*)mmap((void*)NULL, num_pages * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	if (page == MAP_FAILED) {
		goto bad_allocate_page;
	}

	return page;

 bad_allocate_page:

	return NULL;
}


void audit(void) {

	printf("MAPS tabel is at address %p\n",maps);
	printf("MDT entries are %lu (page size is %d - sizeof mdt entry is %lu)\n",MDT_ENTRIES,PAGE_SIZE,sizeof(mdt_entry));
	

}

void audit_map(unsigned int sobj){

	mem_map* m_map;
	mdt_entry* mdte;
	int i;
		
	if( (sobj >= handled_sobjs) ){
		printf("audit request on invalid sobj\n");
		return ; 
	}

	m_map = &maps[sobj]; 

	for(i=0;i<m_map->active;i++){
		mdte = (mdt_entry*)m_map->base + i;
		printf("mdt entry %d is at address %p - content: addr is %p - num pages is %d\n",i,mdte,mdte->addr,mdte->numpages);
	}
}



#ifdef HAVE_NUMA
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

void* allocate_segment(unsigned int sobj, size_t size, bool is_recoverable) {

	mdt_entry* mdt;
	char* segment;
	int numpages;
	int ret;

	#ifdef NEW_ALLOCATOR
	static void *my_initial_address = (void *)(180L * 256L * 256L * 256L * PAGE_SIZE);
	#endif

	if( ((int)sobj >= handled_sobjs) ) goto bad_allocate; 

	if(size <= 0)
		goto bad_allocate;

	numpages = (int)(size/(int)(PAGE_SIZE));

	if (size % PAGE_SIZE) numpages++;

	AUDIT
	printf("segment allocation - requested numpages is %d\n",numpages);

	if(numpages > MAX_SEGMENT_SIZE) goto bad_allocate;

	#ifdef HAVE_NUMA
	ret = lock(sobj);
	if(ret == FAILURE)
		goto bad_allocate;
	#endif

	mdt = get_new_mdt_entry(sobj);
	if (mdt == NULL) {
		goto bad_allocate;
	}

	AUDIT
	printf("segment allocation - returned mdt is at address %p\n",mdt);


	AUDIT
	printf("allocate segment: request for %ld bytes - actual allocation is of %d pages\n",size,numpages);


	#ifndef NEW_ALLOCATOR
	//TODO MN
	if(is_recoverable)
		segment = (char*)get_memory_ecs(sobj,PAGE_SIZE*numpages);
	else
		segment = (char*)mmap((void*)NULL,PAGE_SIZE*numpages, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	#else


	spin_lock(&segment_lock);
	
	//TODO MN
	if(is_recoverable){
                segment = (char*) get_memory_ecs(sobj,PAGE_SIZE*numpages);
	//	printf("Memory is_recoverable - value of segment: %p\n",segment);
	}
        else{
		// Update my_initial_address, keeping it aligned to large pages
		my_initial_address = (void *)((char *)my_initial_address - ( PAGE_SIZE * (int)( ceil((double)numpages / 256.0) * 256  )  ));
		segment = (char*)mmap(my_initial_address, PAGE_SIZE*numpages, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0,0);
	}
	
	spin_unlock(&segment_lock);
	
	if(segment == MAP_FAILED)
		abort();


	#endif

	AUDIT
	printf("allocate segment: actual allocation is at address %p\n",segment);
	
	if (segment == MAP_FAILED) {
		release_mdt_entry(sobj);
		goto bad_allocate;
	}

	mdt->addr = segment;
	mdt->numpages = numpages;

	AUDIT	
	audit_map(sobj);

	#ifdef HAVE_NUMA
	unlock(sobj);
	#endif

	return segment;

bad_allocate:
	printf("Bad allocation\n");
	#ifdef HAVE_NUMA
	unlock(sobj);
	#endif
	
	return NULL;

}


char* allocate_page(void) {
	return allocate_pages(1);
}

char* allocate_mdt(void) {

        char* page;

        page = allocate_pages(MDT_PAGES);

	return page;
}

mdt_entry* get_new_mdt_entry(int sobj){
	
	mem_map* m_map;
	mdt_entry* mdte;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ) return NULL; 

	m_map = &maps[sobj]; 

	if (m_map->active >= m_map->size){
		goto bad_new_mdt_entry;
	}

	m_map->active += 1;

	mdte = (mdt_entry*)m_map->base + m_map->active - 1 ;

	return mdte;
	
    bad_new_mdt_entry:
	return NULL;
}

int release_mdt_entry(int sobj){
	mem_map* m_map;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ) return MDT_RELEASE_FAILURE; 

	m_map = &maps[sobj]; 

	if (m_map->active <= 0){
		goto bad_mdt_release;
	}

	m_map->active -= 1;

	return SUCCESS; 

    bad_mdt_release:

	return MDT_RELEASE_FAILURE;
}


void *pool_get_memory(unsigned int lid, size_t size, bool is_recoverable) {
	return allocate_segment(lid, size, is_recoverable);
}


void pool_release_memory(unsigned int lid, void *ptr) {
	// TODO
}


int allocator_init(unsigned int sobjs) {
	unsigned int i;
	char* addr;

	if( (sobjs > MAX_SOBJS) )
		return INVALID_SOBJS_COUNT; 
	
	if(allocator_ecs_init(sobjs)) {
		printf("ERROR allocator_ecs_init\n");
		return INIT_ERROR;
	}
	
	handled_sobjs = sobjs;

	for (i=0; i<sobjs; i++){
		addr = allocate_mdt();
		if (addr == NULL) goto bad_init;
		maps[i].base = addr;
		maps[i].active = 0;
		maps[i].size = MDT_ENTRIES;
		AUDIT
		printf("INIT: sobj %d - base address is %p - active are %d - MDT size is %d\n",i,maps[i].base, maps[i].active, maps[i].size);
	}
	
#ifdef HAVE_NUMA
	set_daemon_maps(maps, moves);
	init_move(sobjs);
#endif

	set_BH_map(maps);
	init_BH();

#ifdef HAVE_NUMA
	setup_numa_nodes();
#endif

	spinlock_init(&segment_lock);

	return SUCCESS;

bad_init:
	return INIT_ERROR; 
}

//TODO MN
void allocator_fini(unsigned int sobjs) {
	allocator_ecs_fini(sobjs);
}

mem_map* get_m_map(int sobj){
        if( (sobj < 0)||(sobj>=handled_sobjs) ) return NULL;

        return &maps[sobj];
}
