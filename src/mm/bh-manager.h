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
* @file bh-manager.h
* @brief 
* @author Francesco Quaglia
*/

#pragma once

#include <core/core.h>

#define MAX_MSG_SIZE sizeof(msg_t)
#define BH_PAGES	2000
#define BH_SIZE      	(BH_PAGES * PAGE_SIZE) //this is in bytes

void *get_buffer(int, int);
void switch_bh(int);

