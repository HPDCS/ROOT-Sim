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

#pragma once

#include <stdint.h>
#include <datatypes/array.h>

// TODO DOCUMENTATION!!!

#define MAX_LOAD_FACTOR 0.85
#define MIN_LOAD_FACTOR 0.05

typedef uint32_t map_size_t;
typedef map_size_t hash_t;
#define HMAP_INVALID_I UINT_MAX
typedef unsigned long long key_type_t;

struct _inner_hash_map_t{
	map_size_t capacity_mo;
	struct _hash_map_node_t{
		unsigned long long key;
		hash_t hash;
		unsigned elem_i;
	} *nodes;
};

// the type must have a unsigned long long variable named key
#define rootsim_hash_map(type) \
	struct { \
		rootsim_array(type) elems; \
		struct _inner_hash_map_t _i_hmap; \
		}

#define hash_map_init(hashmap) ({ \
		_hash_map_init(&((hashmap)._i_hmap)); \
		array_init((hashmap).elems); \
	})

#define hash_map_count(hashmap) ({ \
		array_count((hashmap).elems);\
	})

#define hash_map_fini(hashmap) ({ \
		_hash_map_fini(&((hashmap)._i_hmap)); \
		array_fini((hashmap).elems); \
	})

#define hash_map_reserve_elem(hashmap, key) ({ \
		_hash_map_add(&((hashmap)._i_hmap), key, array_count((hashmap).elems)); \
		__typeof__(array_items((hashmap).elems)) __ret = array_reserve((hashmap).elems, 1); \
		assert(__ret); \
		__ret; \
	})

#define hash_map_lookup(hashmap, key) ({ \
		map_size_t __lkp_i =  _hash_map_lookup(&((hashmap)._i_hmap), key); \
		__lkp_i != UINT_MAX ? &(array_get_at((hashmap).elems, __lkp_i)) : NULL; \
	})

#define hash_map_delete_elem(hashmap, elem) ({ \
		assert(array_count((hashmap).elems)); \
		key_type_t __l_key = array_peek((hashmap).elems).key; \
		unsigned __rem_i = elem - array_items((hashmap).elems); \
		_hash_map_remove(&((hashmap)._i_hmap), elem->key, array_count((hashmap).elems)); \
		_hash_map_update_i(&((hashmap)._i_hmap), __l_key, __rem_i); \
		array_lazy_remove_at((hashmap).elems, __rem_i); \
	})

#define hash_map_items(hashmap) ({ \
		assert(array_count((hashmap).elems)); \
		__typeof__(array_items((hashmap).elems)) __ret = array_items((hashmap).elems); \
		__ret; \
	})

#define hash_map_dump_size(hashmap) ({ \
		size_t __ret = array_dump_size((hashmap).elems); \
		__ret += _hash_map_dump_size(&((hashmap)._i_hmap)); \
		__ret; \
	})

#define hash_map_dump(hashmap, destination) ({ \
		array_dump((hashmap).elems, destination); \
		destination = _hash_map_dump(&((hashmap)._i_hmap), destination); \
	})

#define hash_map_load(hashmap, source) ({ \
		array_load((hashmap).elems, source); \
		source = _hash_map_load(&((hashmap)._i_hmap), source); \
	})


// XXX returning and requesting a hash_map_pair_t forces a lot of ugly casts, change it somehow!
void 		_hash_map_init	(struct _inner_hash_map_t *_i_hmap);
void		_hash_map_fini	(struct _inner_hash_map_t *_i_hmap);
void 		_hash_map_add	(struct _inner_hash_map_t *_i_hmap, unsigned long long key, map_size_t cur_count);
unsigned	_hash_map_lookup(struct _inner_hash_map_t *_i_hmap, unsigned long long key);
void		_hash_map_remove(struct _inner_hash_map_t *_i_hmap, unsigned long long key, map_size_t cur_count);
void 		_hash_map_update_i(struct _inner_hash_map_t *_i_hmap, unsigned long long key, map_size_t new_i);
inline size_t		_hash_map_dump_size(struct _inner_hash_map_t *_i_hmap);
inline unsigned char*	_hash_map_dump(struct _inner_hash_map_t *_i_hmap, unsigned char *_destination);
inline unsigned char*	_hash_map_load(struct _inner_hash_map_t *_i_hmap, unsigned char *_source);
