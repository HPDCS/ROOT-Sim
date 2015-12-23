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
* @file group.h 
* @brief //TODO MN
* @author Nazzareno Marziale
* @author Francesco Nobilia
*
* @date September 23, 2015
*/

#include <arch/atomic.h>
//#include <scheduler/process.h>

#define HAVE_GLP_SCH_MODULE
#define THRESHOLD_TIME_ECS 3000.0
#define THRESHOLD_ACCESS_ECS 20
#define DELTA_GROUP 1000000.0
#define DIM_STAT_GROUP 2

// This data structure defines the state of each group of LP
typedef struct _GLP_state{
	struct LP_state **local_LPS;	// Array that maintains all the LP managed by this group
	int tot_LP;				// Total number of LP managed by this group
	spinlock_t lock;		
} GLP_state;

// Define how many times a LP exhibits ECS toward another group 
typedef struct _ECS_stat{
	simtime_t last_access;	// Local virtual time at with the data-structure has been updated
	int count_access;		
} ECS_stat;

extern GLP_state **GLPS;

extern void rollback_group(simtime_t, unsigned int);

