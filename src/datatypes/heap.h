/*
 * heap.h
 *
 *  Created on: 30 giu 2018
 *      Author: andrea
 */

#ifndef __HEAP_H_
#define __HEAP_H_

#include <datatypes/array.h>

#define rootsim_heap(type) rootsim_array(type)

#define heap_init(self)	array_init(self)

#define heap_empty(self) array_empty(self)

#define heap_insert(self, elem, cmp_f) ({\
		__typeof(array_count(self)) __i_h = array_count(self);\
		array_push(self, elem);\
		while(__i_h && cmp_f(elem, array_items(self)[(__i_h - 1)/2]) < 0){\
			array_items(self)[__i_h] = array_items(self)[(__i_h - 1)/2];\
			__i_h = (__i_h - 1)/2;\
		}\
		array_items(self)[__i_h] = elem;\
	})

#define heap_min(self) ((const)(array_items(self)[0]))

#define heap_extract(self, cmp_f) ({\
		__typeof(*array_items(self)) __ret_h = array_items(self)[0]; \
		__typeof(*array_items(self)) __last_h = array_pop(self); \
		__typeof(array_count(self)) __i_h = 0; \
		do{ \
			__i_h <<= 1; \
			++__i_h; \
			if(__i_h >= array_count(self)){\
				break; \
			} \
			if(__i_h + 1 < array_count(self)){ \
				if(cmp_f(array_items(self)[__i_h + 1], array_items(self)[__i_h]) < 0) {\
					++__i_h;\
				} \
			}\
			if(cmp_f(array_items(self)[__i_h], __last_h) < 0) {\
				array_items(self)[(__i_h-1) >> 1] = array_items(self)[__i_h]; \
			} \
			else{ \
				break; \
			}\
		} while(1); \
		array_items(self)[(__i_h-1) >> 1] = __last_h; \
		__ret_h; \
	})

#endif /* __HEAP_H_ */
