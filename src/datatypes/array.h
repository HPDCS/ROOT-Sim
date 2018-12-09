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
 * @file array.h
 * @brief This is the implementation of a dynamic array, used for managing various data structures
 * @author Andrea Piccione
 * @date 18 June 2018
 */

#ifndef ARRAY_H_
#define ARRAY_H_

#include <mm/dymelor.h>

#include <memory.h>

// TODO: add some type checking and size checking (is it necessary?)

#define INIT_SIZE_ARRAY 8U

#define rootsim_array(type) \
		struct { \
			type *items; \
			unsigned count, capacity; \
		}

// you can use the array to directly index items, but do at your risk and peril
#define array_items(self) ((self).items)

#define array_count(self) ((self).count)

#define array_capacity(self) ((self).capacity)

#define array_shrink(self) ({ \
		if (array_count(self) > INIT_SIZE_ARRAY && array_count(self) * 3 <= array_capacity(self)) { \
			array_capacity(self) /= 2; \
			array_items(self) = rsrealloc(array_items(self), array_capacity(self) * sizeof(*array_items(self))); \
		} \
	})

#define array_expand(self) ({ \
		if(array_count(self) >= array_capacity(self)){\
			array_capacity(self) *= 2; \
			array_items(self) = rsrealloc(array_items(self), array_capacity(self) * sizeof(*array_items(self))); \
		} \
	})

#define array_new(type) ({ \
		rootsim_array(type) *__newarr; \
		__newarr = rsalloc(sizeof(*__newarr)); \
		array_init(*__newarr);\
		__newarr; \
	})

#define array_free(self) ({ \
		rsfree(array_items(self)); \
		rsfree(&(self)); \
	})

#define array_init(self) ({ \
		array_capacity(self) = INIT_SIZE_ARRAY; \
		array_items(self) = rsalloc(array_capacity(self) * sizeof(*array_items(self))); \
		array_count(self) = 0; \
	})

#define array_fini(self) ({ \
		rsfree(array_items(self)); \
	})
//fixme array_expand doesn't work when reserving with count high since it only doubles once
#define array_reserve(self, count) ({ \
		__typeof__(array_count(self)) __rsvidx = array_count(self); \
		array_count(self) += (count); \
		array_expand(self); \
		&(array_items(self)[__rsvidx]); \
})

#define array_push(self, elem) ({ \
		array_expand(self); \
		array_items(self)[array_count(self)] = (elem); \
		array_count(self)++; \
	})

#define array_pop(self) ({ \
		if(unlikely(!array_count(self))) \
			rootsim_error(true, "pop of an empty array"); \
		__typeof__(*array_items(self)) __popval; \
		array_count(self)--; \
		__popval = array_items(self)[array_count(self)]; \
		array_shrink(self); \
		__popval; \
	})

#define array_add_at(self, i, elem) ({ \
		if(unlikely(array_count(self) <= (i))) \
			rootsim_error(true, "out of bound add in a dynamic array"); \
		array_expand(self); \
		memmove(&(array_items(self)[(i)+1]), &(array_items(self)[(i)]), sizeof(*array_items(self)) * (array_count(self)-(i))); \
		array_items(self)[(i)] = (elem); \
		array_count(self)++; \
	})

#define array_lazy_remove_at(self, i) ({ \
		if(unlikely(array_count(self) <= (i))) \
			rootsim_error(true, "out of bound removal in a dynamic array"); \
		__typeof__(*array_items(self)) __rmval; \
		array_count(self)--; \
		__rmval = array_items(self)[(i)]; \
		array_items(self)[(i)] = array_items(self)[array_count(self)]; \
		array_shrink(self); \
		__rmval; \
	})

#define array_remove_at(self, i) ({ \
		if(unlikely(array_count(self) <= (i))) \
			rootsim_error(true, "out of bound removal in a dynamic array"); \
		__typeof__(*array_items(self)) __rmval; \
		array_count(self)--; \
		__rmval = array_items(self)[(i)]; \
		memmove(&(array_items(self)[(i)]), &(array_items(self)[(i)+1]), sizeof(*array_items(self)) * (array_count(self)-(i))); \
		array_shrink(self); \
		__rmval; \
	})

#define array_remove(self, elem) ({ \
		typeof(array_count(self)) __cntr = array_count(self); \
		while(__cntr--){ \
			if(array_items(self)[__cntr] == (elem)){\
				array_remove_at(self, __cntr); \
				break; \
			} \
		} \
	})
//this isn't checked CARE! TODO add checking
#define array_peek(self) (array_items(self)[array_count(self)-1])
//this isn't checked CARE! TODO add checking
#define array_get_at(self, i) (array_items(self)[i])

#define array_empty(self) (array_count(self) == 0)

#define array_dump_size(self) ({ \
		sizeof(array_count(self)) + array_count(self)*sizeof(*array_items(self)); \
	})

#define array_dump(self, mem_area) ({ \
		memcpy((mem_area), &array_count(self), sizeof(array_count(self))); \
		(mem_area) = ((unsigned char *)(mem_area)) + sizeof(array_count(self)); \
		memcpy((mem_area), array_items(self), array_count(self) * sizeof(*array_items(self))); \
		mem_area = ((unsigned char *)(mem_area)) + array_count(self) * sizeof(*array_items(self)); \
	})

#define array_load(self, mem_area) ({ \
		memcpy(&array_count(self), (mem_area), sizeof(array_count(self))); \
		(mem_area) = ((unsigned char *)(mem_area)) + sizeof(array_count(self)); \
		array_capacity(self) = max(array_count(self), INIT_SIZE_ARRAY); \
		array_items(self) = rsalloc(array_capacity(self) * sizeof(*array_items(self))); \
		memcpy(array_items(self), (mem_area), array_count(self) * sizeof(*array_items(self))); \
		(mem_area) = ((unsigned char *)(mem_area)) + (array_count(self) * sizeof(*array_items(self))); \
	})

#endif /* ARRAY_H_ */
