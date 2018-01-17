/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
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

#include <arch/atomic.h>
#include <communication/communication.h>
#include <datatypes/list.h>
#include <mm/bh.h>
#include <mm/mm.h>
#include <mm/dymelor.h>


static struct _bhmap *bh_maps;

static void switch_bh(LID_t lid) {
	struct _map * volatile swap;

	bh_maps[lid_to_int(lid)].maps[M_WRITE]->read = 0;
	bh_maps[lid_to_int(lid)].maps[M_READ]->written = 0;

	swap = bh_maps[lid_to_int(lid)].maps[M_WRITE];
	bh_maps[lid_to_int(lid)].maps[M_WRITE] = bh_maps[lid_to_int(lid)].maps[M_READ];
	bh_maps[lid_to_int(lid)].maps[M_READ] = swap;
}

void BH_fini(void) {
	unsigned int i;

	for (i = 0; i < n_prc; i++) {
		rsfree((void *)bh_maps[i].maps[M_WRITE]->buffer);
		rsfree((void *)bh_maps[i].maps[M_READ]->buffer);
		rsfree(bh_maps[i].maps[M_WRITE]);
		rsfree(bh_maps[i].maps[M_READ]);
	}

	rsfree(bh_maps);
}


bool BH_init(void) {
        unsigned int i;

        bh_maps = rsalloc(sizeof(struct _bhmap) * n_prc);
	bzero(bh_maps, sizeof(struct _bhmap) * n_prc);

        for (i = 0; i < n_prc; i++) {
		bh_maps[i].maps[M_READ] = rsalloc(sizeof(struct _map));
		bh_maps[i].maps[M_WRITE] = rsalloc(sizeof(struct _map));

		if(bh_maps[i].maps[M_READ] == NULL || bh_maps[i].maps[M_WRITE] == NULL)
			return false;

		bh_maps[i].maps[M_READ]->buffer = rsalloc(INITIAL_BH_SIZE * sizeof(msg_t *));
		bh_maps[i].maps[M_READ]->size = INITIAL_BH_SIZE;
		bh_maps[i].maps[M_READ]->written = 0;
		bh_maps[i].maps[M_READ]->read = 0;

		bh_maps[i].maps[M_WRITE]->buffer = rsalloc(INITIAL_BH_SIZE * sizeof(msg_t *));
		bh_maps[i].maps[M_WRITE]->size = INITIAL_BH_SIZE;
		bh_maps[i].maps[M_WRITE]->written = 0;
		bh_maps[i].maps[M_WRITE]->read = 0;

		if(bh_maps[i].maps[M_READ]->buffer == NULL || bh_maps[i].maps[M_WRITE]->buffer == NULL)
			return false;

		spinlock_init(&bh_maps[i].read_lock);
		spinlock_init(&bh_maps[i].write_lock);
	}

        return true;
}


void insert_BH(LID_t the_lid, msg_t *msg) {
	unsigned int lid = lid_to_int(the_lid);
	spin_lock(&bh_maps[lid].write_lock);

	// Reallocate the live BH buffer. Don't touch the other buffer,
	// as in this way the critical section is much shorter
	if(bh_maps[lid].maps[M_WRITE]->written == bh_maps[lid].maps[M_WRITE]->size) {
		spin_lock(&bh_maps[lid].read_lock);

		// TODO: the realloc causes a bug!
		// In some way, it's making the memory map incoherent...
		abort();

		bh_maps[lid].maps[M_WRITE]->size *= 2;
		bh_maps[lid].maps[M_WRITE]->buffer = rsrealloc((void *)bh_maps[lid].maps[M_WRITE]->buffer, bh_maps[lid].maps[M_WRITE]->size);
		if(bh_maps[lid].maps[M_WRITE]->buffer == NULL)
			abort();

		spin_unlock(&bh_maps[lid].read_lock);
	}

	validate_msg(msg);

	int index = bh_maps[lid].maps[M_WRITE]->written++;
	bh_maps[lid].maps[M_WRITE]->buffer[index] = msg;

	spin_unlock(&bh_maps[lid].write_lock);
}

void *get_BH(LID_t the_lid) {
	msg_t *msg;
	void *buff = NULL;
	unsigned int lid = lid_to_int(the_lid);

	spin_lock(&bh_maps[lid].read_lock);

	if(bh_maps[lid].maps[M_READ]->read == bh_maps[lid].maps[M_READ]->written) {
		spin_lock(&bh_maps[lid].write_lock);
		switch_bh(the_lid);
		spin_unlock(&bh_maps[lid].write_lock);
	}

	if(bh_maps[lid].maps[M_READ]->read == bh_maps[lid].maps[M_READ]->written) {
		goto leave;
	}

	int index = bh_maps[lid].maps[M_READ]->read++;
	msg = bh_maps[lid].maps[M_READ]->buffer[index];

	#ifndef NDEBUG
	bh_maps[lid].maps[M_READ]->buffer[index] = (void *)0xDEADB00B;
	#endif
	
	validate_msg(msg);

	// TODO: we should reorganize the msg list so as to keep pointers, to avoid this copy
	size_t size = sizeof(msg_t) + msg->size;
	buff = list_allocate_node_buffer(the_lid, size);
	memcpy(buff, msg, size);

	// TODO: this should be removed according to the previous TODO in this function
	msg_release(msg);

    leave:
	spin_unlock(&bh_maps[lid].read_lock);
	return buff;
}

