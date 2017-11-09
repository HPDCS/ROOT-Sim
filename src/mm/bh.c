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
#include <communication/communication.h>
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
	msg_t **addr;

	bh_maps[lid].expired_read = 0;
	bh_maps[lid].expired_last_written = bh_maps[lid].live_written;

	bh_maps[lid].live_written = 0;

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
        msg_t **addr;

        bh_maps = rsalloc(sizeof(struct _bhmap) * n_prc);
        bh_write = rsalloc(sizeof(atomic_t) * n_prc);
	bh_read = rsalloc(sizeof(atomic_t) * n_prc);

        for (i = 0; i < n_prc; i++) {
		bzero(&bh_maps[i], sizeof(struct _bhmap));

                addr = (msg_t **)allocate_pages(INITIAL_BH_PAGES);
                if (addr == NULL)
			return false;

                bh_maps[i].live_bh = addr;
		bh_maps[i].actual_bh_addresses[0] = addr;
		bh_maps[i].current_pages[0] = INITIAL_BH_PAGES;


                addr = (msg_t **)allocate_pages(INITIAL_BH_PAGES);
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


int insert_BH(int lid, msg_t *msg) {
	msg_t **old_buffer, **new_buffer;
	unsigned int old_size;

	spin_lock(&bh_write[lid]);

	unsigned int available_space = (bh_maps[lid].live_bh == bh_maps[lid].actual_bh_addresses[0] ? bh_maps[lid].current_pages[0] : bh_maps[lid].current_pages[1]) * PAGE_SIZE;

	// Reallocate the live BH buffer. Don't touch the other buffer,
	// as in this way the critical section is much shorter
	if(bh_maps[lid].live_written * sizeof(msg_t *) >= available_space) {

		spin_lock(&bh_read[lid]);

		old_buffer = bh_maps[lid].live_bh;

		// Update stable pointers
		if(bh_maps[lid].actual_bh_addresses[0] == old_buffer) {
			old_size = bh_maps[lid].current_pages[0];
			bh_maps[lid].current_pages[0] *= 2;
			new_buffer = bh_maps[lid].actual_bh_addresses[0] = (msg_t **)allocate_pages(bh_maps[lid].current_pages[0]);
		} else {
			old_size = bh_maps[lid].current_pages[1];
			bh_maps[lid].current_pages[1] *= 2;
			new_buffer = bh_maps[lid].actual_bh_addresses[1] = (msg_t **)allocate_pages(bh_maps[lid].current_pages[1]);
		}

		bh_maps[lid].live_bh = new_buffer;

		memcpy(new_buffer, old_buffer, bh_maps[lid].live_written * sizeof(msg_t *));

		spin_unlock(&bh_read[lid]);

		free_pages(old_buffer, old_size);
	}

	bh_maps[lid].live_bh[bh_maps[lid].live_written++] = msg;

	spin_unlock(&bh_write[lid]);

	return true; // TODO: pointless...
}

void *get_BH(unsigned int lid) {
	msg_t *msg;
	void *buff = NULL;

	spin_lock(&bh_read[lid]);

	if(bh_maps[lid].expired_read == bh_maps[lid].expired_last_written) {
		spin_lock(&bh_write[lid]);
		switch_bh(lid);
		spin_unlock(&bh_write[lid]);
	}

	if(bh_maps[lid].expired_read == bh_maps[lid].expired_last_written) {
		goto leave;
	}

	msg = bh_maps[lid].expired_bh[bh_maps[lid].expired_read];
	bh_maps[lid].expired_bh[bh_maps[lid].expired_read] = (msg_t *)0xDEADB00B;
	bh_maps[lid].expired_read++;

	validate_msg(msg);

	// TODO: we should reorganize the msg list so as to keep pointers, to avoid this copy
	buff = get_buffer(lid, sizeof(msg_t) + msg->size);
	memcpy(buff, msg, sizeof(msg_t) + msg->size);

	// TODO: this should be removed according to the previous TODO in this function
	msg_release(msg);

    leave:
	spin_unlock(&bh_read[lid]);
	return buff;
}
