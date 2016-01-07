#include <arch/thread.h>
#include <core/core.h>
#include <queues/queues.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/dymelor.h>

#ifdef HAVE_GLP_SCH_MODULE
bool check_state_group(unsigned int lid){

	LP_state *temp_LP = LPS_bound[lid];
	GLP_state *current_group = GLPS[temp_LP->current_group];


	if(is_blocked_state(temp_LP->state) || (is_blocked_state(current_group->state) &&  min_timestamp > current_group->initial_group_time->timestamp  && verify_time_group(lvt(lid)))){ 
      		return true;
	}
	
	if(temp_LP->state == LP_STATE_SILENT_EXEC && current_group->state == GLP_STATE_ROLLBACK)
		return true;


	return false;

}
#endif

/**
* This function implements the smallest timestamp first algorithm
*
* @author Francesco Quaglia
*
* @return The Id of the Logical Process qith the smallest timestamp
*/
unsigned int smallest_timestamp_first(void) {

	simtime_t min_timestamp = -1, evt_time = -1;
	unsigned int next = IDLE_PROCESS;
	register unsigned int i;

	// For each local process
	for (i = 0; i < n_prc_per_thread; i++) {
		#ifdef HAVE_GLP_SCH_MODULE

		if(LPS_bound[i]->state == LP_STATE_WAIT_FOR_GROUP){
			if(GLPS[LPS_bound[i]->current_group]->state == GLP_STATE_WAIT_FOR_GROUP)
				continue;
		}
		else if(LPS_bound[i]->state == LP_STATE_WAIT_FOR_LOG){
			if(GLPS[LPS_bound[i]->current_group]->state == GLP_STATE_WAIT_FOR_LOG)
				continue;
			else
				//For avoiding deadlock in case of double log instance
				LPS_bound[i]->state = LP_STATE_READY;
		}
		else if(check_state_group(i))
			continue;
		#else
		// If waiting for synch, don't take into account the LP
		if(is_blocked_state(LPS_bound[i]->state)) {
			continue;
		}
		#endif
		//If the LP is in READY_FOR_SYNCH has to handle the same messagge of ECS
		if(LPS_bound[i]->state == LP_STATE_READY_FOR_SYNCH) {
			// The LP handles the suspended event as the next event
			evt_time = LPS_bound[i]->bound->timestamp;
		}
		#ifdef HAVE_GLP_SCH_MODULE
		//Due to rollback group in case of LP whitout next event that has to update the state of group
		else if(LPS_bound[i]->state == LP_STATE_ROLLBACK && next_event_timestamp(LPS_bound[i]->lid) <= -1){
			evt_time = LPS_bound[i]->bound->timestamp;
		}
		else if(LPS_bound[i]->state == LP_STATE_SILENT_EXEC && GLPS[LPS_bound[i]->current_group]->state == GLP_STATE_SILENT_EXEC  && (next_event_timestamp(LPS_bound[i]->lid) <= -1 || next_event_timestamp(LPS_bound[i]->lid) > GLPS[LPS_bound[i]->current_group]->lvt->timestamp) ){
                        evt_time = LPS_bound[i]->bound->timestamp;
                }
	 	#endif
		else {
			// Compute the next event's timestamp. Translate the id from the local binding to the local ID
			evt_time = next_event_timestamp(LPS_bound[i]->lid);
		}

		if(evt_time > -1) {
			if((D_EQUAL(min_timestamp, -1)) || ((min_timestamp > -1) && (evt_time < min_timestamp))) {
				min_timestamp = evt_time;
				next = LPS_bound[i]->lid;
			}
		}
	}
	if(D_EQUAL(min_timestamp, -1)) {
		return IDLE_PROCESS;
	} else {
		if(!is_blocked_state(LPS[next]->state) && 
			is_blocked_state(GLPS[LPS[next]->current_group]->state) && 
			min_timestamp > GLPS[LPS[next]->current_group]->initial_group_time->timestamp &&
			verify_time_group(lvt(next)))		
			printf("############ ERRORE STF ############\n");
		return next;
	}
}

