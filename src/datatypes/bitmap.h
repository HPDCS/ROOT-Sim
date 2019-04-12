/**
* @file datatypes/bitmap.h
*
* @brief Bitmap data type
*
* This a simple bitmap implemented with some simple macros.
* Keep in mind that some trust is given to the developer since
* the implementation, for performances and simplicity
* reasons, doesn't remember his effective size; consequently
* it doesn't check boundaries on the array that stores the bits.

*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
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
* @author Andrea Piccione
*
* @date 26 Oct 2018
*/

#pragma once

#include <limits.h>		// for CHAR_BIT
#include <memory.h>		// for memset()
#include <core/core.h>		// for UNION_CAST()

/// This defines a generic bitmap.
typedef unsigned char rootsim_bitmap;

/* macros for internal use */

#define B_BLOCK_TYPE unsigned
#define B_BLOCK_SIZE ((unsigned)sizeof(B_BLOCK_TYPE))
#define B_BITS_PER_BLOCK (B_BLOCK_SIZE*CHAR_BIT)
#define B_MASK ((B_BLOCK_TYPE)1U)
#define B_UNION_CAST(bitmap) UNION_CAST(((rootsim_bitmap*)(bitmap)), B_BLOCK_TYPE*)

// we leverage the fact that B_BITS_PER_BLOCK is a power of 2 in any real architecture
#define B_MOD_OF_BPB(n) (((unsigned)(n)) & ((unsigned)(B_BITS_PER_BLOCK - 1)))

#define B_SET_BIT_AT(B,K) 	( B |= (B_MASK << K) )
#define B_RESET_BIT_AT(B,K) ( B &= ~(B_MASK << K) )
#define B_CHECK_BIT_AT(B,K) ( B & (B_MASK << K) )

#define B_SET_BIT(A, I) 	B_SET_BIT_AT((A)[((I) / B_BITS_PER_BLOCK)], (B_MOD_OF_BPB(I)))
#define B_RESET_BIT(A, I) 	B_RESET_BIT_AT((A)[((I) / B_BITS_PER_BLOCK)], (B_MOD_OF_BPB(I)))
#define B_CHECK_BIT(A, I) 	B_CHECK_BIT_AT((A)[((I) / B_BITS_PER_BLOCK)], (B_MOD_OF_BPB(I)))

#define B_CTZ(x)	(\
		__builtin_choose_expr( \
		__builtin_types_compatible_p (__typeof__ (x), unsigned), __builtin_ctz(x),\
		__builtin_choose_expr(\
		__builtin_types_compatible_p (__typeof__ (x), unsigned long), __builtin_ctzl(x),\
		__builtin_choose_expr(\
		__builtin_types_compatible_p (__typeof__ (x), unsigned long long), __builtin_ctzll(x),\
		(void)0))))

#define B_POPC(x)	(\
		__builtin_choose_expr( \
		__builtin_types_compatible_p (__typeof__ (x), unsigned), __builtin_popcount(x),\
		__builtin_choose_expr(\
		__builtin_types_compatible_p (__typeof__ (x), unsigned long), __builtin_popcountl(x),\
		__builtin_choose_expr(\
		__builtin_types_compatible_p (__typeof__ (x), unsigned long long), __builtin_popcountll(x),\
		(void)0))))

/*!
 * @brief Computes the required size of a bitmap with @a requested_bits entries.
 * @param requested_bits the requested number of bits.
 * @returns the size in bytes of the requested bitmap.
 *
 * For example this statically declares a 100 entries bitmap and initializes it:
 * 		rootsim_bitmap my_bitmap[bitmap_required_size(100)] = {0};
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_required_size(requested_bits) ((((requested_bits)/B_BITS_PER_BLOCK) + (B_MOD_OF_BPB(requested_bits) != 0))*B_BLOCK_SIZE)

/*!
 * @brief This initializes the bitmap at @a memory_pointer containing @a requested_bits entries.
 * @param memory_pointer the pointer to the bitmap to initialize.
 * @param requested_bits the number of bits contained in the bitmap.
 * @returns the very same @a memory_pointer
 *
 * The argument @a requested_bits is necessary since the bitmap is "dumb"
 * For example this dynamically declares a 100 entries bitmap and initializes it:
 * 		rootsim_bitmap *my_bitmap = malloc(bitmap_required_size(100));
 * 		bitmap_initialize(my_bitmap, 100);
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_initialize(memory_pointer, requested_bits)  (memset(memory_pointer, 0, bitmap_required_size(requested_bits)))

/*!
 * @brief This sets the bit with index @a bit_index of the bitmap @a bitmap
 * @param bitmap a pointer to the bitmap to write.
 * @param bit_index the index of the bit to set.
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_set(bitmap, bit_index) (B_SET_BIT(B_UNION_CAST(bitmap), ((unsigned)(bit_index))))

/*!
 * @brief This resets the bit with index @a bit_index of the bitmap @a bitmap
 * @param bitmap a pointer to the bitmap to write.
 * @param bit_index the index of the bit to reset.
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_reset(bitmap, bit_index) (B_RESET_BIT(B_UNION_CAST(bitmap), ((unsigned)(bit_index))))

/*!
 * @brief This checks if the bit with index @a bit_index of the bitmap @a bitmap is set or unset.
 * @param bitmap a pointer to the bitmap.
 * @param bit_index the index of the bit to read
 * @return 0 if the bit is not set, 1 otherwise
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_check(bitmap, bit_index) (B_CHECK_BIT(B_UNION_CAST(bitmap), ((unsigned)(bit_index))) != 0)

/*!
 * @brief This counts the occurrences of cleared bits in the bitmap @a bitmap.
 * @param bitmap a pointer to the bitmap.
 * @param bitmap_size the size of the bitmap in bytes (obtainable through bitmap_required_size())
 * @return the number of cleared bits in the bitmap
 *
 *	XXX: maybe the unsigned variables holding the indexes and the counts are too limited for our application!
 *	This macro expects the number of bits in the bitmap to be a multiple of B_BITS_PER_BLOCK.
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_count_reset(bitmap, bitmap_size) ({				\
		unsigned __i, __blocks = bitmap_size / B_BLOCK_SIZE;		\
		unsigned __ret = bitmap_size * CHAR_BIT;			\
		B_BLOCK_TYPE __cur_block, *__block_b = B_UNION_CAST(bitmap);	\
		for(__i = 0; __i < __blocks; ++__i){				\
			if((__cur_block = __block_b[__i])){			\
				__ret -= B_POPC(__cur_block);			\
			}							\
		}								\
		__ret; 								\
	})

/*!
 * @brief This returns the index of the first cleared bit in @a bitmap.
 * @param bitmap a pointer to the bitmap.
 * @param bitmap_size the size of the bitmap in bytes (obtainable through bitmap_required_size())
 * @return the index of the first cleared bit in the bitmap, UINT_MAX if none is found.
 *
 *	XXX: maybe the unsigned variables holding the indexes and the counts are too limited for our application!
 *	This macro expects the number of bits in the bitmap to be a multiple of B_BITS_PER_BLOCK.
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_first_reset(bitmap, bitmap_size)	({				\
		unsigned __i, __blocks = bitmap_size / B_BLOCK_SIZE;		\
		unsigned __ret = UINT_MAX;					\
		B_BLOCK_TYPE __cur_block, *__block_b = B_UNION_CAST(bitmap);	\
		for(__i = 0; __i < __blocks; ++__i){				\
			if((__cur_block = ~__block_b[__i])){			\
				__ret = B_CTZ(__cur_block);			\
				break;						\
			}							\
		}								\
		__ret; 								\
	})

/*!
 * @brief This executes a user supplied function for each set bit in @a bitmap.
 * @param bitmap a pointer to the bitmap.
 * @param bitmap_size the size of the bitmap in bytes (obtainable through bitmap_required_size())
 * @param func a function which takes a single unsigned argument, the index of the current set bit.
 *
 *	XXX: maybe the unsigned variables holding the indexes and the counts are too limited for our application!
 *	This macro expects the number of bits in the bitmap to be a multiple of B_BITS_PER_BLOCK.
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_foreach_set(bitmap, bitmap_size, func) ({ 			\
		unsigned __i, __fnd, __blocks = bitmap_size / B_BLOCK_SIZE;	\
		B_BLOCK_TYPE __cur_block, *__block_b = B_UNION_CAST(bitmap);	\
		for(__i = 0; __i < __blocks; ++__i){				\
			if((__cur_block = __block_b[__i])){			\
				do{						\
					__fnd = B_CTZ(__cur_block);		\
					B_RESET_BIT_AT(__cur_block, __fnd);	\
					func((__fnd + __i * B_BITS_PER_BLOCK));	\
				}while(__cur_block);				\
			}							\
		}								\
	})
