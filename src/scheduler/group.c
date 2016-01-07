#include <scheduler/process.h>
#include <gvt/gvt.h>

#ifdef HAVE_GLP_SCH_MODULE
unsigned int find_lp_group(unsigned int last_lp, unsigned int group){
        for(;last_lp<n_prc;last_lp++){
                if(GLPS[group]->local_LPS[last_lp]!=NULL)
                        return last_lp;
        }
        return IDLE_PROCESS;
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
	unsigned int i,lp_index = 0;
	GLP_state *current_group = GLPS[group];
	
        for(i=0;i<current_group->tot_LP;i++){
                lp_index = find_lp_group(lp_index,group);
		LPS[lp_index]->updated_counter = false;
		lp_index++;
	}
	
	
}

void reset_synch_counter(unsigned int lid){
	LP_state *temp_LP = LPS[lid];
	GLP_state *current_group = GLPS[LPS[lid]->current_group];
 
	if(temp_LP->bound->type == SYNCH_GROUP || 
		(temp_LP->bound->mark == current_group->initial_group_time->mark && 
			D_EQUAL(temp_LP->bound->timestamp,current_group->initial_group_time->timestamp)) &&
	  	temp_LP->updated_counter
	    ){
		printf("DEC COUNTER LP:%d GLP:%d Type:%d \n",lid,LPS[lid]->current_group,temp_LP->bound->type);
		current_group->counter_synch--;	
		temp_LP->updated_counter = false;
	}
}

void check_rollback_group(msg_t *straggler, unsigned int lid, simtime_t lvt_receiver, int msg_case){
	
	switch(msg_case){
		case positive:
			if(check_start_group(lid) &&  verify_time_group(LPS[lid]->bound->timestamp)){
				if(straggler->timestamp < lvt_receiver){
                                        printf("RGB [POSITIVE Type:%d] T:%f S:%d R:%d\n",
                                        	straggler->type,LPS[lid]->bound->timestamp,
                                        	straggler->sender, lid);

                                	rollback_group(straggler,lid);
                         	}
				else if(straggler->timestamp < GLPS[LPS[lid]->current_group]->lvt->timestamp){
					printf("RGB ltv_group [POSITIVE Type:%d] T:%f R:%d S:%d GRP[%d]->lvt:%f msg->Time:%f\n",
						straggler->type,LPS[lid]->bound->timestamp,
						lid,straggler->sender,LPS[lid]->current_group,
						GLPS[LPS[lid]->current_group]->lvt->timestamp,
						straggler->timestamp
					       );

					rollback_group(straggler,IDLE_PROCESS);
                               }
			}
			break;
		
		case negative:
			if(check_IGT(GLPS[LPS[lid]->current_group]->initial_group_time,straggler)){
				msg_t control_msg;
				bzero(&control_msg, sizeof(msg_t));
				control_msg.sender = LidToGid(LPS[lid]->current_group);
				control_msg.receiver = LidToGid(lid);
				control_msg.type = SYNCH_GROUP;
				control_msg.timestamp = straggler->timestamp;
				control_msg.send_time = straggler->timestamp;
				control_msg.message_kind = positive;
				control_msg.mark = generate_mark(lid);
				Send(&control_msg);

				printf("ANTIMESSAGE INITIAL MESSAGE %d\n",lid);

			}

                        if(check_start_group(lid) &&  verify_time_group(LPS[lid]->bound->timestamp)){
                                if(straggler->timestamp <= lvt_receiver){
                               		 printf("RGB [NEGATIVE] T:%f S:%d R:%d\n",
						LPS[lid]->bound->timestamp, straggler->sender, lid);

						// TODO analise case with antimessage, we have to consider 
						// matched message instead of bound
						rollback_group(straggler,lid);
 
				}
                        	else if(straggler->timestamp <= GLPS[LPS[lid]->current_group]->lvt->timestamp){
					printf("GRB lvt_group [NEGATIVE] T:%f S:%d R:%d\n",
						LPS[lid]->bound->timestamp, straggler->sender, lid);
						// TODO understand if is correct IDLE_PROCESS
					
					rollback_group(straggler,IDLE_PROCESS);
				}
                        }
                        break;

	}

}

//TODO MN
void rollback_group(msg_t *straggler, unsigned int receiver){
	
	unsigned int i, lp_index = 0;
	LP_state *local_LP;
	GLP_state *current_group;

	current_group = GLPS[LPS[straggler->receiver]->current_group];
	
	printf("S:%d R:%d Type:%d TIMESTAMP OF ROLLBACK %f\n",
		straggler->sender,
		straggler->receiver,
		straggler->type,
		straggler->timestamp);	

	current_group->lvt = NULL;	
	if(receiver != IDLE_PROCESS){
		LPS[receiver]->target_rollback = LPS[receiver]->bound;
	        current_group->lvt = LPS[receiver]->target_rollback;
	}
	
	for(i=0; i<current_group->tot_LP; i++){
		lp_index = find_lp_group(lp_index,LPS[straggler->receiver]->current_group);
		if(lp_index == IDLE_PROCESS)
			rootsim_error(true, "Error, returned IDLE PROCESS during found LP GROUP. Aborting...");
		local_LP = LPS[lp_index];
		if(lp_index!=receiver){
			printf("ROLLBACK GROUP LP[%d]\n",lp_index);
			
			/*if(straggler->timestamp > local_LP->bound->timestamp){
				local_LP->target_rollback = NULL;
			}
			else{*/
				//Giving a timestamp it has to return the message with the maximum timestamp lesser than timestamp
				local_LP->bound = list_get_node_timestamp(straggler->timestamp,lp_index);
				local_LP->target_rollback = local_LP->bound;
			
				if(current_group->lvt == NULL || current_group->lvt->timestamp < local_LP->target_rollback->timestamp)
					current_group->lvt = local_LP->target_rollback;
		//	}
                       	local_LP->state = LP_STATE_ROLLBACK;
		}

		lp_index++;	
	}
	
	if(current_group->tot_LP > 1 && current_group->initial_group_time->timestamp >= straggler->timestamp){
		printf("RESET GROUP IGT:%f S->T:%f\n",current_group->initial_group_time->timestamp, straggler->timestamp);
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

	current_group = GLPS[LPS[lid]->current_group];
		
	for(i=0;i<current_group->tot_LP;i++){
		
		lp_index = find_lp_group(lp_index,LPS[lid]->current_group);
		
		if(lp_index == IDLE_PROCESS ){
			rootsim_error(true,"Returned IDLE_PROCESS during send NULL_LOG messages.Aborting...");
		}

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

#endif
