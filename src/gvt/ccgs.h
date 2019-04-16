/**
* @file gvt/ccgs.h
*
* @brief Consistent and Committed Global State
*
* Consistent and Committed Global State (CCGS) is a subsystem that (poeriodically)
* recomputes a global state on which the LPs can inspect the simulation trajectory
* and determine whether the simulation can stop, by relying on the OnGVT() callback.
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
* @author Paolo Romano
* @author Alessandro Pellegrini
* @author Diego Cucuzzo
* @author Stefano D’Alessio
*
* @date 2007
*/

#pragma once

enum {
	CKTRM_INVALID = 0,	/**< By convention 0 is the invalid field */
	CKTRM_NORMAL,		/**< Normal CheckTermination */
	CKTRM_INCREMENTAL,	/**< Incremental CheckTermination */
	CKTRM_ACCURATE		/**< Accurate CheckTermination */
};

#include <mm/state.h>

extern inline bool ccgs_can_halt_simulation(void);
extern void ccgs_reduce_termination(void);
extern void ccgs_compute_snapshot(state_t * time_barrier_pointer[], simtime_t gvt);
