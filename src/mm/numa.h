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
* @file numa.h
* @brief This module implements all the NUMA-oriented facilities of ROOT-Sim
* @author Francesco Quaglia
*/

#pragma once
#ifndef _MAPMOVE_H
#define _MAPMOVE_H

#ifdef HAVE_NUMA

#include <stdbool.h>

#define SLEEP_PERIOD 3 //this is defined in seconds
#define NUMA_NODES   8 //numer of handled numa nodes

#define unlikelynew(x)  (x!=-1)

void numa_init(void);
void numa_move_request(int, int);


int verify(int);
bool is_moving(void);
int get_numa_node(int);
void move_sobj(int, unsigned);
void move_segment(unsigned);

#endif /* HAVE_NUMA */

#endif /* _MAPMOVE_H */

