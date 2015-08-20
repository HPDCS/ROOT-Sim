/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file globvars.h
* @brief Global Variables: this module gives support for using global variables within the 
*        application level programming, allowing the simulation states of the LPs not being
*        completely disjoint.
* 
* 	 For the description of the preliminary implementation refer to the paper:
* 
* 	 Alessandro Pellegrini, Roberto Vitali, Sebastiano Peluso and Francesco Quaglia
*	 Transparent and Efficient Shared-State Management for Optimistic Simulations on Multi-core Machines
*	 In Proceedings 20th International Symposium on Modeling, Analysis and Simulation of Computer and Telecommunication Systems (MASCOTS), pp. 134–141, Arlington, VA, USA, IEEE Computer Society, August 2012.
* @author Alessandro Pellegrini
* @date Jan 24, 2012: Initial Version
* 
* 	Nov 11, 2014: Ported to Multithreaded ROOT-Sim

*/
#pragma once
#ifndef _GLOBVARS_H
#define _GLOBVARS_H


#include <arch/atomic.h>
#include <core/core.h>

/// Number of global variables allowed. Must be a power of 2.
#define MAX_GLOBVAR_NUM		32

/// Number of versions nodes available
#define MAX_GLOBVAR_VER		100000

/// Number of events allowed to read on a variable version
#define MAX_GLOBVAR_READ	10

/// Number of chunks for one block of the bitmap
#define NUM_CHUNKS_PER_BLOCK_BITMAP (8 * sizeof(unsigned int))

/// This structure defines a version node for a global variable
typedef struct _globvar_node {
	/// Is this node allocated?
	volatile int 	alloc;
	/// The LVT associated with the version
	simtime_t	lvt;
	/// The payload of the node
	unsigned long long value; 	// TODO: mettere qui un buffer abbastanza grande...
	/// Read list's spinlock
	spinlock_t	read_list_spinlock;
	/// A "pointer" to the next entry in the versions list
	long long	next;
	/// Bitamp
	unsigned int bitmap[MAX_LPs / NUM_CHUNKS_PER_BLOCK_BITMAP];
	/// Counter Read and Time
	simtime_t	read_time[];
} globvar_node;


/// This structure keeps information on the global variable
typedef struct _globvar_info {
	void			*orig_addr;
	unsigned short int	size;
	long long		head;
	long long		tail;
} globvar_info;


/// This structure defines the map of the shared memory
typedef struct _globval_shmem {
	int num_vars;
	globvar_info	variables[MAX_GLOBVAR_NUM];
	volatile int	first_node_free;
} globvar_shmem;



/// This structure is used to order the aggregation matrix
typedef struct _matrix_order {
	int	accesses;
	int	variable;
} matrix_order;



/// Fast preprocessed mask for the hash function
#define GLOBVAR_MASK	(~(-MAX_GLOBVAR_NUM))


/// Predicate for aggregation decision
#define can_aggregate(i,j)	((atomic_read(&globvars->correlation_matrix[(i)][(j)]) / atomic_read(&globvars->correlation_matrix[(i)][(i)]) > AGGREGATE_THRESHOLD) && (atomic_read(&globvars->correlation_matrix[(i)][(j)]) / atomic_read(&globvars->correlation_matrix[(j)][(j)]) > AGGREGATE_THRESHOLD))


/// Macro for composing the next pointer to avoid ABA problem and storing the marked bit
#define aba_next(next)			((\
					(((unsigned long long)generate_mark(current_lp)) << (sizeof(int) * 8))\
					| (unsigned int)next \
					) & ~((long long)1 << ((sizeof(long long) * 8 - 1))) \
					)

#define aba_is_marked(B)		(B >> ((sizeof(long long) * 8) - 1))

#define aba_mark(B)			(B |= ((long long)1 << ((sizeof(long long) * 8 - 1))))

#define aba_unmark(B)			(B &= ~((long long)1 << ((sizeof(long long) * 8 - 1))))

#define aba_get_idx(B)			((int)(B))

#define shm_size()			(int)((sizeof(globvar_shmem) + (MAX_GLOBVAR_VER * (sizeof(globvar_node) + (sizeof(simtime_t) * n_prc_tot)))))

#define address(slot)			(globvar_node *)(base() + (slot * (sizeof(globvar_node) + (sizeof(simtime_t) * n_prc_tot))))

#define base()				(long long int)((long long int)globvars + sizeof(globvar_shmem)) 

/* Defines to handle the bitmap */
#define BLOCK_SIZE sizeof(unsigned int)

#define SET_BIT_AT_BITMAP(B,K) ( B |= (MASK << K) )
#define RESET_BIT_AT_BITMAP(B,K) ( B &= ~(MASK << K) )
#define CHECK_BIT_AT_BITMAP(B,K) ( B & (MASK << K) )

#define CHECK_DIRTY_BIT_BITMAP(A,I) ( CHECK_BIT_AT_BITMAP(									\
			((unsigned int*)(((globvar_node *)A)->bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK_BITMAP)],\
			((int)I % NUM_CHUNKS_PER_BLOCK_BITMAP)) )
#define SET_DIRTY_BIT_BITMAP(A,I) ( SET_BIT_AT_BITMAP(									\
			((unsigned int*)(((globvar_node *)A)->bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK_BITMAP)],\
			((int)I % NUM_CHUNKS_PER_BLOCK_BITMAP)) )
#define RESET_DIRTY_BIT_BITMAP(A,I) ( RESET_BIT_AT_BITMAP(									\
			((unsigned int*)(((globvar_node*)A)->bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK_BITMAP)],\
			((int)I % NUM_CHUNKS_PER_BLOCK_BITMAP)) )



extern void globvars_init(void);
extern void globvars_fini(void);
extern void write_global_variable(void *orig_addr, simtime_t lvt, long long val);
extern long long read_global_variable(void *orig_addr, simtime_t my_lvt);
extern void globvars_init_event(void);
extern void globvars_on_gvt(simtime_t gvt);
extern void globvars_rollback(simtime_t rollback_time);


#endif /* _GLOBVARS_H */

