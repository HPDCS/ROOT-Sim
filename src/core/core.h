/**
* @file core/core.h
*
* @brief Core ROOT-Sim functionalities
*
* Core ROOT-Sim functionalities
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
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @date 3/18/2011
*/

#pragma once

#include <ROOT-Sim.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <setjmp.h>

#include <arch/thread.h>


/// This macro expands to true if the local kernel is the master kernel
#define master_kernel() (kid == 0)

// XXX: This should be moved to state or queues
enum {
	SNAPSHOT_INVALID = 0,	/**< By convention 0 is the invalid field */
	SNAPSHOT_FULL,		/**< xxx documentation */
};

/// Maximum number of kernels the distributed simulator can handle
#define N_KER_MAX	128

/// Maximum number of LPs the simulator will handle
#define MAX_LPs		250000

// XXX: this should be moved somewhere else...
enum {
	VERBOSE_INVALID = 0,	/**< By convention 0 is the invalid field */
	VERBOSE_INFO,		/**< xxx documentation */
	VERBOSE_DEBUG,		/**< xxx documentation */
	VERBOSE_NO		/**< xxx documentation */
};

extern jmp_buf exit_jmp;

/// Optimize the branch as likely taken
#define likely(exp) __builtin_expect(exp, 1)
/// Optimize the branch as likely not taken
#define unlikely(exp) __builtin_expect(exp, 0)


enum {
	LP_DISTRIBUTION_INVALID = 0,	/**< By convention 0 is the invalid field */
	LP_DISTRIBUTION_BLOCK,		/**< Distribute exceeding LPs according to a block policy */
	LP_DISTRIBUTION_CIRCULAR	/**< Distribute exceeding LPs according to a circular policy */
};

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

/**
 * @brief Definition of a GID.
 *
 * This structure defines a GID. The purpose of this structure is to make
 * functions dealing with GIDs and LIDs type safe, to avoid runtime problems
 * if the two are mixed when calling a function.
 */
typedef struct _gid_t {
	unsigned int to_int;	///< The GID numerical value
} GID_t;


/**
 * @brief Definition of a LID.
 *
 * This structure defines a LID. The purpose of this structure is to make
 * functions dealing with GIDs and LIDs type safe, to avoid runtime problems
 * if the two are mixed when calling a function.
 */
typedef struct _lid_t {
	unsigned int to_int;	///< The LID numerical value
} LID_t;

#define is_lid(val) __builtin_types_compatible_p(__typeof__ (val), LID_t)
#define is_gid(val) __builtin_types_compatible_p(__typeof__ (val), GID_t)

#define set_lid(lid, value) (__builtin_choose_expr(is_lid(lid), lid.to_int, (void)0) = (value))
#define set_gid(gid, value) (__builtin_choose_expr(is_gid(gid), gid.to_int, (void)0) = (value))

typedef enum { positive, negative, control } message_kind_t;

#ifdef HAVE_MPI
typedef unsigned char phase_colour;
#endif

#define MSG_PADDING offsetof(msg_t, sender)
#define MSG_META_SIZE (offsetof(msg_t, event_content) - MSG_PADDING)

/// Message Type definition
typedef struct _msg_t {

	/* Place here all memebers of the struct which should not be transmitted over the network */

	// Pointers to attach messages to chains
	struct _msg_t *next;
	struct _msg_t *prev;

	/* Place here all members which must be transmitted over the network. It is convenient not to reorder the members
	 * of the structure. If new members have to be addedd, place them right before the "Model data" part.*/

	// Kernel's information
	GID_t sender;
	GID_t receiver;
#ifdef HAVE_MPI
	phase_colour colour;
#endif
	int type;
	message_kind_t message_kind;
	simtime_t timestamp;
	simtime_t send_time;
	unsigned long long mark;	/// Unique identifier of the message, used for antimessages
	unsigned long long rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous events

	// Model data
	unsigned int size;
	unsigned char event_content[];
} msg_t;

/// Message envelope definition. This is used to handle the output queue and stores information needed to generate antimessages
typedef struct _msg_hdr_t {
	// Pointers to attach messages to chains
	struct _msg_hdr_t *next;
	struct _msg_hdr_t *prev;
	// Kernel's information
	GID_t sender;
	GID_t receiver;
	// TODO: non serve davvero, togliere
	int type;
	unsigned long long rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous event
	// TODO: fine togliere
	simtime_t timestamp;
	simtime_t send_time;
	unsigned long long mark;
} msg_hdr_t;


/// Barrier for all worker threads
extern barrier_t all_thread_barrier;

// XXX: this should be refactored someway
extern unsigned int kid,	/* Kernel ID for the local kernel */
 n_ker,				/* Total number of kernel instances */
 n_cores,			/* Total number of cores required for simulation */
 n_prc,				/* Number of LPs hosted by the current kernel instance */
*kernel;

extern void ProcessEvent_light(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);
bool OnGVT_light(unsigned int me, void *snapshot);
extern void ProcessEvent_inc(unsigned int me, simtime_t now, int event_type, void *event_content, unsigned int size, void *state);
bool OnGVT_inc(unsigned int me, void *snapshot);

extern void base_init(void);
extern void base_fini(void);
extern unsigned int find_kernel_by_gid(GID_t gid) __attribute__((pure));
extern void _rootsim_error(bool fatal, const char *msg, ...);
extern void distribute_lps_on_kernels(void);
extern void simulation_shutdown(int code) __attribute__((noreturn));
extern inline bool user_requested_exit(void);
extern inline bool simulation_error(void);
extern void initialization_complete(void);

#define rootsim_error(fatal, msg, ...) _rootsim_error(fatal, "%s:%d: %s(): " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
