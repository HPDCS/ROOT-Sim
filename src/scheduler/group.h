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

#pragma once

#include <arch/atomic.h>

#define HAVE_GLP_SCH_MODULE
#define THRESHOLD_TIME_ECS 3000.0
#define THRESHOLD_ACCESS_ECS 20
#define DELTA_GROUP 1000000.0
#define DIM_STAT_GROUP 2
#define CKPT_PERIOD_GROUP 10

#define GLP_STATE_READY			0x02001
#define GLP_STATE_ROLLBACK		0x02004
#define GLP_STATE_SILENT_EXEC		0x02008
#define GLP_STATE_READY_FOR_SYNCH	0x02011	
#define GLP_STATE_WAIT_FOR_SYNCH	0x03001
#define GLP_STATE_WAIT_FOR_UNBLOCK	0x03002
#define GLP_STATE_WAIT_FOR_GROUP	0x03003
#define GLP_STATE_WAIT_FOR_LOG		0x02012

// This data structure defines the state of each group of LP
typedef struct _GLP_state{
	struct LP_state **local_LPS;	// Array that maintains all the LP managed by this group
	unsigned int tot_LP;		// Total number of LP managed by this group
	spinlock_t lock;
	msg_t *initial_group_time;	// Time from with the group is ready to start
	short unsigned int state;	// State of entire group
	msg_t *lvt;			// Local Virtual Time of group, last message time correctly executed
	unsigned int counter_rollback;	// Counter to verify the condition to exit from GLP_STATE_ROLLBACK
	unsigned int counter_silent_ex;	// Counter to verify the condition to exit from GLP_STATE_SILENT_EXEC
	unsigned int from_last_ckpt;	// Counts how many events executed from the last checkpoint (to support PSS)
	unsigned int ckpt_period;	// This variable mainains the current checkpointing interval for the LP
	bool group_is_ready;		// Bool to check if group is at the correct time to execute
	unsigned int counter_synch;	// Counter to check if all LPs reach the correct time
	unsigned int counter_log;	// Counter to check if all LPs done the log
} GLP_state;

// Define how many times a LP exhibits ECS toward another group 
typedef struct _ECS_stat{
	simtime_t last_access;	// Local virtual time at with the data-structure has been updated
	unsigned int count_access;		
} ECS_stat;

extern GLP_state **GLPS;

extern void rollback_group(msg_t*, unsigned int);
extern bool check_start_group(unsigned int);
extern void force_checkpoint_group(unsigned int);
