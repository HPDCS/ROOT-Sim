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
* @file triber.c
* @brief This module implements a Triber stack.
*/

#include <datatypes/treiber.h>
#include <mm/dymelor.h>

// TODO: move the CAS implementation with the builtin in <arch/atomic.h>

#define CAS(ptr, old, new) __sync_bool_compare_and_swap((ptr), (old), (new))

treiber *treiber_init(void) {
	treiber *ret;

	ret = rsalloc(sizeof(treiber));
	ret->next = NULL;
	ret->data = NULL;

	return ret;
}


void treiber_push(treiber *treib, void *data) {
	treiber *f;
	treiber *t = rsalloc(sizeof(treiber));
	t->data = data;

	do {
		f = treib->next;
		t->next = f;
		if(CAS(&treib->next, f, t))
			return;
	} while(1);
}

void *treiber_pop(treiber *treib) {
	void *data;

	if(treib->next == NULL) {
		return NULL;
	}

	do {
		treiber *f = treib->next;
		treiber *f_nxt = treib->next->next;

		if(CAS(&treib->next, f, f_nxt)) {
			data = f->data;
			rsfree(f);
			return data;
		}
	} while(1);
}

