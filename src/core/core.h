/**
*			Copyright (C) 2008-2017 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
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
* @file core.h
* @brief This header defines all the shared symbols which are needed by different subsystems
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
*/


#pragma once
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <ROOT-Sim.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

#include <lib/numerical.h>
#include <arch/thread.h>
#include <statistics/statistics.h>

/// If set, ROOT-Sim will produce statistics on the execution
#define PLATFORM_STATS

/// This macro expands to true if the local kernel is the master kernel
#define master_kernel() (kid == 0)


// XXX: This should be moved to state or queues
#define INVALID_SNAPSHOT	2000
#define FULL_SNAPSHOT		2001

/// This defines an idle process (i.e., the fake process to be executed when no events are available)
#define IDLE_PROCESS	UINT_MAX

/// Maximum number of kernels the distributed simulator can handle
#define N_KER_MAX	128

/// Maximum number of LPs the simulator will handle
#define MAX_LPs		16384		// This is 2^14

// XXX: this should be moved somewhere else...
#define VERBOSE_INFO	1700
#define VERBOSE_DEBUG	1701
#define VERBOSE_NO	1702


// XXX Do we still use transient time?
/// Transient duration (in msec)
#define STARTUP_TIME	0

/// Distribute exceeding LPs according to a block policy
#define LP_DISTRIBUTION_BLOCK 0
/// Distribute exceeding LPs according to a circular policy
#define LP_DISTRIBUTION_CIRCULAR 1


// XXX should be moved to a more librarish header
/// Equality condition for floats
#define F_EQUAL(a,b) (fabsf((a) - (b)) < FLT_EPSILON)
/// Equality to zero condition for floats
#define F_EQUAL_ZERO(a) (fabsf(a) < FLT_EPSILON)
/// Difference condition for floats
#define F_DIFFER(a,b) (fabsf((a) - (b)) >= FLT_EPSILON)
/// Difference from zero condition for floats
#define F_DIFFER_ZERO(a) (fabsf(a) >= FLT_EPSILON)


/// Equality condition for doubles
#define D_EQUAL(a,b) (fabs((a) - (b)) < DBL_EPSILON)
/// Equality to zero condition for doubles
#define D_EQUAL_ZERO(a) (fabs(a) < DBL_EPSILON)
/// Difference condition for doubles
#define D_DIFFER(a,b) (fabs((a) - (b)) >= DBL_EPSILON)
/// Difference from zero condition for doubles
#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)


/// Macro to find the maximum among two values
#ifdef max
#undef max
#endif
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

/// Macro to find the minimum among two values
#ifdef min
#undef min
#endif
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


/// Macro to "legitimately" pun a type
#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

typedef enum {positive, negative, control} message_kind_t;

#ifdef HAS_MPI
typedef unsigned char phase_colour;
#endif

/* The MPI datatype msg_mpi_t depends on the order of this struct. */

/// Message Type definition
typedef struct _msg_t {
	// Kernel's information
	unsigned int   		sender;
	unsigned int   		receiver;
	#ifdef HAS_MPI
	phase_colour		colour;
	#endif
	int   			type;
	message_kind_t		message_kind;
	simtime_t		timestamp;
	simtime_t		send_time;
	unsigned long long	mark;	/// Unique identifier of the message, used for antimessages
	unsigned long long	rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous events
	int			alloc_tid; // TODO: this should be moved into an external container, to avoid transmitting it!
	// Model data
	int size;
	unsigned char event_content[];
} msg_t;


/// Message envelope definition. This is used to handle the output queue and stores information needed to generate antimessages
typedef struct _msg_hdr_t {
	// Kernel's information
	unsigned int   		sender;
	unsigned int   		receiver;
	// TODO: non serve davvero, togliere
	int   			type;
	unsigned long long	rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous event
	// TODO: fine togliere
	simtime_t		timestamp;
	simtime_t		send_time;
	unsigned long long	mark;
} msg_hdr_t;



/// Configuration of the execution of the simulator
typedef struct _simulation_configuration {
	char *output_dir;		/// Destination Directory of output files
	int backtrace;			/// Debug mode flag
	int scheduler;			/// Which scheduler to be used
	int gvt_time_period;		/// Wall-Clock time to wait before executiong GVT operations
	int gvt_snapshot_cycles;	/// GVT operations to be executed before rebuilding the state
	int simulation_time;		/// Wall-clock-time based termination predicate
	int lps_distribution;		/// Policy for the LP to Kernel mapping
	int ckpt_mode;			/// Type of checkpointing mode (Synchronous, Semi-Asyncronous, ...)
	int checkpointing;		/// Type of checkpointing scheme (e.g., PSS, CSS, ...)
	int ckpt_period;		/// Number of events to execute before taking a snapshot in PSS (ignored otherwise)
	int snapshot;			/// Type of snapshot (e.g., full, incremental, autonomic, ...)
	int check_termination_mode;	/// Check termination strategy: standard or incremental
	bool blocking_gvt;		/// GVT protocol blocking or not
	bool deterministic_seed;	/// Does not change the seed value config file that will be read during the next runs
	int verbose;			/// Kernel verbose
	enum stat_levels stats;		/// Produce performance statistic file (default STATS_ALL)
	bool serial;			// If the simulation must be run serially
	seed_type set_seed;		/// The master seed to be used in this run
	bool core_binding;		/// Bind threads to specific core ( reduce context switches and cache misses )

#ifdef HAVE_PREEMPTION
	bool disable_preemption;	/// If compiled for preemptive Time Warp, it can be disabled at runtime
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
	bool disable_allocator;
#endif
} simulation_configuration;


/// Barrier for all worker threads
extern barrier_t all_thread_barrier;

// XXX: this should be refactored someway
extern unsigned int	kid,		/* Kernel ID for the local kernel */
			n_ker,		/* Total number of kernel instances */
			n_cores,	/* Total number of cores required for simulation */
			n_prc,		/* Number of LPs hosted by the current kernel instance */
			*kernel;


extern simulation_configuration rootsim_config;

extern void ProcessEvent_light(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);
bool OnGVT_light(int gid, void *snapshot);
extern void ProcessEvent_inc(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);
bool OnGVT_inc(int gid, void *snapshot);
extern bool (**OnGVT)(int gid, void *snapshot);
extern void (**ProcessEvent)(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);

extern void base_init(void);
extern void base_fini(void);
extern unsigned int LidToGid(unsigned int lid);
extern unsigned int GidToLid(unsigned int gid);
extern unsigned int GidToKernel(unsigned int gid);
extern void rootsim_error(bool fatal, const char *msg, ...);
extern void distribute_lps_on_kernels(void);
extern void simulation_shutdown(int code) __attribute__((noreturn));
extern inline bool simulation_error(void);
extern void initialization_complete(void);

#endif

