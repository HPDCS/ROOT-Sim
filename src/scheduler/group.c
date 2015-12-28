#include <scheduler/process.h>
#ifdef HAVE_GLP_SCH_MODULE
//TODO MN
void rollback_group(simtime_t timestamp, unsigned int receiver){
	
	unsigned int i;
	LP_state **list;
	
	if(GLPS[LPS[receiver]->current_group]->tot_LP == 1) return;

	list = GLPS[LPS[receiver]->current_group]->local_LPS;

	for(i=0; i<n_prc; i++){
		if(i!=receiver && list[i]!=NULL && timestamp < lvt(i)){

			printf("ROLLBACK GROUP LP[%d]\n",i);
			//Giving a timestamp it has to return the message with the maximum timestamp lesser than timestamp
			list[i]->bound = list_get_node_timestamp(timestamp,i);
//			list_get_node_timestamp(timestamp,i);
        		list[i]->state = LP_STATE_ROLLBACK;
		}
	}

}

bool check_start_group(simtime_t timestamp, unsigned int lid){
	if(GLPS[LPS[lid]->current_group]->initial_group_time < timestamp){
		GLPS[LPS[lid]->current_group]->initial_group_time = 0.0;
		return true;
	}
	
	return false;
}

void force_checkpoint_group(unsigned int lid){
	unsigned int i;
	msg_t control_msg;
        msg_hdr_t msg_hdr;	

	for(i=0;i<n_prc;i++){
		if(GLPS[LPS[lid]->current_group]->local_LPS[i] != NULL){
			// Diretcly place the control message in the target bottom half queue
		        bzero(&control_msg, sizeof(msg_t));//TODO check if it could be removed
			control_msg.sender = LidToGid(lid);
        		control_msg.receiver = LidToGid(i);
        		control_msg.type = NULL_LOG_MESSAGE;
        		control_msg.timestamp = lvt(lid);
        		control_msg.send_time = lvt(lid);
        		control_msg.message_kind = positive;
        		control_msg.mark = generate_mark(current_lp);

			// This message must be stored in the output queue as well, in case this LP rollback
			bzero(&msg_hdr, sizeof(msg_hdr_t)); //TODO check if it could be removed
        		msg_hdr.sender = control_msg.sender;
       			msg_hdr.receiver = control_msg.receiver;
		        msg_hdr.timestamp = control_msg.timestamp;
		        msg_hdr.send_time = control_msg.send_time;
		        msg_hdr.mark = control_msg.mark;
		        (void)list_insert(lid, LPS[lid]->queue_out, send_time, &msg_hdr);

			Send(&control_msg);
		}

	}
}

#endif
