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
* @file bh.c
* @brief 
* @author Francesco Quaglia
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <pthread.h>

#include <arch/atomic.h>
#include <mm/bh.h>
#include <mm/allocator.h>
#include <mm/dymelor.h>
#include <datatypes/list.h>


//mem_map *bh_maps;
//char fictitious[MAX_MSG_SIZE];

//pthread_spinlock_t bh_write[MAX_LPs];
//pthread_spinlock_t bh_read[MAX_LPs];

//extern int handled_sobjs;

//void set_BH_map(mem_map *argA) {
//        bh_maps = argA;
//}


static struct _bhmap *bh_maps;
static spinlock_t *bh_write;
static spinlock_t *bh_read;


static void switch_bh(int sobj){

	char *addr;

	//atomic needed
	bh_maps[sobj].expired_msgs = bh_maps[sobj].live_msgs;
	bh_maps[sobj].expired_offset = bh_maps[sobj].live_offset;
	bh_maps[sobj].expired_boundary = bh_maps[sobj].live_boundary;

	bh_maps[sobj].live_msgs = 0;
	bh_maps[sobj].live_offset = 0;
	bh_maps[sobj].live_boundary = 0;

	addr = bh_maps[sobj].expired_bh; 
	bh_maps[sobj].expired_bh = bh_maps[sobj].live_bh;
	bh_maps[sobj].live_bh = addr;
}


static void *get_buffer(int sobj, int size) {
	return list_allocate_node_buffer(sobj, size);
}



void BH_fini(void) {
	unsigned int i;
	
	for (i = 0; i < n_prc; i++) {
		// free_pages(bh_maps[i].actual_bh_addresses[0];
		// free_pages(bh_maps[i].actual_bh_addresses[1];
	}
	
	rsfree(bh_maps);
	rsfree(bh_write);
	rsfree(bh_read);
}


bool BH_init(void) {
        unsigned int i;
        char *addr;

        bh_maps = rsalloc(sizeof(struct _bhmap) * n_prc);
        bh_write = rsalloc(sizeof(atomic_t) * n_prc);
	bh_read = rsalloc(sizeof(atomic_t) * n_prc);

        for (i = 0; i < n_prc; i++) {
                bh_maps[i].live_msgs = 0;
                bh_maps[i].live_boundary = 0;
                bh_maps[i].expired_msgs = 0;
                bh_maps[i].expired_offset = 0;
                bh_maps[i].expired_boundary = 0;

                addr = allocate_pages(BH_PAGES);
                if (addr == NULL)
			return false;
			
                bh_maps[i].live_bh = addr;
		bh_maps[i].actual_bh_addresses[0] = addr;

                addr = allocate_pages(BH_PAGES);
                if (addr == NULL)
			return false;
			
                bh_maps[i].expired_bh = addr;
		bh_maps[i].actual_bh_addresses[1] = addr;

		spinlock_init(&bh_write[i]);
		spinlock_init(&bh_read[i]);
	}

        return true;
}


int insert_BH(int sobj, void* msg, int size) {
	int tag;
	int needed_store;
	int residual_store;
	int offset;

	if( (size <= 0) || size > MAX_MSG_SIZE)
		goto bad_insert;

	if( msg == NULL )
		goto bad_insert;

	spin_lock(&bh_write[sobj]);

	if(bh_maps[sobj].live_boundary >= BH_SIZE) {
		spin_unlock(&bh_write[sobj]);
		goto bad_insert;
	}

	tag = size;
	needed_store = tag + sizeof(tag);

	residual_store = BH_SIZE - bh_maps[sobj].live_boundary;
	
	if( residual_store < needed_store ){ 
		spin_unlock(&bh_write[sobj]);
		goto bad_insert;
	}

	offset = bh_maps[sobj].live_boundary;

	memcpy(bh_maps[sobj].live_bh + offset, &tag, sizeof(tag));

	offset += sizeof(tag);

	memcpy(bh_maps[sobj].live_bh + offset, msg, size);

	bh_maps[sobj].live_boundary += needed_store;

	bh_maps[sobj].live_msgs += 1;

	spin_unlock(&bh_write[sobj]);
	
	return true;

bad_insert: 
	// TODO: realloc here!
	printf("BH insert failure - sobj %d\n", sobj);

	return false;
}

void *get_BH(unsigned int sobj) {

	int msg_tag;
	void *buff;
	char *msg_addr;
	int msg_offset;
	

	if(sobj >= n_prc)
		goto no_msg;
	
	spin_lock(&bh_read[sobj]);

	if(bh_maps[sobj].expired_msgs <= 0 ) {
	
		spin_lock(&bh_write[sobj]);
		switch_bh(sobj);
		spin_unlock(&bh_write[sobj]);

	}

	if(bh_maps[sobj].expired_msgs <= 0 ){
		spin_unlock(&bh_read[sobj]);
		goto no_msg;
	}

	msg_offset = bh_maps[sobj].expired_offset;  

	msg_addr = bh_maps[sobj].expired_bh + msg_offset;

	memcpy(&msg_tag, msg_addr, sizeof(msg_tag)); 

	buff = get_buffer(sobj, msg_tag);

	msg_addr += sizeof(msg_tag);

	memcpy(buff, msg_addr, msg_tag);

	msg_addr += msg_tag;

	bh_maps[sobj].expired_offset = (char*)msg_addr - bh_maps[sobj].expired_bh;

	bh_maps[sobj].expired_msgs -= 1;

	spin_unlock(&bh_read[sobj]);

	return buff;

no_msg:
	return NULL;	
}
