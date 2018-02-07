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
* @file heap.h
* @brief This header defines macros for accessing the heap
* 	 implementation used in the simulator.
* @author Stefano Conoci
* @date November 2, 2018
*/

#pragma once 
 
typedef struct {
       	double priority;
	void *data;
} node_heap_t;
 
typedef struct {
	node_heap_t *nodes;
	int len;
	int size;
} heap_t;

void heap_push(heap_t *, double, void *);

// Returns data sorted from lowest to highest priority
void* heap_pop(heap_t *);
