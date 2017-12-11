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
* @file bh.h
* @brief
* @author Francesco Quaglia
*/

#pragma once
#ifndef _BH_H
#define _BH_H

#include <core/core.h>

struct _map {
	msg_t		* volatile *buffer;
	volatile unsigned int	size;
	volatile unsigned int	written;
	volatile unsigned int	read;
};

struct _bhmap {
	struct _map	* volatile maps[2];
	spinlock_t	read_lock;
	spinlock_t	write_lock;
};

// These are used to simplify reading the code
#define M_WRITE	0
#define M_READ	1

#define INITIAL_BH_SIZE 1024

extern bool BH_init(void);
extern void BH_fini(void);
extern void insert_BH(int, msg_t *);
extern void *get_BH(unsigned int);

#endif /* _BH_H */
