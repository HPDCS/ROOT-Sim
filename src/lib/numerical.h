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
* @file numerical.h
* @brief This header is used to define symbols which must be accessible by the simulator
* 	but not by the application-level code. Any symbol needed by the application-
* 	level code (i.e. random distributions) is found in ROOT-Sim.h
*        numerical distribution implementations
* @author Alessandro Pellegrini
* @date Dec 10, 2013
*/

#pragma once
#ifndef __NUMERICAL_H
#define __NUMERICAL_H

/// Numerical seed type
typedef uint64_t seed_type;

void numerical_init(void);

#endif /* #ifndef __NUMERICAL_H */

