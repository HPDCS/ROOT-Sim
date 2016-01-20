#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <statistics/statistics.h>

#ifdef HAVE_GLP_SCH_MODULE
void insert_lp_group(GLP_state *current_group, unsigned int lid){
	current_group->local_LPS[current_group->tot_LP] = LPS[lid];
        current_group->tot_LP++;
}

void remove_lp_group(GLP_state *old_group, unsigned int lid){
	unsigned int i;
	LP_state **list;
	for(i=0; i< old_group->tot_LP; i++){
		list = old_group->local_LPS;
		if(list[i]->lid == lid){
			if(old_group->tot_LP == 1 || (old_group->tot_LP-1) == i){
				old_group->tot_LP--;
			}
			else{
				list[i] = list[old_group->tot_LP-1];
				old_group->tot_LP--;
			}
	
			break;
		}
	}
}


void update_IGT(msg_t *IGT, msg_t *new_IGT){
	memcpy(IGT,new_IGT,sizeof(msg_t));	
}

bool check_IGT(msg_t *IGT, msg_t *msg){
	if(D_EQUAL(IGT->timestamp,msg->timestamp) && IGT->mark == msg->mark)
		return true;
	
	return false;
}

void reset_IGT(msg_t *IGT){
	IGT->timestamp = -1.0;
	IGT->mark = 0;
}

void reset_flag_counter_synch(unsigned int group){
	unsigned int i;
	GLP_state *current_group = GLPS[group];
	LP_state **list = current_group->local_LPS;	
        for(i=0;i<current_group->tot_LP;i++){
		list[i]->updated_counter = false;
	}
	
	
}

void reset_synch_counter(unsigned int lid){
	LP_state *temp_LP = LPS[lid];
	GLP_state *current_group = GLPS[LPS[lid]->current_group];
 
	if(temp_LP->bound->type == SYNCH_GROUP || 
		/*(temp_LP->bound->mark == current_group->initial_group_time->mark && 
			D_EQUAL(temp_LP->bound->timestamp,current_group->initial_group_time->timestamp)) &&*/
	  	temp_LP->updated_counter
	    ){
		PRINT_DEBUG_GLP{
			printf("DEC COUNTER LP:%d GLP:%d Type:%d counter:%d \n",lid,LPS[lid]->current_group,temp_LP->bound->type,current_group->counter_synch);
		}
		current_group->counter_synch--;	
		temp_LP->updated_counter = false;
	}
}

void check_rollback_group(msg_t *straggler, unsigned int lid, simtime_t lvt_receiver, int msg_case){
	
	switch(msg_case){
		case positive:
			if(check_start_group(lid) &&  verify_time_group(LPS[lid]->bound->timestamp)){
				if(straggler->timestamp < lvt_receiver){
					PRINT_DEBUG_GLP{
	                                        printf("RGB [POSITIVE Type:%d] T:%f S:%d R:%d\n",
        	                                	straggler->type,LPS[lid]->bound->timestamp,
                	                        	straggler->sender, lid);
					}

                                	rollback_group(straggler,lid);
                         	}
				else if(straggler->timestamp < GLPS[LPS[lid]->current_group]->lvt->timestamp){
					PRINT_DEBUG_GLP{
						printf("RGB ltv_group [POSITIVE Type:%d] T:%f R:%d S:%d GRP[%d]->lvt:%f msg->Time:%f\n",
							straggler->type,LPS[lid]->bound->timestamp,
							lid,straggler->sender,LPS[lid]->current_group,
							GLPS[LPS[lid]->current_group]->lvt->timestamp,
							straggler->timestamp
						       );
					}

					rollback_group(straggler,IDLE_PROCESS);
                               }
			}
			break;
		
		case negative:

                        if(check_start_group(lid) &&  verify_time_group(LPS[lid]->bound->timestamp)){
                                if(straggler->timestamp <= lvt_receiver){
                               		PRINT_DEBUG_GLP{
						printf("RGB [NEGATIVE] T:%f S:%d R:%d\n",
							LPS[lid]->bound->timestamp, straggler->sender, lid);
					}
					
					// TODO analise case with antimessage, we have to consider 
					// matched message instead of bound
					rollback_group(straggler,lid);
 
				}
                        	else if(straggler->timestamp <= GLPS[LPS[lid]->current_group]->lvt->timestamp){
					PRINT_DEBUG_GLP{
						printf("GRB lvt_group [NEGATIVE] T:%f S:%d R:%d\n",
							LPS[lid]->bound->timestamp, straggler->sender, lid);
					}
					if(LPS[lid]->state == LP_STATE_SILENT_EXEC){
						PRINT_DEBUG_GLP printf("Rollback negative with SIL_EXEC\n");
						rollback_group(straggler,lid);	
					}	
					else
						rollback_group(straggler,IDLE_PROCESS);
				}
                        }
                        break;

	}

}

//TODO MN
void rollback_group(msg_t *straggler, unsigned int receiver){
	
	unsigned int i;
	LP_state *local_LP;
	GLP_state *current_group;

	current_group = GLPS[LPS[straggler->receiver]->current_group];
	PRINT_DEBUG_GLP{
		printf("S:%d R:%d Type:%d TIMESTAMP OF ROLLBACK %f\n",
			straggler->sender,
			straggler->receiver,
			straggler->type,
			straggler->timestamp);	
	}

	current_group->lvt = NULL;	
	if(receiver != IDLE_PROCESS){
		 if(LPS[receiver]->state == LP_STATE_SILENT_EXEC){
			PRINT_DEBUG_GLP	printf("R Straggler:%f\n",straggler->timestamp);
			while(LPS[receiver]->target_rollback->timestamp >= straggler->timestamp){
				LPS[receiver]->target_rollback = list_prev(LPS[receiver]->target_rollback);
					if(LPS[receiver]->target_rollback == NULL){
						rootsim_error(true,"Target rollback NULL\n");
					}
				PRINT_DEBUG_GLP	printf("R Mex:%f\n",LPS[receiver]->target_rollback->timestamp);
			}
			LPS[receiver]->bound = LPS[receiver]->target_rollback;
	        	current_group->lvt = LPS[receiver]->target_rollback;
			PRINT_DEBUG_GLP	printf("LP[%d] Selected_targhet_rollback:%f\n",receiver,LPS[receiver]->target_rollback->timestamp);
                }
                else{
			LPS[receiver]->target_rollback = LPS[receiver]->bound;
	        	current_group->lvt = LPS[receiver]->target_rollback;
		}
	}
	
	for(i=0; i<current_group->tot_LP; i++){
		local_LP = current_group->local_LPS[i];
		
		if(local_LP->lid != receiver){
			
			PRINT_DEBUG_GLP{
				printf("ROLLBACK GROUP LP[%d]\n",local_LP->lid);
			}
			
			PRINT_DEBUG_GLP	printf("LP[%d] lvt:%f\n",local_LP->lid,lvt(local_LP->lid));
			//Giving a timestamp it has to return the message with the maximum timestamp lesser than timestamp
			if(local_LP->state == LP_STATE_SILENT_EXEC){
				PRINT_DEBUG_GLP	printf("Straggler:%f\n",straggler->timestamp);
				while(local_LP->target_rollback->timestamp >= straggler->timestamp){
					local_LP->target_rollback = list_prev(local_LP->target_rollback);
					if(local_LP->target_rollback == NULL){
						rootsim_error(true,"Target rollback NULL\n");
					}
					PRINT_DEBUG_GLP	printf("Mex:%f\n",local_LP->target_rollback->timestamp);
				}
				local_LP->bound = local_LP->target_rollback;
				PRINT_DEBUG_GLP	printf("Selected:%f\n",local_LP->bound->timestamp);
			}
			else{
				local_LP->bound = list_get_node_timestamp(straggler->timestamp,local_LP->lid);
				local_LP->target_rollback = local_LP->bound;
			}
			PRINT_DEBUG_GLP	printf("LP[%d] Selected_targhet_rollback:%f\n",local_LP->lid,LPS[local_LP->lid]->target_rollback->timestamp);
		
			if(current_group->lvt == NULL || current_group->lvt->timestamp < local_LP->target_rollback->timestamp)
				current_group->lvt = local_LP->target_rollback;
                       	
			local_LP->state = LP_STATE_ROLLBACK;
		}
	}
	
	if(current_group->initial_group_time->timestamp >= straggler->timestamp && check_start_group(local_LP->lid)){
		PRINT_DEBUG_GLP{
			printf("RESET GROUP IGT:%f S->T:%f\n",current_group->initial_group_time->timestamp, straggler->timestamp);
		}
		current_group->state = GLP_STATE_WAIT_FOR_GROUP;
	        current_group->counter_synch = 0;
		reset_flag_counter_synch(LPS[straggler->receiver]->current_group);
	}
	else{
		current_group->state = GLP_STATE_ROLLBACK;
	        current_group->counter_rollback = current_group->tot_LP;
       		current_group->counter_silent_ex = current_group->tot_LP;
	}
	
}

bool check_start_group (unsigned int lid){
	return GLPS[LPS[lid]->current_group]->state != GLP_STATE_WAIT_FOR_GROUP;
}

void force_checkpoint_group(unsigned int lid){
	unsigned int i,lp_index = 0;
	msg_t control_msg;
        msg_hdr_t msg_hdr;
	GLP_state *current_group;
	LP_state **list;

	current_group = GLPS[LPS[lid]->current_group];
	list = current_group->local_LPS;
	
	for(i=0;i<current_group->tot_LP;i++){
		
		lp_index = list[i]->lid;
		
		// Diretcly place the control message in the target bottom half queue
		bzero(&control_msg, sizeof(msg_t));//TODO check if it could be removed
		control_msg.sender = LidToGid(lid);
		control_msg.receiver = LidToGid(lp_index);
		control_msg.type = NULL_LOG_MESSAGE;
		control_msg.timestamp = lvt(lid);
		control_msg.send_time = lvt(lid);
		control_msg.message_kind = positive;
		control_msg.mark = generate_mark(lid);

		// This message must be stored in the output queue as well, in case this LP rollback
		bzero(&msg_hdr, sizeof(msg_hdr_t)); //TODO check if it could be removed
		msg_hdr.sender = control_msg.sender;
		msg_hdr.receiver = control_msg.receiver;
		msg_hdr.timestamp = control_msg.timestamp;
		msg_hdr.send_time = control_msg.send_time;
		msg_hdr.mark = control_msg.mark;
		(void)list_insert(lid, LPS[lid]->queue_out, send_time, &msg_hdr);

		Send(&control_msg);

		lp_index++;
	}
}

bool check_postpone_synch_message(unsigned int lid){
	msg_t *msg = LPS[lid]->bound;
	msg_t *old_bound;

	while(list_next(msg) != NULL && D_EQUAL(list_next(msg)->timestamp, msg->timestamp) ){
		msg = list_next(msg);
		if(msg->type == RENDEZVOUS_START){
			old_bound = LPS[lid]->bound;
			LPS[lid]->bound = list_prev(LPS[lid]->bound);
			msg = list_extract_by_content(lid,LPS[lid]->queue_in,old_bound);
			list_insert(lid,LPS[lid]->queue_in,timestamp,msg);
			PRINT_DEBUG_GLP{	
				printf("Before state:%d\n",LPS[lid]->state);
			}
			LPS[lid]->state = LP_STATE_READY;
			GLPS[LPS[lid]->current_group]->counter_synch--;
			LPS[lid]->updated_counter = false;
			PRINT_DEBUG_GLP{
				printf("Inside check_postpone LP:%d \n",lid);
			}
			return true;
		}	
	}
	
	return false;
}

bool check_state_group(unsigned int lid_bound){
	
	msg_t *next_event;
        LP_state *temp_LP = LPS_bound[lid_bound];
        GLP_state *current_group = GLPS[temp_LP->current_group];

/*	printf("LP:%d LP->S:%d GLP->S:%d  is_blockG:%d CTG:%d VTG:%d\n",
		temp_LP->lid,
		temp_LP->state,
		current_group->state,
		is_blocked_state(current_group->state),
		check_start_group(temp_LP->lid),
		verify_time_group(lvt(temp_LP->lid))
		);
*/	
	if(temp_LP->state == LP_STATE_WAIT_FOR_GROUP){
        	if(current_group->state == GLP_STATE_WAIT_FOR_GROUP){
			if(check_postpone_synch_message(temp_LP->lid)){
				return false;
			}
                	return true;
		}
		else {
			// For avoiding deadlock in case next_event_timestamp is after the 
			// end group time
                        temp_LP->state = LP_STATE_READY;		
		}
        }
        
	if(temp_LP->state == LP_STATE_WAIT_FOR_LOG){
        	if(current_group->state == GLP_STATE_WAIT_FOR_LOG)
	                return true;
                else
                	//For avoiding deadlock in case of double log instance
                        temp_LP->state = LP_STATE_READY;
        }

	if(current_group->state == GLP_STATE_WAIT_FOR_UNBLOCK && temp_LP->state == LP_STATE_READY){
		next_event = list_next(LPS[temp_LP->lid]->bound);
		if(	next_event != NULL && 
			next_event->type == RENDEZVOUS_START &&
			next_event->sender == current_group->lvt->sender && 
			next_event->rendezvous_mark == current_group->lvt->rendezvous_mark &&
			verify_time_group(next_event->timestamp)
		  ){
			return false;
		}	
	}
       
	if( (is_blocked_state(temp_LP->state) && temp_LP->state != LP_STATE_WAIT_FOR_GROUP) ||
                        (is_blocked_state(current_group->state) &&
                        check_start_group(temp_LP->lid) &&
                        verify_time_group(lvt(temp_LP->lid)))){

                return true;
        }

        if(temp_LP->state == LP_STATE_SILENT_EXEC && current_group->state == GLP_STATE_ROLLBACK)
                return true;


        return false;

}

simtime_t get_delta_group(void){
	simtime_t result;
	result = DELTA_GROUP * statistics_get_data(STAT_GET_SIMTIME_ADVANCEMENT,0.0);
	if(result > 10)
		return result;
	else
		return DELTA_GROUP_TIME;
}

void check_state_order(unsigned int lid){
	state_t *restore_state;
	state_t *s=NULL;
	
	restore_state = list_tail(LPS[lid]->queue_states);
        while (restore_state != NULL) {
                printf("[%d] State: %f\n",lid,restore_state->lvt);
                s = restore_state;
                restore_state = list_prev(restore_state);
        }
}

void check_lvt_group(unsigned int lid){
	unsigned int j;
	GLP_state *current_group = GLPS[LPS[lid]->current_group];
		

	for(j=0;j<current_group->tot_LP;j++){
		if(lvt(current_group->local_LPS[j]->lid) < current_group->initial_group_time->timestamp)
			printf("ERRORE lvt group\n");	
	}
}

void print_blocked_group(void){
	unsigned int i,j =0;
	GLP_state current_group;
	LP_state mate;

	for(j=0;j<n_grp;j++){
		current_group = GLPS[j];		
		if(is_blocked_state(current_group->state) || GLP_STATE_READY_FOR_SYNCH==current_group->state || GLP_STATE_WAIT_FOR_LOG==current_group->state){
			printf("GLP[%d] state:%d RC:%d SYC:%d SIC:%d \t LP:",j,current_group->state,
				current_group->counter_rollback,
				current_group->counter_synch,
				current_group->counter_silent_exe
			);
			fori(i=0;i<current_group->tot_LP;i++){
				mate = current_group->local_LPS[i];
				printf("%d state:%d ",mate->lid,mate->state);
			}
			printf("\n");
		}	
	}
}

#endif
