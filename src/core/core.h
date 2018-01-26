/**
*			Copyright (C) 2008-2018 HPDCS Group
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

/// If set, ROOT-Sim will produce statistics on the execution
#define PLATFORM_STATS

/// This macro expands to true if the local kernel is the master kernel
#define master_kernel() (kid == 0)

// XXX: This should be moved to state or queues
#define INVALID_SNAPSHOT	2000
#define FULL_SNAPSHOT		2001

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
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

/// Macro to find the minimum among two values
#ifdef min
#undef min
#endif
#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


/// Macro to "legitimately" pun a type
#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

// GID and LID types
typedef struct _gid_t {unsigned int id;} GID_t;
typedef struct _lid_t {unsigned int id;} LID_t;

// The idle process identifier
extern LID_t idle_process;

#define is_lid(val) __builtin_types_compatible_p(__typeof__ (val), LID_t)
#define is_gid(val) __builtin_types_compatible_p(__typeof__ (val), GID_t)

#define is_valid_lid(lid) ((lid).id < n_prc)
#define is_valid_gid(gid) ((gid).id < n_prc_tot)

#define lid_equals(first, second) (is_lid(first) && is_lid(second) && first.id == second.id)
#define gid_equals(first, second) (is_gid(first) && is_gid(second) && first.id == second.id)

#define lid_to_int(lid) __builtin_choose_expr(is_lid(lid), (lid).id, (void)0)
#define gid_to_int(gid) __builtin_choose_expr(is_gid(gid), (gid).id, (void)0)

#define set_lid(lid, value) (__builtin_choose_expr(is_lid(lid), lid.id, (void)0) = (value))
#define set_gid(gid, value) (__builtin_choose_expr(is_gid(gid), gid.id, (void)0) = (value))

typedef enum {positive, negative, control} message_kind_t;

#ifdef HAVE_MPI
typedef unsigned char phase_colour;
#endif

/** The MPI datatype msg_mpi_t depends on the order of this struct.
   See src/communication/mpi.c for the implementation of the datatype */
/// Message Type definition
typedef struct _msg_t {

	/* Place here all memebers of the struct which should not be transmitted over the network */

	// Pointers to attach messages to chains
	struct _msg_t 		*next;
	struct _msg_t 		*prev;
	int			alloc_tid; // TODO: this should be moved into an external container, to avoid transmitting it!

	/* Place here all members which must be transmitted over the network. It is convenient not to reorder the members
	 * of the structure. If new members have to be addedd, place them right before the "Model data" part.
	 * The code in `mpi_datatype_init()` in communication/mpi.c must be aligned to the content that we have here. */

	// Kernel's information
	GID_t   		sender;
	GID_t   		receiver;
	#ifdef HAVE_MPI
	phase_colour		colour;
	#endif
	int   			type;
	message_kind_t		message_kind;
	simtime_t		timestamp;
	simtime_t		send_time;
	unsigned long long	mark;	/// Unique identifier of the message, used for antimessages
	unsigned long long	rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous events

	// Model data
	int size;
	unsigned char event_content[];
} msg_t;


/// Message envelope definition. This is used to handle the output queue and stores information needed to generate antimessages
typedef struct _msg_hdr_t {
	// Pointers to attach messages to chains
	struct _msg_hdr_t 		*next;
	struct _msg_hdr_t 		*prev;
	// Kernel's information
	GID_t   		sender;
	GID_t   		receiver;
	// TODO: non serve davvero, togliere
	int   			type;
	unsigned long long	rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous event
	int			alloc_tid;
	// TODO: fine togliere
	simtime_t		timestamp;
	simtime_t		send_time;
	unsigned long long	mark;
} msg_hdr_t;




/// Barrier for all worker threads
extern barrier_t all_thread_barrier;

// XXX: this should be refactored someway
extern unsigned int	kid,		/* Kernel ID for the local kernel */
			n_ker,		/* Total number of kernel instances */
			n_cores,	/* Total number of cores required for simulation */
			n_prc,		/* Number of LPs hosted by the current kernel instance */
			*kernel;



extern void ProcessEvent_light(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);
bool OnGVT_light(unsigned int me, void *snapshot);
extern void ProcessEvent_inc(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);
bool OnGVT_inc(unsigned int me, void *snapshot);
extern bool (**OnGVT)(unsigned int me, void *snapshot);
extern void (**ProcessEvent)(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);

extern void base_init(void);
extern void base_fini(void);
extern GID_t LidToGid(LID_t lid);
extern LID_t GidToLid(GID_t gid);
extern unsigned int GidToKernel(GID_t gid);
extern void rootsim_error(bool fatal, const char *msg, ...);
extern void distribute_lps_on_kernels(void);
extern void simulation_shutdown(int code) __attribute__((noreturn));
extern inline bool simulation_error(void);
extern void initialization_complete(void);

#endif

