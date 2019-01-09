/**
* @file datatypes/calqueue.h
*
* @brief Calendar Queue Implementation
*
* Classical Calendar Queue implementation. It is an array of lists of
* events which reorganizes itself upon each insertion/deletion, ensuring
* amortized O(1) operations.
*
* Due to the nature of the calendar queue, you cannot insert events which
* "happen before" the last extracted events, otherwise the list breaks
* and starts returning dummy events.
*
* For a thorough description of the algorithm, refer to:
*
* R. Brown
* “Calendar Queues: A Fast 0(1) Priority Queue Implementation for the
* Simulation Event Set Problem”
* CACM, Vol. 31, No. 10, pp. 1220-1227, Oct. 1988.
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
* @author R. Brown
*/

#pragma once

#define CALQSPACE 65536		// Calendar array size needed for maximum resize
#define MAXNBUCKETS 32768	// Maximum number of buckets in calendar queue

typedef struct __calqueue_node {
	double timestamp;	// Timestamp associated to the event
	void *payload;		// A pointer to the actual content of the node
	struct __calqueue_node *next;	// Pointers to other nodes
} calqueue_node;

typedef struct __calqueue_node *calendar_queue;

extern void calqueue_init(void);
extern void *calqueue_get(void);
extern void calqueue_put(double, void *);
