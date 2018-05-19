/*
 * bitmap.h
 *
 *  Created on: 17 mag 2018
 *      Author: andrea
 */

#pragma once
#ifndef __BITMAP_DATATYPE_H_
#define __BITMAP_DATATYPE_H_

#include <string.h> // for memset()

/// This structure defines a generic bitmap.
typedef char rootsim_bitmap;


#define B_BLOCK_SIZE sizeof(unsigned)
#define B_NUM_BITS_PER_BLOCK (B_BLOCK_SIZE*CHAR_BIT)

#define B_SET_BIT_AT(B,K) ( B |= (1U << K) )
#define B_RESET_BIT_AT(B,K) ( B &= ~(1U << K) )
#define B_CHECK_BIT_AT(B,K) ( B & (1U << K) )

#define B_SET_BIT(A, I) B_SET_BIT_AT((A)[(I / B_NUM_BITS_PER_BLOCK)], (I % B_NUM_BITS_PER_BLOCK))
#define B_RESET_BIT(A, I) B_RESET_BIT_AT((A)[(I / B_NUM_BITS_PER_BLOCK)], (I % B_NUM_BITS_PER_BLOCK))
#define B_CHECK_BIT(A, I) B_CHECK_BIT_AT((A)[(I / B_NUM_BITS_PER_BLOCK)], (I % B_NUM_BITS_PER_BLOCK))


#define bitmap_required_size(requested_bits) (((requested_bits/B_NUM_BITS_PER_BLOCK) + (requested_bits%B_NUM_BITS_PER_BLOCK != 0))*B_BLOCK_SIZE)

#define bitmap_initialize(memory_pointer, requested_bits)  (memset(memory_pointer, 0, bitmap_required_size(requested_bits)))

#define bitmap_set(bitmap, bit_index) (B_SET_BIT(((unsigned*)(bitmap)), ((unsigned)(bit_index))))

#define bitmap_reset(bitmap, bit_index) (B_RESET_BIT(((unsigned*)(bitmap)), ((unsigned)(bit_index))))

#define bitmap_check(bitmap, bit_index) (B_CHECK_BIT(((const unsigned*)(bitmap)), ((unsigned)(bit_index))))


#endif /* __BITMAP_DATATYPE_H_ */
