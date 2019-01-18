/**
 * @file scheduler/stf.h
 *
 * @brief O(n) scheduling algorithm
 *
 * This module implements the O(n) scheduler based on the Lowest-Timestamp
 * First policy.
 *
 * Each worker thread has its own pool of LPs to check, thanks to the
 * temporary binding which is computed in binding.c
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
 */

#pragma once

#include <core/core.h>
#include <scheduler/process.h>

extern struct lp_struct *smallest_timestamp_first(void);
