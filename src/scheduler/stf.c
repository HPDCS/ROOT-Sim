#include <arch/thread.h>
#include <core/core.h>
#include <queues/queues.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>

/**
* This function implements the smallest timestamp first algorithm
*
* @author Francesco Quaglia
*
* @return The Id of the Logical Process qith the smallest timestamp
*/
unsigned int smallest_timestamp_first(void) {

	simtime_t min_timestamp = -1, evt_time = -1;
	simtime_t min_bound = INFTY;
	unsigned int next = IDLE_PROCESS;
	register unsigned int i;

	// For each local process
	for (i = 0; i < n_prc_per_thread; i++) {

		min_bound = min(min_bound, (LPS_bound[i]->bound != NULL ? LPS_bound[i]->bound->timestamp : 0.0));

		// Blocked LPs cannot be scheduled
		if(is_blocked_state(LPS_bound[i]->state)) {
			continue;
		}

		// Compute the next event's timestamp. Translate the id from the local binding to the local ID
		evt_time = next_event_timestamp(LPS_bound[i]->lid);

		if(evt_time > -1) {
			if((D_EQUAL(min_timestamp, -1)) || ((min_timestamp > -1) && (evt_time < min_timestamp))) {
				min_timestamp = evt_time;
				next = LPS_bound[i]->lid;
			}
		}
	}

	// Return the process to execute
	if(D_EQUAL(min_timestamp, -1)) {
		return IDLE_PROCESS;
	} else {
		return next;
	}

}

