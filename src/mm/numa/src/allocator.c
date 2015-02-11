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
#include "allocator.h"
#include <numaif.h>
#include <errno.h>

#define AUDIT if(1)

mem_map maps[MAX_SOBJS];
map_move moves[MAX_SOBJS];
int handled_sobjs = -1;



void audit(){

	printf("MAPS tabel is at address %p\n",maps);
	printf("MDT entries are %d (page size is %d - sizeof mdt entry is %d)\n",MDT_ENTRIES,PAGE_SIZE,sizeof(mdt_entry));
	

}

void audit_map(int sobj){

	mem_map* mmap;
	mdt_entry* mdte;
	int i;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ){
		printf("audit request on invalid sobj\n");
		return ; 
	}

	mmap = &maps[sobj]; 

	for(i=0;i<mmap->active;i++){
		mdte = (mdt_entry*)mmap->base + i;
		printf("mdt entry %d is at address %p - content: addr is %p - num pages is %d\n",i,mdte,mdte->addr,mdte->numpages);
	}

	return ;
}


int init_allocator(int sobjs){

	int i;
	char* addr;

	if( (sobjs <= 0)||(sobjs>MAX_SOBJS) ) return INVALID_SOBJS_COUNT; 

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
void* allocate_segment(int sobj, int size){

	mdt_entry* mdt;
	char* segment;
	int numpages;
	int ret;

	if( (sobj < 0)||(sobj>=handled_sobjs) ) goto bad_allocate; 

	if( size<=0) goto bad_allocate;

	numpages = (int)(size/(int)(PAGE_SIZE));

	if (size % PAGE_SIZE) numpages++;

	AUDIT
	printf("numpages is %d\n",numpages);

	if(numpages > MAX_SEGMENT_SIZE) goto bad_allocate;

	ret = lock(sobj);
	if(ret == FAILURE) goto bad_allocate;

	mdt = get_new_mdt_entry(sobj);
	if (mdt == NULL) {
		unlock(sobj);
		goto bad_allocate;
	}

	AUDIT
	printf("returned mdt is at address %p\n",mdt);


	AUDIT
	printf("allocate segment: request for %d bytes - actual allocation is of %d pages\n",size,numpages);

        segment = (char*)mmap((void*)NULL,PAGE_SIZE*numpages, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	AUDIT
	printf("actual allocation is at address %p\n",segment);

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

char* allocate_page(){

        char* page;

        page = (char*)mmap((void*)NULL,PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	if (page == MAP_FAILED) {
		goto bad_allocate_page;
	}

	return page;

 bad_allocate_page:

	return NULL;
}

char* allocate_mdt(){

        char* page;

        page = allocate_page();

	return page;
}

mdt_entry* get_new_mdt_entry(int sobj){
	
	mem_map* mmap;
	mdt_entry* mdte;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ) return NULL; 

	mmap = &maps[sobj]; 

	if (mmap->active >= mmap->size){
		goto bad_new_mdt_entry;
	}

	mmap->active += 1;

	mdte = (mdt_entry*)mmap->base + mmap->active - 1 ;

	return mdte;
	
bad_new_mdt_entry:
	return NULL;
}

int release_mdt_entry(int sobj){

	mem_map* mmap;
	mdt_entry* mdte;
		
	if( (sobj < 0)||(sobj>=handled_sobjs) ) return MDT_RELEASE_FAILURE; 

	mmap = &maps[sobj]; 

	if (mmap->active <= 0){
		goto bad_mdt_release;
	}

	mmap->active -= 1;

	return SUCCESS; 

bad_mdt_release:

	return MDT_RELEASE_FAILURE;
}
