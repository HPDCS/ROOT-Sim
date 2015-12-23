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
#endif
