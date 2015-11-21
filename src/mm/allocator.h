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
* @file allocator.h
* @brief 
* @author Francesco Quaglia
*/


#pragma once
#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <core/core.h>

typedef struct _mem_map {
	char* base;   //base address of the chain of meta-data tables for the memory map of the sobj	
	int   size;   //maximum number of entries in the current meta-data tables of the memory map of the sobj
	int   active;   //number of valid entries in the meta-data tables of the memory map of the sobj
	char* live_bh; //address of the live bottom half for the sobj
	char* expired_bh; //address of the expired bottom half
	int   live_msgs; //number of messages currently present inthe the live bottom half
	int   live_offset; // offset of the oldest undelivered msg from the expired pool
	int   live_boundary; //memory occupancy (in bytes) of live messages
	int   expired_msgs; //number of messages currently present in the live bottom half
	int   expired_offset; // offset of the oldest undelivered msg from the expired pool
	int   expired_boundary; //memory occupancy (in bytes) of live messages
	char* actual_bh_addresses[2];// these are the stable pointers seen for ottom half buffers' migration across numa nodes
} mem_map; 

typedef struct _mdt_entry { //mdt stands for 'meta data table'
	char* addr;
	int   numpages;
} mdt_entry;

typedef struct _map_move {
	pthread_spinlock_t spinlock;
	unsigned 	   target_node;
	int      	   need_move;
	int    		   in_progress;
} map_move; 


#define MDT_PAGES	80
#define PAGE_SIZE (4*1<<10)
#define MDT_ENTRIES ((MDT_PAGES * PAGE_SIZE) / sizeof(mdt_entry))
#define MAX_SEGMENT_SIZE 16384 // this is expressed in number of pages
 
#define MAX_SOBJS  	MAX_LPs

#define SUCCESS 		0
#define FAILURE			-1
#define INVALID_SOBJS_COUNT 	-99
#define INIT_ERROR		-98
#define INVALID_SOBJ_ID 	-97
#define MDT_RELEASE_FAILURE	-96

char* allocate_mdt(void);
char *allocate_pages(int num_pages);
char* allocate_page(void);
mdt_entry* get_new_mdt_entry(int );
int allocator_init(unsigned int);
void* allocate_segment(unsigned int, size_t, bool);
void audit(void);
int release_mdt_entry(int);
void audit_map(unsigned int);
void set_daemon_maps(mem_map *, map_move* );
int init_move(int);
int lock(int);
int unlock(int);
void set_BH_map(mem_map* );
int init_BH(void);
int insert_BH(int , void*, int );
void* get_BH(int);
mem_map* get_m_map(int);

#ifdef HAVE_NUMA
int get_numa_node(int);
void move_sobj(int , unsigned );
void move_segment(mdt_entry *, unsigned );
int move_request(int , int );
#endif


#endif /* _ALLOCATOR_H */


