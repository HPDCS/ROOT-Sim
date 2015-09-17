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
* @file mapmove.h
* @brief 
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

void * background_work( void* );
int verify( int );
void move_BH(int , unsigned );
bool is_moving(void);

#endif /* HAVE_NUMA */

#endif /* _MAPMOVE_H */

