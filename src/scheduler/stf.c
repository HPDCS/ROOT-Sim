#include <arch/thread.h>
#include <core/core.h>
#include <queues/queues.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/mm.h>


/**
* This function implements the smallest timestamp first algorithm.
* Bound LPs are looped through their foreach iteratora.
*
* @author Francesco Quaglia
*
* @return The Id of the Logical Process qith the smallest timestamp
*/
struct lp_struct *smallest_timestamp_first(void) {
	struct lp_struct *next_lp = NULL;
	simtime_t evt_time, next_time = INFTY;

	foreach_bound_lp(lp) {
		// If waiting for synch, don't take into account the LP
		if(is_blocked_state(lp->state)) {
			continue;
		}
	
		// If the LP is in READY_FOR_SYNCH it has to handle the same ECS message
		if(lp->state == LP_STATE_READY_FOR_SYNCH) {
			// The LP handles the suspended event as the next event
			evt_time = lvt(lp);
		} else {
			// Compute the next event's timestamp.
			evt_time = next_event_timestamp(lp);
		}
	
		if(evt_time < next_time && evt_time < INFTY) {
			next_time = evt_time;
			next_lp = lp;
		}
	}

	return next_lp;
}

