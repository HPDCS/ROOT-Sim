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
* @file msgchannel.h
* @brief This module implements an (M, N) channel to transfer message pointers.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/

#pragma once
#ifndef _BH_H
#define _BH_H

#include <core/core.h>

struct _msg_buff {
	msg_t * volatile *buffer;
	volatile unsigned int	size;
	volatile unsigned int	written;
	volatile unsigned int	read;
};

typedef struct _msg_channel {
	struct _msg_buff	*volatile buffers[2];
	spinlock_t		read_lock;
	spinlock_t		write_lock;
} msg_channel;

#define INITIAL_CHANNEL_SIZE (512 * sizeof(msg_t *))

extern msg_channel *init_channel(void);
extern void fini_channel(msg_channel *);
extern void insert_msg(msg_channel *, msg_t *);
extern void *get_msg(msg_channel *);

#endif /* _BH_H */
