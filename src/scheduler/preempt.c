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
* @file preempt.c
* @brief LP preemption management
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*/


/**
 * This function is activated when control is transferred back from
 * kernel space, when an APIC timer interrupt is received. When this is
 * the case, this function quickly checks whether some other LP has
 * gained an increased priority over the currently executing one, and
 * in the case changes the control flow so as to activate it.
 */
void preempt(void) {
}
