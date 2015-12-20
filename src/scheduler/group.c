#include <scheduler/process.h>
//TODO MN
void rollback_group(simtime_t timestamp, unsigned int receiver){
	
	int i;
	LP_state **list;
	
	list = GLPS[LPS[receiver]->current_group]->local_LPS;

	for(i=0; i<n_prc; i++){
		if(i!=receiver && list[i]!=NULL && list[i]->bound->timestamp >= timestamp){
			
			//Giving a timestamp it has to return the message with the maximum timestamp lesser than timestamp
			list[i]->bound = list_get_node_timestamp(timestamp,i);
        		list[i]->state = LP_STATE_ROLLBACK;
		}
	}

}
