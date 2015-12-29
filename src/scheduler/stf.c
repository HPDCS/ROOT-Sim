#include <arch/thread.h>
#include <core/core.h>
#include <queues/queues.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <mm/dymelor.h>

//Check if the next event of at least one LP of group is bigger or equal of group's gvt, if so update the group_state according to
//the state of LP whit minimum timestamp that belongs to the group
/*
void update_info_group(){
	GLP_state *current_group;
	unsigned int i,j,min_lid;
	simtime_t min_time;

	for (i = 0; i < n_prc_per_thread; i++) { 
	
		current_group = GLPS[LPS_bound[i]->current_group];
		if(current_group->group_is_ready)
			continue;
	
		//If the LP is in READY_FOR_SYNCH has to handle the same messagge of ECS
                if(LPS_bound[i]->state == LP_STATE_READY_FOR_SYNCH || is_blocked_state(LPS_bound[i]->state)) {
                        // The LP handles the suspended event as the next event
                        evt_time = LPS_bound[i]->bound->timestamp;
                }
                else {
                        // Compute the next event's timestamp. Translate the id from the local binding to the local ID
                        evt_time = next_event_timestamp(LPS_bound[i]->lid);
		}
		
		if(evt_time->timestamp >= current_group->lvt && verify_time_group(evt_time->timestamp)){
			current_group->group_is_ready = true;
						
			min_lid = i;
        		min_time = evt_time->timestamp;

			LP_state **local_LPS = current_group->local_LPS;
			for(j=0; j<n_prc; j++){
				if(local_LPS[j] != NULL){
					if(local_LPS[j]->state == LP_STATE_READY_FOR_SYNCH || is_blocked_state(local_LPS[j]->state)) {
						// The LP handles the suspended event as the next event
						evt_temp = local_LPS[j]->bound->timestamp;
					}
					else {
						// Compute the next event's timestamp. Translate the id from the local binding to the local ID
						evt_temp = next_event_timestamp(local_LPS[j]->lid);
					}
					
					if(evt_temp->timestamp < min_time){
						min_lid = j;
						min_time = evt_time->timestamp;	
					}
				}		
			}	
			
			if(is_blocked_state(LPS[min_lid]->state)){
				switch(LPS[min_lid]->state){
					case LP_STATE_WAIT_FOR_SYNCH:
						current_group->state = GLP_STATE_WAIT_FOR_SYNCH;
						break;
					case LP_STATE_WAIT_FOR_UNBLOCK:
						current_group->state = GLP_STATE_WAIT_FOR_UNBLOCK;
                                        	break;
                        	}
			}					
		}
		
	}
}
*/

bool check_state_group(unsigned int lid){

	LP_state *temp_LP = LPS_bound[lid];
	GLP_state *current_group = GLPS[temp_LP->current_group];


	if(is_blocked_state(temp_LP->state) || (is_blocked_state(current_group->state) && check_start_group(lid) && verify_time_group(lvt(lid)))){ 
      		return true;
	}
	
	if(temp_LP->state == LP_STATE_SILENT_EXEC && current_group->state == GLP_STATE_ROLLBACK)
		return true;


	return false;

}

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
		return next;
	}
}

