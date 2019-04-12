/**
* @file gvt/gvt.h
*
* @brief Global Virtual Time
*
* This module implements the GVT reduction. The current implementation
* is non blocking for observable simulation plaftorms.
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
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date June 14, 2014
*/

#pragma once

#include <ROOT-Sim.h>
#include <mm/state.h>

/* API from gvt.c */
extern void gvt_init(void);
extern void gvt_fini(void);
extern simtime_t gvt_operations(void);
inline extern simtime_t get_last_gvt(void);

/* API from fossil.c */
extern void adopt_new_gvt(simtime_t);

/* API from ccgs.c */
extern void ccgs_init(void);
extern void ccgs_fini(void);
