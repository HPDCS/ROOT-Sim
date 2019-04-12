/**
 * @file arch/memusage.h
 *
 * @brief Memory usage module header
 *
 * This module provides system-dependend functions to retrieve the
 * current/peak memory usage. This is used for statistics. The original
 * implementation is from David Robert Nadeau, which released its code
 * under the Creative Commons Attribution 3.0 Unported License
 * ( http://creativecommons.org/licenses/by/3.0/deed.en_US )
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
 * @author David Robert Nadeau - http://NadeauSoftware.com/
 */

#pragma once

#include <string.h>
extern size_t getCurrentRSS(void);
extern size_t getPeakRSS(void);
