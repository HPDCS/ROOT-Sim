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
* @file hash_map.h
* @date 9 Nov 2018
* @brief This header implements a simple hash map data structure
* @author Andrea Piccione
*
* This a simple hash map implementation, currently used in the abm layer.
* It's based on a open addressing design:
* it handles collisions through linear probing,
* entries are reordered through robin hood hashing.
* TODO 	by default __wrap_malloc and __wrap_free are used, instead
* 	the choice of the allocation facilities should change on demand
*/

#ifndef SRC_DATATYPES_HASH_MAP_H_
#define SRC_DATATYPES_HASH_MAP_H_

#include <stdint.h>

#define MAX_LOAD_FACTOR 0.85
#define MIN_LOAD_FACTOR 0.05

typedef uint32_t map_size_t;
typedef map_size_t hash_t;

typedef struct _hash_map_pair_t{
	unsigned long long key;
	char elem[];
}hash_map_pair_t;

struct _rootsim_hash_map_t{
	struct _hash_map_node_t{
		hash_map_pair_t *pair;
		hash_t hash;
	} *nodes;
	map_size_t capacity_mo;
	map_size_t count;
};

typedef struct _rootsim_hash_map_t rootsim_hash_map_t;

void 		hash_map_init	(rootsim_hash_map_t *hmap);
void		hash_map_fini	(rootsim_hash_map_t *hmap);
void 		hash_map_add	(rootsim_hash_map_t *hmap, hash_map_pair_t *pair);
hash_map_pair_t* hash_map_lookup(rootsim_hash_map_t *hmap, unsigned long long key);
hash_map_pair_t* hash_map_remove(rootsim_hash_map_t *hmap, unsigned long long key);

#endif /* SRC_DATATYPES_HASH_MAP_H_ */
