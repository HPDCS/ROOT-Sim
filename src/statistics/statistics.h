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
* @file statistics.h
* @brief State Management subsystem's header
* @author Francesco Quaglia
* @author Roberto Vitali
*/

#pragma once
#ifndef _STATISTICS_H
#define _STATISTICS_H

#define PER_KERNEL	0
#define PER_LP		1
#define UNIQUE		2
#define PREALLOC_FILE	3

#ifndef OUTPUT_FILE
#define OUTPUT_FILE	"kernel"
#endif

extern FILE 		*fout;		/* Output file, overwritten at each execution */
extern FILE 		*f_perf_stat;

struct stat_type {
	int kid;

	float 	Fr,
		Lr,
		Ef,
		tot_antimessages,
		tot_events,
		committed_events,
		committed_eventsRP,
		tot_rollbacks,
		tot_ckpts,
		ckpt_cost,
		recovery_cost,
		tot_checkpoints,
		tot_recoveries;
		
	double	event_time,
		ckpt_time;
};

extern FILE **files;
extern struct timeval simulation_timer;

extern void statistics_init(void);
extern void statistics_fini(void);
extern void _mkdir(const char *path);
extern void start_statistics(void);
extern int stop_statistics(void);
extern void flush_statistics(void);

#endif /* _STATISTICS_H */
