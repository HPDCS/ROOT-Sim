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
* @file bitmap.h
* @date 26 Oct 2018
* @brief This header implements a minimal bitmap data type
* @author Andrea Piccione
*
* This a simple bitmap implemented with some simple macros.
* Keep in mind that some trust is given to the developer since
* the implementation, for performances and simplicity
* reasons, doesn't remember his effective size; consequently
* it doesn't check boundaries on the array that stores the bits.
*/

#pragma once
#ifndef __BITMAP_DATATYPE_H_
#define __BITMAP_DATATYPE_H_

#include <limits.h> // for CHAR_BIT
#include <memory.h> // for memset()

/// This defines a generic bitmap.
typedef unsigned char rootsim_bitmap;

/* macros for internal use */

#define B_BASETYPE unsigned
#define B_MASK 0x00000001

#define B_SAFE_CAST_TO_BLOCK(x) (((union {rootsim_bitmap* a; B_BASETYPE* b;})(x)).b)
#define B_BLOCK_SIZE sizeof(B_BASETYPE)
#define B_BITS_PER_BLOCK (B_BLOCK_SIZE*CHAR_BIT)

#define B_SET_BIT_AT(B,K) ( B |= (B_MASK << K) )
#define B_RESET_BIT_AT(B,K) ( B &= ~(B_MASK << K) )
#define B_CHECK_BIT_AT(B,K) ( B & (B_MASK << K) )

#define B_SET_BIT(A, I) B_SET_BIT_AT((A)[((I) / B_BITS_PER_BLOCK)], ((I) % B_BITS_PER_BLOCK))
#define B_RESET_BIT(A, I) B_RESET_BIT_AT((A)[((I) / B_BITS_PER_BLOCK)], ((I) % B_BITS_PER_BLOCK))
#define B_CHECK_BIT(A, I) B_CHECK_BIT_AT((A)[((I) / B_BITS_PER_BLOCK)], ((I) % B_BITS_PER_BLOCK))

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
#define bitmap_required_size(requested_bits) ((((requested_bits)/B_BITS_PER_BLOCK) + ((requested_bits)%B_BITS_PER_BLOCK != 0))*B_BLOCK_SIZE)

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
#define bitmap_set(bitmap, bit_index) (B_SET_BIT(B_SAFE_CAST_TO_BLOCK(bitmap), ((B_BASETYPE)(bit_index))))

/*!
 * @brief This resets the bit with index @a bit_index of the bitmap @a bitmap
 * @param bitmap a pointer to the bitmap to write.
 * @param bit_index the index of the bit to reset.
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_reset(bitmap, bit_index) (B_RESET_BIT(B_SAFE_CAST_TO_BLOCK(bitmap), ((B_BASETYPE)(bit_index))))

/*!
 * @brief This puts the value @a value (true or false) in the bit with index @a bit_index of the bitmap @a bitmap
 * @param bitmap a pointer to the bitmap to write.
 * @param bit_index the index of the bit to set/reset.
 * @param value the value to be written.
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_put(bitmap, bit_index, value) ((value) ? bitmap_set(bitmap, bit_index) : bitmap_reset(bitmap, bit_index))

/*!
 * @brief This checks if the bit with index @a bit_index of the bitmap @a bitmap is set or unset.
 * @param bitmap a pointer to the bitmap.
 * @param bit_index the index of the bit to read
 * @return 0 if the bit is not set, 1 otherwise
 *
 * 	Care to avoid side effects in the arguments because they may be evaluated more than once
 */
#define bitmap_check(bitmap, bit_index) (B_CHECK_BIT(B_SAFE_CAST_TO_BLOCK(bitmap), ((B_BASETYPE)(bit_index))) != 0)


#endif /* __BITMAP_DATATYPE_H_ */
