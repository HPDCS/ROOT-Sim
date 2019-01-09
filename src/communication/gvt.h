/**
* @file communication/gvt.h
*
* @brief Distributed GVT Support module
*
* This module implements communication primitives to implement the
* asynchronous distributed GVT reduction algorithm described in:
*
* T. Tocci, A. Pellegrini, F. Quaglia, J. Casanovas-García, and T. Suzumura,<br/>
* <em>“ORCHESTRA: An Asynchronous Wait-Free Distributed GVT Algorithm,”</em><br/>
* in Proceedings of the 21st IEEE/ACM International Symposium on Distributed
* Simulation and Real Time Applications<br/>
* 2017
*
* For a full understanding of the code, we encourage reading that paper.
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
* @author Tommaso Tocci
*/

#pragma once

#ifdef HAVE_MPI

#include <core/core.h>

/**
 * @brief Define colour values for messages and threads.
 *
 * Messages sent out from a kernel instance are either white or red,
 * depending on the colour of the thread which originated the send operation.
 *
 * Actually, since colour phases are repeating, we differentiate between
 * the first instance of a white phase, the first instance of red phase,
 * the next instance of a white phase, and the next instance of a red
 * phase. The third instance is mapped to the first, as it is ensured
 * that no overlaps in messages might occur.
 *
 * This enum defines the colours for messages, taking into account the
 * multiple phases which occur one after the other. Also threads colours
 * use the same definitions.
 *
 * The order of these values are important. White messages are mapped to
 * even numbers, while red messages are mapped to odd numbers. This is
 * used in macros such as is_red_colour() to quickly determine the colour
 * of a message.
 */
enum _msg_colours {
	white_0,	///< Color is white (phase 1)
	red_0,		///< Color is red (phase 1)
	white_1,	///< Color is white (phase 2)
	red_1		///< Color is red (phase 2)
};

/**
 * This macro tells whether a message is red. Message colours are defined
 * using the enum @ref _msg_colours.
 */
#define is_red_colour(c) ( (bool) (c & 0x1) )

/// Tells whether the current thread is in red phase
#define in_red_phase() ( is_red_colour(threads_phase_colour[local_tid]) )

/// Tells what is the next colour, using simple arithmetics
#define next_colour(c) ( ((c)+1) & 0x3 )

extern phase_colour *threads_phase_colour;

extern simtime_t *min_outgoing_red_msg;

void gvt_comm_init(void);
void gvt_comm_finalize(void);
void enter_red_phase(void);
void exit_red_phase(void);
void join_white_msg_redux(void);
bool white_msg_redux_completed(void);
void wait_white_msg_redux(void);
bool all_white_msg_received(void);
void flush_white_msg_recv(void);
void flush_white_msg_sent(void);
void broadcast_gvt_init(unsigned int round);
bool gvt_init_pending(void);
void gvt_init_clear(void);
void join_gvt_redux(simtime_t local_vt);
bool gvt_redux_completed(void);
simtime_t last_reduced_gvt(void);
void register_incoming_msg(const msg_t *);
void register_outgoing_msg(const msg_t *);

#endif /* HAVE_MPI */
