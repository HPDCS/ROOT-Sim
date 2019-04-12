/**
* @file datatypes/msgchannel.c
*
* @brief A (M, 1) channel for messages.
*
* This module implements an (M, 1) channel to transfer message pointers.
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
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
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arch/atomic.h>
#include <core/core.h>
#include <communication/communication.h>
#include <datatypes/list.h>
#include <datatypes/msgchannel.h>
#include <mm/mm.h>

// These are used to simplify reading the code
#define M_WRITE	0
#define M_READ	1

static void switch_channel_buffers(msg_channel * mc)
{
	struct _msg_buff *volatile swap;

	mc->buffers[M_WRITE]->read = 0;
	mc->buffers[M_READ]->written = 0;

	swap = mc->buffers[M_WRITE];
	mc->buffers[M_WRITE] = mc->buffers[M_READ];
	mc->buffers[M_READ] = swap;
}

void fini_channel(msg_channel * mc)
{
	rsfree((void *)mc->buffers[M_WRITE]->buffer);
	rsfree((void *)mc->buffers[M_READ]->buffer);
	rsfree(mc->buffers[M_WRITE]);
	rsfree(mc->buffers[M_READ]);
	rsfree(mc);
}

msg_channel *init_channel(void)
{
	msg_channel *mc = rsalloc(sizeof(msg_channel));

	mc->buffers[M_READ] = rsalloc(sizeof(struct _msg_buff));
	mc->buffers[M_WRITE] = rsalloc(sizeof(struct _msg_buff));

	if (mc->buffers[M_READ] == NULL || mc->buffers[M_WRITE] == NULL)
		rootsim_error(true, "Unable to allocate message channel\n");

	mc->buffers[M_READ]->buffer =
	    rsalloc(INITIAL_CHANNEL_SIZE * sizeof(msg_t *));
	mc->buffers[M_READ]->size = INITIAL_CHANNEL_SIZE;
	mc->buffers[M_READ]->written = 0;
	mc->buffers[M_READ]->read = 0;

	mc->buffers[M_WRITE]->buffer =
	    rsalloc(INITIAL_CHANNEL_SIZE * sizeof(msg_t *));
	mc->buffers[M_WRITE]->size = INITIAL_CHANNEL_SIZE;
	mc->buffers[M_WRITE]->written = 0;
	mc->buffers[M_WRITE]->read = 0;

	if (mc->buffers[M_READ]->buffer == NULL
	    || mc->buffers[M_WRITE]->buffer == NULL)
		rootsim_error(true, "Unable to allocate message channel\n");

	spinlock_init(&mc->write_lock);

	return mc;
}

void insert_msg(msg_channel * mc, msg_t * msg)
{

	spin_lock(&mc->write_lock);

	// Reallocate the live BH buffer. Don't touch the other buffer,
	// as in this way the critical section is much shorter
	if (unlikely
	    (mc->buffers[M_WRITE]->written == mc->buffers[M_WRITE]->size)) {

		mc->buffers[M_WRITE]->size *= 2;
		mc->buffers[M_WRITE]->buffer =
		    rsrealloc((void *)mc->buffers[M_WRITE]->buffer,
			      mc->buffers[M_WRITE]->size * sizeof(msg_t *));

		if (unlikely(mc->buffers[M_WRITE]->buffer == NULL))
			rootsim_error(true, "Unable to reallocate message channel\n");

	}
#ifndef NDEBUG
	validate_msg(msg);
#endif

	int index = mc->buffers[M_WRITE]->written++;
	mc->buffers[M_WRITE]->buffer[index] = msg;

	spin_unlock(&mc->write_lock);
}

void *get_msg(msg_channel * mc)
{
	msg_t *msg = NULL;

	if (unlikely(mc->buffers[M_READ]->read == mc->buffers[M_READ]->written)) {
		spin_lock(&mc->write_lock);
		switch_channel_buffers(mc);
		spin_unlock(&mc->write_lock);
	}

	if (unlikely(mc->buffers[M_READ]->read == mc->buffers[M_READ]->written)) {
		goto leave;
	}

	int index = mc->buffers[M_READ]->read++;
	msg = mc->buffers[M_READ]->buffer[index];
	atomic_dec(&mc->size);

#ifndef NDEBUG
	mc->buffers[M_READ]->buffer[index] = (void *)0xDEADB00B;
	validate_msg(msg);
#endif

 leave:
	return msg;
}
