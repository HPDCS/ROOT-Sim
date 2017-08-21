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
#include <mm/mm.h>
#include <mm/dymelor.h>
#include <datatypes/list.h>


static struct _bhmap *bh_maps;
static spinlock_t *bh_write;
static spinlock_t *bh_read;

static char *allocate_pages(int num_pages) {

        char *page;

        page = (char*)mmap((void*)NULL, num_pages * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0,0);

	if (page == MAP_FAILED) {
		page = NULL;
	}

	return page;
}

static void free_pages(void *ptr, size_t length) {
	int ret;

	ret = munmap(ptr, length);
	if(ret < 0)
		perror("free_pages(): unable to deallocate memory");
}



static void switch_bh(int lid){

	char *addr;

	bh_maps[lid].expired_msgs = bh_maps[lid].live_msgs;
	bh_maps[lid].expired_offset = bh_maps[lid].live_offset;
	bh_maps[lid].expired_boundary = bh_maps[lid].live_boundary;

	bh_maps[lid].live_msgs = 0;
	bh_maps[lid].live_offset = 0;
	bh_maps[lid].live_boundary = 0;

	addr = bh_maps[lid].expired_bh;
	bh_maps[lid].expired_bh = bh_maps[lid].live_bh;
	bh_maps[lid].live_bh = addr;
}


static void *get_buffer(int lid, int size) {
	return list_allocate_node_buffer(lid, size);
}



void BH_fini(void) {
	unsigned int i;

	for (i = 0; i < n_prc; i++) {
		free_pages(bh_maps[i].actual_bh_addresses[0], bh_maps[i].current_pages[0]);
		free_pages(bh_maps[i].actual_bh_addresses[1], bh_maps[i].current_pages[1]);
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
                bh_maps[i].live_offset = 0;
                bh_maps[i].live_boundary = 0;
                bh_maps[i].expired_msgs = 0;
                bh_maps[i].expired_offset = 0;
                bh_maps[i].expired_boundary = 0;

                addr = allocate_pages(INITIAL_BH_PAGES);
                if (addr == NULL)
			return false;

                bh_maps[i].live_bh = addr;
		bh_maps[i].actual_bh_addresses[0] = addr;
		bh_maps[i].current_pages[0] = INITIAL_BH_PAGES;


                addr = allocate_pages(INITIAL_BH_PAGES);
                if (addr == NULL)
			return false;

                bh_maps[i].expired_bh = addr;
		bh_maps[i].actual_bh_addresses[1] = addr;
		bh_maps[i].current_pages[1] = INITIAL_BH_PAGES;

		spinlock_init(&bh_write[i]);
		spinlock_init(&bh_read[i]);
	}

        return true;
}


int insert_BH(int lid, void* msg, int size) {
	int tag;
	int needed_store;
	int residual_store;
	int offset;
	void *old_buffer, *new_buffer = NULL;
	size_t old_size;

	//printf("Insert for %d\n", lid);


	spin_lock(&bh_write[lid]);

//	if(bh_maps[lid].live_boundary >= INITIAL_BH_PAGES * PAGE_SIZE) {
//		return false;
//	}

	tag = size;
	needed_store = tag + sizeof(tag);
	residual_store = (bh_maps[lid].live_bh == bh_maps[lid].actual_bh_addresses[0] ? bh_maps[lid].current_pages[0] : bh_maps[lid].current_pages[1])
			 * PAGE_SIZE - bh_maps[lid].live_boundary;


	// Reallocate the live BH buffer. Don't touch the other buffer,
	// as in this way the critical section is much shorter
	if(residual_store < needed_store) {

		//printf("Reallocating...\n");

		spin_lock(&bh_read[lid]);

		old_buffer = bh_maps[lid].live_bh;

		// Update stable pointers
		if(bh_maps[lid].actual_bh_addresses[0] == old_buffer) {
			//printf("first if\n");
			old_size = bh_maps[lid].current_pages[0];
			bh_maps[lid].current_pages[0] *= 2;
			new_buffer = bh_maps[lid].actual_bh_addresses[0] = allocate_pages(bh_maps[lid].current_pages[0]);
		} else {
			//printf("second if\n");
			old_size = bh_maps[lid].current_pages[1];
			bh_maps[lid].current_pages[1] *= 2;
			new_buffer = bh_maps[lid].actual_bh_addresses[1] = allocate_pages(bh_maps[lid].current_pages[1]);
		}

		bh_maps[lid].live_bh = new_buffer;

		memcpy(new_buffer, old_buffer, bh_maps[lid].live_boundary);

		spin_unlock(&bh_read[lid]);

		free_pages(old_buffer, old_size);
	}


	offset = bh_maps[lid].live_boundary;
	memcpy(bh_maps[lid].live_bh + offset, &tag, sizeof(tag));

	offset += sizeof(tag);
	memcpy(bh_maps[lid].live_bh + offset, msg, size);

	bh_maps[lid].live_boundary += needed_store;
	bh_maps[lid].live_msgs += 1;

	spin_unlock(&bh_write[lid]);

	return true;
}

void *get_BH(unsigned int lid) {

	int msg_tag;
	void *buff;
	char *msg_addr;
	int msg_offset;


	if(lid >= n_prc)
		goto no_msg;

	spin_lock(&bh_read[lid]);

	if(bh_maps[lid].expired_msgs <= 0 ) {

		spin_lock(&bh_write[lid]);
		switch_bh(lid);
		spin_unlock(&bh_write[lid]);

	}

	if(bh_maps[lid].expired_msgs <= 0 ){
		spin_unlock(&bh_read[lid]);
		goto no_msg;
	}

	//printf("Getting a message from offset %u\n", bh_maps[lid].expired_offset);
	//fflush(stdout);

	msg_offset = bh_maps[lid].expired_offset;

	msg_addr = bh_maps[lid].expired_bh + msg_offset;

	memcpy(&msg_tag, msg_addr, sizeof(msg_tag));

	buff = get_buffer(lid, msg_tag);

	msg_addr += sizeof(msg_tag);

	memcpy(buff, msg_addr, msg_tag);

	msg_addr += msg_tag;

	bh_maps[lid].expired_offset = (char *)msg_addr - bh_maps[lid].expired_bh;

	bh_maps[lid].expired_msgs -= 1;

	spin_unlock(&bh_read[lid]);

	return buff;

no_msg:
	return NULL;
}
