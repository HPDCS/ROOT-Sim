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


#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <numaif.h>
#include <errno.h>
#include <mm/dymelor.h>
#include <mm/allocator.h>


extern void *__real_malloc(size_t);
extern void __real_free(void *);



#define AUDIT if(0)

mem_map maps[MAX_SOBJS];
map_move moves[MAX_SOBJS];
int handled_sobjs = -1;



void audit(void) {

	printf("MAPS tabel is at address %p\n",maps);
	printf("MDT entries are %lu (page size is %d - sizeof mdt entry is %lu)\n",MDT_ENTRIES,PAGE_SIZE,sizeof(mdt_entry));
	

}

void audit_map(unsigned int sobj){

	mem_map* m_map;
	mdt_entry* mdte;
	int i;
		
	if( (sobj>=handled_sobjs) ){
		printf("audit request on invalid sobj\n");
		return ; 
	}

	m_map = &maps[sobj]; 

	for(i=0;i<m_map->active;i++){
		mdte = (mdt_entry*)m_map->base + i;
		printf("mdt entry %d is at address %p - content: addr is %p - num pages is %d\n",i,mdte,mdte->addr,mdte->numpages);
	}

	return ;
}


int allocator_init(unsigned int sobjs){
	unsigned int i;
	char* addr;

	if( (sobjs > MAX_SOBJS) )
		return INVALID_SOBJS_COUNT; 

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
	
	set_daemon_maps(maps,moves);
	init_move(sobjs);

	set_BH_map(maps);
	init_BH();

	return SUCCESS;

bad_init:
	return INIT_ERROR; 
}

/*
int init_BH(){

	int i;
	char* addr;
	int sobjs;

	if(handled_sobjs < 0) return INVALID_SOBJS_COUNT; 

	sobjs = handled_sobjs;

	for (i=0; i<sobjs; i++){
		addr = allocate_page();
		if (addr == NULL) goto bad_init;
		maps[i].live_bh = addr;
		addr = allocate_page();
		if (addr == NULL) goto bad_init;
		maps[i].expired_bh = addr;
	}
	
	return SUCCESS;

bad_init:
	return INIT_ERROR; 


}
*/
void* allocate_segment(unsigned int sobj, size_t size){

	mdt_entry* mdt;
	char* segment;
	int numpages;
	int ret;

	if( ((int)sobj >= handled_sobjs) ) goto bad_allocate; 

	if( size<=0) goto bad_allocate;

	numpages = (int)(size/(int)(PAGE_SIZE));

	if (size % PAGE_SIZE) numpages++;

	AUDIT
	printf("segment allocation - requested numpages is %d\n",numpages);

	if(numpages > MAX_SEGMENT_SIZE) goto bad_allocate;

	ret = lock(sobj);
	if(ret == FAILURE) goto bad_allocate;

	mdt = get_new_mdt_entry(sobj);
	if (mdt == NULL) {
		unlock(sobj);
		goto bad_allocate;
	}

	AUDIT
	printf("segment allocation - returned mdt is at address %p\n",mdt);


	AUDIT
	printf("allocate segment: request for %ld bytes - actual allocation is of %d pages\n",size,numpages);

        segment = (char*)mmap((void*)NULL,PAGE_SIZE*numpages, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	AUDIT
	printf("allocate segment: actual allocation is at address %p\n",segment);

	if (segment == MAP_FAILED) {
		release_mdt_entry(sobj);
		unlock(sobj);
		goto bad_allocate;
	}

	mdt->addr = segment;
	mdt->numpages = numpages;

	AUDIT	
	audit_map(sobj);

	unlock(sobj);

	return segment;

bad_allocate:
	return NULL;

}

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

void *pool_get_memory(unsigned int lid, size_t size) {
	//~ #ifndef NUMA
	//~ (void)lid;
	//~ return __real_malloc(size);
	//~ #else
	return allocate_segment(lid, size);
	//~ #endif
}

void pool_release_memory(unsigned int lid, void *ptr) {
	//~ #ifndef NUMA
	//~ (void)lid;
	//~ __real_free(ptr);
	//~ #else
	// TODO
	//~ #endif
}
