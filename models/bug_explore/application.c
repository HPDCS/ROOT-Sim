#include "application.h"

unsigned int execution_time = EXECUTION_TIME; //this variable is updated by a user parameter

/**
 * This (helper) function schedules a new REGION_IN event towards 
 * the cells that are lower/greater than NUM_OCCUPIED_CELLS
 *
 * @author Matteo Principe
 * @date 8 feb 2018
 *
 * @param me The id of the cell who is calling the function
 * @param now The current time of the cell
*/
void generate_init_region_in(int me, simtime_t now, event_content_type *new_event_content){
	int i;
	int parity;

	parity = 1;
	if(NUM_OCCUPIED_CELLS %2 == 0)
		parity = 0;

	if(me < (int)(NUM_OCCUPIED_CELLS/2 + parity) || me >= (int) (n_prc_tot - (NUM_OCCUPIED_CELLS/2 + parity)) ){
		printf("generating REGION_IN for %d\n",me);
		for(i = 0; i < BUG_PER_CELL;i++){
			ScheduleNewEvent(me, now + (simtime_t)(20*Random()), REGION_IN, new_event_content, sizeof(new_event_content));
		}

	}

}
/**
 * This (helper) function schedules a new UPDATE_NEIGHBOURS
 * event towards the cells that represent my neighbours
 *
 * @author Matteo Principe
 * @date 8 feb 2018
 *
 * @param me The id of the cell who is calling the function
 * @param now The current time of the cell
 * @param present The number of bugs that are inside the cell
*/

void send_update_neighbours(int me, simtime_t now, event_content_type *new_event_content){
	int receiver;
	int i;

	for(i = 0; i < 4; i++){
		receiver = GetReceiver(TOPOLOGY_TORUS,i);
		if(receiver >= (int) n_prc_tot || receiver < 0){
			rootsim_error(true,"%s:%d Got receiver out of bounds!\n",__FILE__, __LINE__);
		}	
		ScheduleNewEvent(receiver, now + TIME_STEP/100000, UPDATE_NEIGHBOURS, new_event_content, sizeof(new_event_content));
	}

}

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, int event_size, lp_state_type *pointer) {

	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.present = -1;
	
	int i;
	int receiver;

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

			//initialize data structures
			for(i = 0; i < 4; i++){
				pointer->neighbour_bugs[i] = 0;
			}
			
			pointer->present = 0;
			pointer->explored = 0;
			pointer->bug_size = 1;
			pointer->food_availability = 0.0;
			pointer->food_production = RandomRange(0,MAX_FOOD_PRODUCTION_RATE);
			pointer->food_consumption = (MAX_FOOD_CONSUMPTION_RATE > pointer->food_availability) ? MAX_FOOD_CONSUMPTION_RATE : pointer->food_availability;
		
			new_event_content.cell = me;
			new_event_content.present = 0;
			new_event_content.bug_size = 1;

			ScheduleNewEvent(me, now + TIME_STEP/1000, PRODUCE_FOOD, NULL, 0);
			//send REGION_IN towards first and last cells	
			generate_init_region_in(me,now,&new_event_content);
	


			break;
		
		case REGION_IN:
			
			pointer->present++;
			if(pointer->explored == 0)
				pointer->explored++;

			//Sanity check
			if(pointer->present > BUG_PER_CELL ){
				rootsim_error(true,"%s:%d: More than BUG_PER_CELL (%d) are inside cell %d !\n", BUG_PER_CELL, me);
			}
			
			pointer->food_consumption = (MAX_FOOD_CONSUMPTION_RATE < pointer->food_availability) ? MAX_FOOD_CONSUMPTION_RATE : pointer->food_availability;
			pointer->bug_size = pointer->food_consumption;
			pointer->food_availability =- pointer->food_consumption;
			if(pointer->food_availability < 0)
				pointer->food_availability = 0.0;
			
			//printf("entering region %d and bug size is %u (event content %u)\n", me, pointer->bug_size, temp);

			new_event_content.cell = me;
			new_event_content.present = pointer->present;
			
			//for every neighbour I have, send them an UPDATE_NEIGHBOUR event to notify them that a bug passed through me
			send_update_neighbours(me, now, &new_event_content);
			
			//a bug is going outside me, I need to notify myself
			ScheduleNewEvent(me, now + TIME_STEP/100000, REGION_OUT, &new_event_content, sizeof(new_event_content));

			break;
		
		case UPDATE_NEIGHBOURS:
			
			//update only the entry dedicated to the sender cell with the number of bugs that are inside it
			for(i = 0; i < 4; i++){
				if(event_content->cell == GetReceiver(TOPOLOGY_TORUS,i)){
					pointer->neighbour_bugs[i] = event_content->present;
				}
			}

			break;
		
		case PRODUCE_FOOD:
			pointer->food_production = RandomRange(0,MAX_FOOD_PRODUCTION_RATE);
			pointer->food_availability =+ pointer->food_production;
			
			//printf("producing food at cell %d, product %f and avail %f\n", me, pointer->food_production, pointer->food_availability); 
			ScheduleNewEvent(me, now + TIME_STEP/1000, PRODUCE_FOOD, NULL, 0);

			break; 
		
		case REGION_OUT:
			
			pointer->present--;
		
			new_event_content.cell = me;
			new_event_content.present = pointer->present;
			//increase bug size every time it moves...
			new_event_content.bug_size = event_content->bug_size;
			
			//for every neighbour I have, send them an UPDATE_NEIGHBOUR event to notify them that a bug is going outside me
			send_update_neighbours(me, now, &new_event_content);
			
			//choose a random direction to take, and get the corresponding cell ID (receiver)
			do{
				i = (int) RandomRange(0,3);
				receiver = GetReceiver(TOPOLOGY_TORUS, i);
			}while(pointer->neighbour_bugs[i] >= BUG_PER_CELL);

			//The bug is going to another cell, send a REGION_IN to it. TODO: Only uniform distribution used for timestamp
            ScheduleNewEvent(receiver, now + (simtime_t) (TIME_STEP * Random()), REGION_IN, &new_event_content, sizeof(new_event_content));
			break;
		
		default:
			rootsim_error(true,"%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
	}
	

}

int OnGVT(unsigned int me, lp_state_type *snapshot){
	
	if(snapshot->explored == 0) 
		printf("cell %u not explored yet (%u)",me, snapshot->explored);
	else
		printf("cell %u explored (%u)",me, snapshot->explored);

	printf(" and size of last passed bug is %f\n",snapshot->bug_size);

	if(snapshot->lvt < EXECUTION_TIME)
		return 0;
	
	return 1;
}
