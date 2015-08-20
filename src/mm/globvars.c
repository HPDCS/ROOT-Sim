/**
*			Copyright (C) 2008-2014 HPDCS Group
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
* @file globvars.c
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <float.h>

#include <ROOT-Sim.h>
#include <globvars/globvars.h>
#include <arch/atomic.h>
#include <arch/thread.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>


/// Holds a pointer to the memory segment containing the global variables information
static globvar_shmem *globvars;

/// Hold a pointer to the shared memory segment containing nodes
static globvar_node *versions;

/// Number of variables accessed during the event
static __thread int access_index = 0;

/// This implements the access set used by LPs when processing events
static __thread int access_set[MAX_GLOBVAR_NUM];


// This are the symbols, filled by the linker, to discover where are global variables in memory
// See section "Source Code Reference"
// L'assunzione e` che il name mangling del compilatore sia disattivato (in gcc, di default, dovrebbe essere cosi`)
#ifndef GLOB_DEBUG
extern char __globvars_data_end,
	    __globvars_data_start,
	    __globvars_bss_start,
	    __globvars_bss_end;
#endif



/**
* This function finds a free entry for a variable version and marks it as allocated.
* It is thread safe, and uses a non-blocking approach.
* An entry in the thread is considered alloc'd if its alloc field is non-zero.
* This can be used to perform a safety check without using locks, and preventing
* the ABA problem on the alloc field.
*
* @author Alessandro Pellegrini
*
* @ret an index in the globvar_node versions array, which is not used by any other version
*/
static int global_version_alloc(void) {
	
	static int first_node_free = -1;

	int	first_slot,
		slot,
		allocd,
		mark;

	if(first_node_free == -1) {
		first_node_free = MAX_GLOBVAR_VER / n_ker * kid;
	}

	// Generate a system-wide unique mark
	mark = generate_mark(current_lp);

	// Start from what is marked as "first free node"	
	first_slot = slot = first_node_free;

	do {
		// Try to allocate, using an iCAS
		versions = (address(slot));
		allocd = versions -> alloc;
		
		if(allocd) {
			goto try_next;
		}
		
		if(!iCAS((volatile unsigned int *)&(address(slot))->alloc, allocd, mark)) {
		
		    try_next:
			// Try the next
			slot = (slot + 1) % (MAX_GLOBVAR_VER);

			// Is the space up?
			if(slot == first_slot) {
				rootsim_error(true, "Unable to allocate space for the global variable's version. Filled up all %d slots.\n",MAX_GLOBVAR_VER);
			}

			continue;

		}
		
		first_node_free = slot;

		break;

	} while(true);

	if(slot > (MAX_GLOBVAR_VER)) {
		printf("ERROR: %d > %d!!\n", slot, (MAX_GLOBVAR_VER));
	}

	return slot;
}

/**
* Based on the code by Timothy L. Harris. For further information see:
* Timothy L. Harris, "A Pragmatic Implementation of Non-Blocking Linked-Lists"
* Proceedings of the 15th International Symposium on Distributed Computing, 2001
*
* @author Alessandro Pellegrini
*/

// alloc Ã¨ del right node
static long long search_version_node(int globvar, simtime_t lvt, long long *left_node, int *alloc) {

	long long	left_node_next,
			right_node;

	long long head = aba_get_idx(globvars->variables[globvar].head);
	long long tail = aba_get_idx(globvars->variables[globvar].tail);
	
    search_again:
	do {
		long long t = head;
		long long t_next = (address(aba_get_idx(t)))->next;

		/* 1: Find left_node and right_node */
		do {
			// Questo confronto è sempre uno: non viene mai marcato qualcuno!!!
			if(!aba_is_marked(t_next)) {
				*alloc = (address(aba_get_idx(t_next)))->alloc;
				(*left_node) = t;
				left_node_next = t_next;
			}

			t = aba_unmark(t_next);
			t = t_next;
			if(aba_get_idx(t) == tail) {
				break;
			}

			t_next = (address(aba_get_idx(t)))->next;

		} while(aba_is_marked(t_next) || (address(aba_get_idx(t)))->lvt > lvt);

		right_node = t;
	
		// Il confronto è sempre true, entra dentro e il secondo confronto è sempre false poichè aba_is_marked() restituisce sempre
		// zero, quindi viene effettuato sempre il return  
		/* 2: Check if nodes are adjacent */
		if(aba_get_idx(left_node_next) == aba_get_idx(right_node)) {
			if((aba_get_idx(right_node) != tail) && aba_is_marked((address(aba_get_idx(right_node)))->next) ){
				goto search_again; /* G1 */
			} else {
				return right_node;
			}
		}
		
		/* 3: Remove one or more marked nodes */
		if(CAS((volatile long long unsigned int *)&((address(aba_get_idx(*left_node)))->next), left_node_next, right_node)) { /* C1 */

			if((aba_get_idx(right_node) != tail) && aba_is_marked((address(aba_get_idx(right_node)))->next)) {
				goto search_again; /* G2 */
			} else {
				return right_node; /* R2 */
			}
		} 
		
	} while(1);
}


static int insert_version_node(int globvar, simtime_t lvt, int value, long long *right) {

	int local_alloc;

	long long new_node = aba_next(global_version_alloc());
	
	long long right_node,
		  left_node;

	long long tail = aba_get_idx(globvars->variables[globvar].tail);
	
	// Populate entry
	versions = address(aba_get_idx(new_node));
	versions->lvt = lvt;
	versions->value = value;
	
	do {
		right_node = search_version_node(globvar, lvt, &left_node, &local_alloc);

		(*right) = right_node;

		if(aba_get_idx(right_node) != tail
		   && D_EQUAL((address(aba_get_idx(right_node)))->lvt, lvt)) {  /* T1 */
			return 0;
		}

		(address(aba_get_idx(new_node)))->next = aba_next(aba_get_idx(right_node));
		
		if(CAS((volatile unsigned long long *)&((address(aba_get_idx(left_node)))->next), right_node, new_node)) {  /* C1*/
			return 1;
		}
	} while(1); /* B3 */
}

static int find_version_node(int globvar, simtime_t lvt, long long *left_node, int *alloc) {

	long long right_node;

	right_node = search_version_node(globvar, lvt, left_node, alloc);

	if((address(aba_get_idx(right_node)))->lvt > lvt) {
		return -1;
	} else {
		return aba_get_idx(right_node);
	}
}


/**
* This function initializes data structures needed for a global variable.
* It is not thread safe, so must be called in a section which is executed
* by the master kernel only, before any LP tries to 
*
* @author Alessandro Pellegrini
*/
static void register_global_variable(void *orig_addr, unsigned short int size) {

	int slot;
	int vers_slot;
	
	if(globvars->num_vars == MAX_GLOBVAR_NUM) {
		rootsim_error(true, "Unable allocate more than %d global variables\n", MAX_GLOBVAR_NUM);
	}

	// Take an available slot
	slot = (int)((long)orig_addr & GLOBVAR_MASK);
	while(globvars->variables[slot].orig_addr != NULL) {
		slot = (slot + 1) % MAX_GLOBVAR_NUM;
	}

	// Store the mapping between the variables
	globvars->variables[slot].orig_addr = orig_addr;
	globvars->variables[slot].size = size; // TODO: how to handle movsinstructions?

	// Allocate and initialize a dummy version node which will act as a head forever
	vers_slot = global_version_alloc();
	globvars->variables[slot].head = aba_next(vers_slot);
	(address(vers_slot))->lvt = DBL_MAX; // No simulation can reach this time!

	// Allocate the first version node
	(address(vers_slot))->next = aba_next(global_version_alloc());

	// Initialize the first version's node
	vers_slot = aba_get_idx((address(vers_slot))->next);
	(address(vers_slot))->lvt = -0.5; // That's the beginning of the simulation, right before INIT


	// Copy the initial value
	memcpy(&(address(vers_slot))->value,orig_addr,size);

	// Initialize the tail
	(address(vers_slot))->next = aba_next(global_version_alloc());
	vers_slot = aba_get_idx((address(vers_slot))->next);
	(address(vers_slot))->next = aba_next(-1);
	(address(vers_slot))->lvt = -1; // Sentinel Node
	globvars->variables[slot].tail = aba_next(vers_slot);

	// We have just set another variable
	globvars->num_vars++;
}






/**
* This function initilizes the global variables subsystem.
* Relies on the creation of a dummy file for generating a random key for getting
* a shared memory segment.
* The function tries indefinitely to get a segment.
*
* @author Alessandro Pellegrini
*
* @todo This might cause a deadlock if we're asking for too much shared memory.
*       A check on errno must be added.
*/
void globvars_init(void) {
	unsigned int i;
	FILE *globvars_conf;
	char conf_line[512];
	
	if(master_thread()) {

		globvars = (globvar_shmem *)malloc(shm_size());	
		
		bzero(globvars, shm_size());
	}

	// Set to -1 (no reference) every next in the versions list
	for(i = 0; i < MAX_GLOBVAR_VER; i++) {
		versions = address(i);
		versions -> next = aba_next(-1);
	}

	// Now initialize the variables pointers
	if(master_thread()) {

		// TODO: this must change: it's not good to force the user to keep external files.
		// One hack is to verbatim append the content of the globvars file to the end of the
		// actual executable at the end of compilation, and here retrieve the text from there...
		globvars_conf = fopen("globvars", "r");
		if(globvars_conf == NULL) {
			rootsim_error(true, "Unable to load global variables information. Aborting...\n");
		}

		while(fgets(conf_line, 512, globvars_conf) != NULL) {
			char *token;
			void *addr;
			unsigned short int size;

			// Tokenize the string. Parameters are: addres size name. Name can be discarded here
			token = strtok(conf_line, " ");
			addr = parseHex(token);
			token = strtok(NULL, " ");
			size = (unsigned short int)parseInt(token);
		
			register_global_variable(addr, size);
		}

		fclose(globvars_conf);

	}
}



/**
* This function finalizes the core structures of ROOT-Sim, just before terminating a simulation
*
* @author Alessandro Pellegrini
*/
void globvars_fini(void) {
	if(master_thread()) {
		free(globvars);
	}
}

/**
* This function reads a value from a global variable
*
* @author Alessandro Pellegrini
*
* @ret the global variable being read
*/
long long read_global_variable(void *orig_addr, simtime_t my_lvt) {
	int slot;
	int version;
	int local_alloc;
	long long left_node;

	// Find where is the list related to the variable
	slot = (int)((long)orig_addr & GLOBVAR_MASK);
	while(globvars->variables[slot].orig_addr != orig_addr) {
		slot = (slot + 1) % MAX_GLOBVAR_NUM;
	}

	// Check whether this variable was already accessed during this event's execution
	if(access_set[slot] != -1 && (address(access_set[slot]))->alloc) {  

		// Already accessed and still valid FIXME maybe an ABA problem here!
		version = access_set[slot];
	
	} else {
		// Search for the list related to the variable and its version node
		do {
			// Find the best version wrt my lvt
			version = find_version_node(slot, my_lvt, &left_node, &local_alloc);
			if(version == -1) {
				printf("Unable to find a valid version for the variable at LVT %f\n", my_lvt);
				return 0;
			}
			
			// Store the node index into the access set
			access_set[slot] = version;

			globvar_node *temp = address(version);

			// Get the spinlock
			if(current->state != LP_STATE_SILENT_EXEC)
				spin_lock(&temp->read_list_spinlock);
	
			// Check if alloc has changed
			if(local_alloc != temp->alloc) {
				if(current->state != LP_STATE_SILENT_EXEC)
					spin_unlock(&temp->read_list_spinlock);
				continue;
			}

			/* We're executing alone here! */
			
			if ( D_DIFFER(temp->lvt, my_lvt) ) {
				
				// Record the maximum access time for this version and this LP
				if ( temp->read_time[LidToGid(current_lp)] < my_lvt ) {
					temp->read_time[LidToGid(current_lp)] = my_lvt;
					
					SET_DIRTY_BIT_BITMAP(temp,LidToGid(current_lp));
				}
			}

			if(current->state != LP_STATE_SILENT_EXEC)
				spin_unlock(&temp->read_list_spinlock);

			break;

		} while(true);
	}
	
	// Return the version's value
	return (address(version))->value;
}


/**
* This function writes a new value into a global variable
*
* @author Alessandro Pellegrini
*
* @param orig_addr the original address the global variable was to be found at
* @param lvt the logical virtual time associated with the variable's write
* @param val the value to be written in the global variable
*/
void write_global_variable(void *orig_addr, simtime_t lvt, long long val) {
	int slot;
	long long right_node;
	unsigned int i, j;
	simtime_t access_lvt;
	globvar_node *temp;

	// In silent execution, we don't need to write again the variable
	if(current->state == LP_STATE_SILENT_EXEC)
		return;

	// Find where is the list related to the variable
	slot = (int)((long)orig_addr & GLOBVAR_MASK);
	while(globvars->variables[slot].orig_addr != orig_addr) {
		slot = (slot + 1) % MAX_GLOBVAR_NUM;
	}

	// Add a new version node
	if(!insert_version_node(slot, lvt, val, &right_node)) {
		// Two different values at the same LVT!
		// TODO This must be handled somehow!!!!
	}

	// Reset the access set for this variable
	access_set[slot] = -1;
	
	temp = (address(slot));
	
	spin_lock(&temp->read_list_spinlock);
	
	// TODO: qui inserire il controllo sulla bitmap
	for(i = 0; i < (MAX_LPs / NUM_CHUNKS_PER_BLOCK_BITMAP) ; i++) {
	
		if ( temp->bitmap[i] != 0 ) {
		
			for ( j = 0 ; j < NUM_CHUNKS_PER_BLOCK_BITMAP ; j++ ) {

				unsigned int current_check = (i * MAX_LPs/NUM_CHUNKS_PER_BLOCK_BITMAP) + j;
				
				if(current_check >= n_prc_tot) {
					goto done;
				}

				if ( current_check != current_lp ) {

					access_lvt = (address(aba_get_idx(right_node)))->read_time[current_check];
				
					if (access_lvt > lvt ) {
						ScheduleNewEvent(current_check, lvt, 789654, NULL, 0);
						RESET_DIRTY_BIT_BITMAP(temp, current_check);
					}
				}
			}
		}
	}
	
    done:
	
	spin_unlock(&temp->read_list_spinlock);
}



/**
* This function actually prunes the list. The implementation is non-reentrant,
* so it must be only called from prune_globvars, which ensures execution in isolation.
*
* @author Alessandro Pellegrini
*
* @param gvt the global virtual time where to prune the list at
*/
static void prune_list(int var, simtime_t gvt) {
	int last_node;
	int curr_node;
	int curr_node_next;
	int local_alloc;
	int tail;
	long long l;
	
	if(globvars->variables[var].orig_addr == NULL)
		return;

	// Find the frontier node
	last_node = aba_get_idx(find_version_node(var, gvt, &l, &local_alloc));

	// Get the tail
	tail = aba_get_idx(globvars->variables[var].tail);
	
	// If last node is the last valid node in the list, I have nothing to do!
	if ( aba_get_idx((address(last_node))->next) == tail )
		return;

	// We will start freeing from the next node
	curr_node = aba_get_idx((address(last_node))->next);

	// This node must be directly connected to the tail
	(address(last_node))->next = aba_next(tail);	

	// Free all the other nodes
	while(curr_node != tail) {
		curr_node_next = aba_get_idx((address(curr_node))->next);
		(address(curr_node))->next = aba_next(-1);
		(address(curr_node))->alloc = 0;
		curr_node = curr_node_next;
	}
}


/**
* This function divides variables evenly
* across kernel instances, so to allow a non reentrant implementation
* of list pruning. Then, the actual function to prune the lists is called.
* 
* @author Alessandro Pellegrini
*
* @param gvt the global virtual time where to prune the list at
*/
static void prune_globvars(simtime_t gvt) {
	register unsigned int i = 0;
	unsigned int j;
	unsigned int buf1;
	unsigned int block_leftover;
	unsigned int offset;
	unsigned int variables[MAX_GLOBVAR_NUM];

	bzero(variables, sizeof(int) * MAX_GLOBVAR_NUM);

	// Divide variables evenly across kernel instances
	buf1 = (MAX_GLOBVAR_NUM / n_ker);
	block_leftover = MAX_GLOBVAR_NUM - buf1 * n_ker;

	// It's a hack to bypass the first check that sets offset to 0
	if (block_leftover > 0)
		buf1++;

	offset = 0;
	while (i < MAX_GLOBVAR_NUM) {
		j = 0;

		while (j < buf1) {
			variables[i] = offset;
			i++;
			j++;
		}

		offset++;
		block_leftover--;

		if (block_leftover == 0)
			buf1--;

	}

	// Now prune the lists
	for(i = 0; i < MAX_GLOBVAR_NUM; i++) {
		if(variables[i] == kid) {
			prune_list(i, gvt);
		}
	}
}



/**
* 
*
* @author
*
* @param
*/void globvars_on_gvt(simtime_t gvt) {

	// Prune the globvars list
	prune_globvars(gvt);
}




/**
* 
*
* @author
*
* @param
*/
void globvars_init_event(void) {

	// No variable has been accessed yet
	access_index = 0;

	// Reset access set
	memset(access_set, -1, sizeof(int) * MAX_GLOBVAR_NUM);
}



/**
* 
*
* @author
*
* @param
*/
void globvars_rollback(simtime_t rollback_time) {

}
