/**
* @file arch/x86/linux/cross_state_manager/cross_state_manager.h
*
* @brief Per-thread page table
*
*
* This Linux kernel module implements a modification to the x86_64 page
* table management to support event cross state dependency tracking
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
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date November 15, 2013 - Initial version
* @date September 19, 2015 - Full restyle of the module, to use dynamic scheduler
* 			     patching
*/

#pragma once

//#ifdef HAVE_CROSS_STATE

#include <linux/ioctl.h>

#define CROSS_STATE_IOCTL_MAGIC 'R'

/* core user defined parameters */
#define SIBLING_PGD 128 // max number of concurrent memory views (concurrent root-sim worker threads on a node)
#define MAX_CROSS_STATE_DEPENDENCIES 1024 // max number of cross-state dependencied per LP at each event

#ifndef IOCTL_STRUCT
#define IOCTL_STRUCT

typedef struct _ioctl_info{
	int ds;
	void *addr;
	int id;
	unsigned int count;
	unsigned int *objects;
	unsigned mapped_processes;

	ulong callback;

	void *base_address;
	int page_count;
	int write_mode;
} ioctl_info;

#endif


#define ECS_MAJOR_FAULT	0
#define ECS_MINOR_FAULT	1
#define ECS_CHANGE_PAGE_PRIVILEGE	2

typedef struct _fault_info_t {
	long long rcx;
	long long rip;
	long long target_address;
	unsigned long target_gid;
	unsigned char fault_type;
} fault_info_t;

// Setup all ioctl commands
#define IOCTL_GET_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 1, void *)
#define IOCTL_SET_ANCESTOR_PGD _IO(CROSS_STATE_IOCTL_MAGIC, 2)
#define IOCTL_SET_VM_RANGE _IOW(CROSS_STATE_IOCTL_MAGIC, 3, ioctl_info *)
#define IOCTL_SCHEDULE_ON_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 4, ioctl_info *)
#define IOCTL_UNSCHEDULE_ON_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 5, int)
#define IOCTL_SET_PAGE_PRIVILEGE _IOW(CROSS_STATE_IOCTL_MAGIC, 6, int)
#define IOCTL_PROTECT_REMOTE_LP _IOW(CROSS_STATE_IOCTL_MAGIC, 7, int)

// Macros to access subportions of an address
#define PML4(addr) (((long long)(addr) >> 39) & 0x1ff)
#define PDP(addr)  (((long long)(addr) >> 30) & 0x1ff)
#define PDE(addr)  (((long long)(addr) >> 21) & 0x1ff)
#define PTE(addr)  (((long long)(addr) >> 12) & 0x1ff)

#define OBJECT_TO_PML4(object_id) ((ulong)object_id >> 9 )
#define OBJECT_TO_PDP(object_id) ((ulong)object_id &  0x1ff)
#define GET_ADDRESS(addr)  ( (((long long)(addr)) & ((1LL << 40) - 1)) >> 12)
#define PML4_PLUS_ONE(addr) (void *)((long long)(addr) + (1LL << 39))

#define MASK_PTADDR 0x07FFFFFFFFFFF000
#define MASK_PTCONT 0xF800000000000FFF

//#endif /* HAVE_CROSS_STATE */
