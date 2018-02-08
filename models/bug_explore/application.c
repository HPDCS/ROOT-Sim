#include "application.h"

unsigned int execution_time = EXECUTION_TIME;

void generate_init_region_in(int me, simtime_t now){
	int i;
	int parity;
	
	parity = 1;
	if(NUM_OCCUPIED_CELLS %2 == 0)
		parity = 0;

	if(me < (int)(NUM_OCCUPIED_CELLS/2 + parity) || me >= (int) (n_prc_tot - (NUM_OCCUPIED_CELLS/2 + parity)) ){
		printf("generating REGION_IN for %d\n",me);
		for(i = 0; i < BUG_PER_CELL;i++){
			ScheduleNewEvent(me, now, REGION_IN, NULL, 0);
		}

	}

}

void send_update_neighbours(int me, simtime_t now, int present){
	event_content_type new_event_content;
	int receiver;
	int i;

	new_event_content.cell = me;
	new_event_content.present = present;

	for(i = 0; i < 4; i++){
		receiver = GetReceiver(TOPOLOGY_TORUS,i);
		if(receiver >= (int) n_prc_tot || receiver < 0){
			rootsim_error(true,"%s:%d Got receiver out of bounds!\n",__FILE__, __LINE__);
		}	
		ScheduleNewEvent(receiver, now + TIME_STEP, UPDATE_NEIGHBOURS, &new_event_content, sizeof(new_event_content));
	}

}
void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, int event_size, lp_state_type *pointer) {

	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.present = -1;
	
	int i;
	int receiver;
	simtime_t timestamp = 0;

	if(pointer!= NULL) {
		pointer->lvt = now;
	}


	switch(event_type){

		case INIT:
			
			if(IsParameterPresent(event_content, "execution_time"))
				execution_time = GetParameterInt(event_content, "execution_time");
						
			pointer = (lp_state_type *)malloc(sizeof(lp_state_type));
			if(pointer == NULL){
				rootsim_error(true,"%s:%d: Unable to allocate memory!", __FILE__, __LINE__);
			}

			SetState(pointer);
			
			//Sanity check
			if(NUM_OCCUPIED_CELLS > n_prc_tot){
				rootsim_error(true, "%s:%d: Require more cell than available LPs\n", __FILE__, __LINE__);
			}
	
			generate_init_region_in(me,now);
			
			for(i = 0; i < 4; i++){
				pointer->neighbour_bugs[i] = 0;
			}
			
			pointer->present = 0;
			pointer->explored = 0;

			break;
		
		case REGION_IN:
			
			pointer->present++;
			pointer->explored++;

			//Sanity check
			if(pointer->present > BUG_PER_CELL ){
				rootsim_error(true,"%s:%d: More than BUG_PER_CELL (%d) are inside cell %d !\n", BUG_PER_CELL, me);
			}

			new_event_content.cell = me;
			new_event_content.present = pointer->present;

			for(i = 0; i < 4; i++){
				receiver = GetReceiver(TOPOLOGY_TORUS,i);
				if(receiver >= (int) n_prc_tot || receiver < 0){
					rootsim_error(true,"%s:%d Got receiver out of bounds!\n",__FILE__, __LINE__);
				}	
				ScheduleNewEvent(receiver, now + TIME_STEP/100000, UPDATE_NEIGHBOURS, &new_event_content, sizeof(new_event_content));
			}

			ScheduleNewEvent(me, now + TIME_STEP/100000, REGION_OUT, NULL,0);

			break;
		
		case UPDATE_NEIGHBOURS:
			
			for(i = 0; i < 4; i++){
				if(event_content->cell == GetReceiver(TOPOLOGY_TORUS,i)){
					pointer->neighbour_bugs[i] = event_content->present;
				}
			}

			break;

		case REGION_OUT:
			
			pointer->present--;

			new_event_content.cell = me;
			new_event_content.present = pointer->present;

			for(i = 0; i < 4; i++){
				receiver = GetReceiver(TOPOLOGY_TORUS,i);
				if(receiver >= (int) n_prc_tot || receiver < 0){
					rootsim_error(true,"%s:%d Got receiver out of bounds!\n",__FILE__, __LINE__);
				}	
				ScheduleNewEvent(receiver, now + TIME_STEP/100000, UPDATE_NEIGHBOURS, &new_event_content, sizeof(new_event_content));
			}

			do{
				i = (int) RandomRange(0,3);
				receiver = GetReceiver(TOPOLOGY_TORUS, i);
			}while(pointer->neighbour_bugs[i] >= BUG_PER_CELL);

			// Uniform distribution used for timestamp
            ScheduleNewEvent(receiver, now + (simtime_t) (TIME_STEP * Random()), REGION_IN, NULL, 0);
			break;
		
		default:
			rootsim_error(true,"%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
	}
	

}

int OnGVT(unsigned int me, lp_state_type *snapshot){
	
	if(snapshot->explored == 0) printf("cell %u not explored yet (%u), lvt is %f\n",me, snapshot->explored, snapshot->lvt);

	if(snapshot->lvt < EXECUTION_TIME)
		return 0;
	
	return 1;
}
