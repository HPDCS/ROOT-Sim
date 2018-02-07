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
* @file heap.c
* @brief This file implements the logic for the heap data strucutre 
* 	 used in the simulator.
* @author Stefano Conoci
* @date November 2, 2018
*/

#include <datatypes/heap.h>

#include <stdio.h>
#include <mm/dymelor.h>

void heap_push(heap_t *h, double priority, void *data){
	// Resize if necessary, or alloc for first use
	if (h->len + 1 >= h->size) {
		if(h->size == 0){
			h->nodes = (node_heap_t *) rsalloc(4096*sizeof(node_heap_t));
			h->len = 0;
			h->size = 4096;
		}
		else{ 
			h->size = h->size*2;
			h->nodes = (node_heap_t *) rsrealloc(h->nodes, h->size * sizeof (node_heap_t));
		}
	}
	
	int i = h->len + 1;
	int j = i / 2;
	while (i > 1 && h->nodes[j].priority > priority) {
		h->nodes[i] = h->nodes[j];
		i = j;
		j = j / 2;
	}
	h->nodes[i].priority = priority;
	h->nodes[i].data = data;
	h->len++;
}

void* heap_pop(heap_t *h) {
	int i, j, k;
	if (!h->len) {
		return NULL;
	}
	void *data = h->nodes[1].data;
	h->nodes[1] = h->nodes[h->len];
	double priority = h->nodes[1].priority;
			     
	h->len--;
				 
	i = 1;
	while (1) {
		k = i;
		j = 2 * i;
		if (j <= h->len && h->nodes[j].priority < priority){
			k = j;
		}
		if (j + 1 <= h->len && h->nodes[j + 1].priority < h->nodes[k].priority){
			k = j + 1;
		}
		if (k == i) {
			break;
		}
		h->nodes[i] = h->nodes[k];
		i = k;
	}
	h->nodes[i] = h->nodes[h->len + 1];
	return data;
}


