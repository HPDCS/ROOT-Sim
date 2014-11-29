/**
*			Copyright (C) 2008-2014 HPDCS Group
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
* @file malloc.h
* @brief This is the ROOT-Sim implementation of the malloc library (to come...)
* @author Alessandro Pellegrini
*/


#pragma once
#ifndef _ROOTSIM_MALLOC_H
#define _ROOTSIM_MALLOC_H

#include <stddef.h>

extern inline void *rsalloc(size_t);
extern inline void rsfree(void *);
extern inline void *rsrealloc(void *, size_t);
extern inline void *rscalloc(size_t, size_t);

#endif /* _ROOTSIM_MALLOC_H */

