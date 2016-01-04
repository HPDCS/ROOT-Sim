#include <scheduler/process.h>
#ifdef HAVE_GLP_SCH_MODULE


unsigned int find_lp_group(unsigned int last_lp, unsigned int group){
        for(;last_lp<n_prc;last_lp++){
                if(GLPS[group]->local_LPS[last_lp]!=NULL)
                        return last_lp;
        }
        return IDLE_PROCESS;
}

//TODO MN
void rollback_group(msg_t *straggler, unsigned int receiver){
	
	unsigned int i, lp_index = 0;
	LP_state *local_LP;
	GLP_state *current_group;

	current_group = GLPS[straggler->receiver];
	
	if(receiver != IDLE_PROCESS)
		LPS[receiver]->target_rollback = LPS[receiver]->bound;
	current_group->lvt = straggler;
	current_group->state = GLP_STATE_ROLLBACK;
	current_group->counter_rollback = current_group->tot_LP;
	current_group->counter_silent_ex = current_group->tot_LP; 

	if(current_group->tot_LP == 1) return;

	for(i=0; i<current_group->tot_LP; i++){
		lp_index = find_lp_group(lp_index,LPS[straggler->receiver]->current_group);
		if(lp_index == IDLE_PROCESS)
			rootsim_error(true, "Error, returned IDLE PROCESS during found LP GROUP. Aborting...");
		local_LP = LPS[lp_index];
		if(lp_index!=receiver){
			printf("ROLLBACK GROUP LP[%d]\n",lp_index);
                       	//Giving a timestamp it has to return the message with the maximum timestamp lesser than timestamp
                       	local_LP->bound = list_get_node_timestamp(straggler->timestamp,lp_index);
                       	local_LP->target_rollback = local_LP->bound;
                       	local_LP->state = LP_STATE_ROLLBACK;
		}

		lp_index++;
		
		
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
