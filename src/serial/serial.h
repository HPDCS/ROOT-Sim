/**
 * @file serial/serial.h
 *
 * @brief Serial scheduler
 *
 * This module implements the sequential execution of simulation models.
 * Here all the routines to support sequential simulations are implemented,
 * except for the event queue which uses the Calendar Queue implemented in
 * calqueue.c.
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
 */
#pragma once

#include <ROOT-Sim.h>

extern void SerialSetState(void *);
extern void SerialScheduleNewEvent(unsigned int, simtime_t, unsigned int,
				   void *, unsigned int);

extern void serial_init(void);
extern void serial_simulation(void) __attribute__((noreturn));
