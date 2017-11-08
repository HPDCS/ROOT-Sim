/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file ktblmgr.h
* @brief This is the main header for the Linux Kernel Module which implements
*	per-kernel-thread different page table for supporting shared state.
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date November 15, 2013
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
} ioctl_info;

#endif

typedef struct _fault_info_t {
	long long rcx;
	long long rip;
	long long target_address;
	unsigned long target_gid;
} fault_info_t;

// Setup all ioctl commands
#define IOCTL_INIT_PGD _IO(CROSS_STATE_IOCTL_MAGIC, 0)
#define IOCTL_GET_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 1, void *)
#define IOCTL_RELEASE_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 2 , int)
#define IOCTL_INSTALL_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 3 , int)
#define IOCTL_UNINSTALL_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 4 , int)
#define IOCTL_GET_INFO_PGD _IO(CROSS_STATE_IOCTL_MAGIC, 5)
#define IOCTL_GET_INFO_VMAREA _IO(CROSS_STATE_IOCTL_MAGIC, 6)
#define IOCTL_GET_CR_REGISTERS _IO(CROSS_STATE_IOCTL_MAGIC, 7)
#define IOCTL_TRACE_VMAREA _IOW(CROSS_STATE_IOCTL_MAGIC, 8 , void*)
#define IOCTL_CHANGE_VIEW _IOW(CROSS_STATE_IOCTL_MAGIC, 10 , int)
#define IOCTL_SYNC_SLAVES _IO(CROSS_STATE_IOCTL_MAGIC, 11)
#define IOCTL_SCHEDULE_ID _IOW(CROSS_STATE_IOCTL_MAGIC, 12, int)
#define IOCTL_UNSCHEDULE_CURRENT _IO(CROSS_STATE_IOCTL_MAGIC, 13)
#define IOCTL_CHANGE_MODE_VMAREA _IOW(CROSS_STATE_IOCTL_MAGIC, 14 , void *)
#define IOCTL_SET_ANCESTOR_PGD _IO(CROSS_STATE_IOCTL_MAGIC, 15 )
#define IOCTL_SET_VM_RANGE _IOW(CROSS_STATE_IOCTL_MAGIC, 16 , ioctl_info *)
#define IOCTL_REGISTER_THREAD _IOW(CROSS_STATE_IOCTL_MAGIC, 17 , int )
#define IOCTL_DEREGISTER_THREAD _IOW(CROSS_STATE_IOCTL_MAGIC, 18  , int)
#define IOCTL_RESTORE_VIEW _IOW(CROSS_STATE_IOCTL_MAGIC, 19 , int)
#define IOCTL_SCHEDULE_ON_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 21 , ioctl_info *)
#define IOCTL_UNSCHEDULE_ON_PGD _IOW(CROSS_STATE_IOCTL_MAGIC, 22 , int)
#define IOCTL_GET_FREE_PML4 _IO(CROSS_STATE_IOCTL_MAGIC, 23)
#define IOCTL_PGD_PRINT _IO(CROSS_STATE_IOCTL_MAGIC, 24)


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
