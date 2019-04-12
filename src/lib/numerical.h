/**
* @file lib/numerical.h
*
* @brief Numerical Library
*
* Piece-Wise Deterministic Random Number Generators.
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
* @author Alessandro Pellegrini
*
* @date March 16, 2011
*/

#pragma once

#include <stdbool.h>

/// Numerical seed type
typedef uint64_t seed_type;

/**
 * This structure keeps track of the per-LP members required to rollback
 * the internal state of the simulation library.
 */
typedef struct _numerical_state {
	seed_type seed;	      /**< Random seed */
	double gset;	      /**< Normal distribution saved sample */
	bool iset;	      /**< Normal distribution saved sample flag */
} numerical_state_t;

void numerical_init(void);


// without this stuff dijkstra on thousands of nodes would probably fail horribly:
// this is needed in order to lose less precision on hundreds of double additions

/// this represents a partial Neumaier sum
struct _sum_helper_t{
	double sum;
	double crt;
};

double 			NeumaierSum	(unsigned cnt, double addendums[cnt]);
struct _sum_helper_t 	PartialNeumaierSum(struct _sum_helper_t sh, double addendum);

#define ValueSumHelper(a) (a.crt + a.sum)

#define CmpSumHelpers(a, b) ({\
	double __a_s = a.sum + a.crt;\
	double __b_s = b.sum + b.crt;\
	(__a_s > __b_s) - (__b_s > __a_s);\
})

