#include <arch/thread.h>
#include <core/core.h>
#include <queues/queues.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/dymelor.h>

typedef struct _min_lp {
	LID_t lid;
	simtime_t time;
} min_lp;


/**
* This function implements the smallest timestamp first algorithm.
* Bound LPs are looped through their foreach iteratora.
*
* @author Francesco Quaglia
*
* @return The Id of the Logical Process qith the smallest timestamp
*/
LID_t smallest_timestamp_first(void) {
	min_lp stf_candidate = {
		.lid = idle_process,
		.time = INFTY,
	};

	int __helper(LID_t lid, GID_t gid, unsigned int num, void *data) {
		min_lp *candidate = (min_lp *)data;
		LP_State *lp = LPS(lid);
		simtime_t evt_time;
		(void)gid;
		(void)num;
	
		// If waiting for synch, don't take into account the LP
		if(is_blocked_state(lp->state)) {
			return 0; // continue to the next element
		}
	
		//If the LP is in READY_FOR_SYNCH has to handle the same messagge of ECS
		if(lp->state == LP_STATE_READY_FOR_SYNCH) {
			// The LP handles the suspended event as the next event
			evt_time = lvt(lid);
		} else {
			// Compute the next event's timestamp. Translate the id from the local binding to the local ID
			evt_time = next_event_timestamp(lid);
		}
	
		if(evt_time < candidate->time && evt_time < INFTY) {
			candidate->time = evt_time;
			candidate->lid = lid;
		}

		return 0; // continue to the next element
	}

	LPS_bound_foreach(__helper, &stf_candidate);

	return stf_candidate.lid;
}


/* This function implements a selection strategy similar
* to smallest timestamp first algorithm for the asymmetric 
* architecture. It returns the LP of the logical process with 
* the smallest timestamp among the set of LPs passed as parameter
* This is necessary to skip LPs for whom the respective input port
* is already filled. 
*
* TO BE IMPLEMENTED.
* 
* @author Stefano Conoci 
* @author Alessandro Pellegrini */

LID_t asym_smallest_timestamp_first(void){
	min_lp stf_candidate = {
		.lid = idle_process,
		.time = INFTY,
	};

	int __helper(LID_t lid, GID_t gid, unsigned int num, void *data) {
		min_lp *candidate = (min_lp *)data;
		LP_State *lp = LPS(lid);
		simtime_t evt_time;
		(void)gid;
		(void)num;
	
		// If waiting for synch, don't take into account the LP
		if(is_blocked_state(lp->state)) {
			return 0; // continue to the next element
		}
	
		//If the LP is in READY_FOR_SYNCH has to handle the same messagge of ECS
		if(lp->state == LP_STATE_READY_FOR_SYNCH) {
			// The LP handles the suspended event as the next event
			evt_time = lvt(lid);
		} else {
			// Compute the next event's timestamp. Translate the id from the local binding to the local ID
			evt_time = next_event_timestamp(lid);
		}
	
		if(evt_time < candidate->time && evt_time < INFTY) {
			candidate->time = evt_time;
			candidate->lid = lid;
		}

		return 0; // continue to the next element
	}

	LPS_asym_mask_foreach(__helper, &stf_candidate);

	return stf_candidate.lid;
}

