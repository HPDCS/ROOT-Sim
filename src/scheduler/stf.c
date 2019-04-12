/**
 * @file scheduler/stf.c
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

#include <arch/thread.h>
#include <core/core.h>
#include <queues/queues.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/mm.h>

/**
 *
 * @brief O(n) scheduler
 *
 * This function implements the smallest timestamp first algorithm.
 * This function is executed by every worker thread independently. Data
 * separation is ensured by relying on the temporary LP binding.
 *
 * @return a pointer to the @ref lp_struct of the LP to be activated.
 */
struct lp_struct *smallest_timestamp_first(void)
{
	struct lp_struct *next_lp = NULL;
	simtime_t evt_time, next_time = INFTY;

	foreach_bound_lp(lp) {
		// If waiting for synch, don't take into account the LP
		if (is_blocked_state(lp->state)) {
			continue;
		}
		// If the LP is in READY_FOR_SYNCH it has to handle the same ECS message
		if (lp->state == LP_STATE_READY_FOR_SYNCH) {
			// The LP handles the suspended event as the next event
			evt_time = lvt(lp);
		} else {
			// Compute the next event's timestamp.
			evt_time = next_event_timestamp(lp);
		}

		if (evt_time < next_time && evt_time < INFTY) {
			next_time = evt_time;
			next_lp = lp;
		}
	}

	return next_lp;
}
