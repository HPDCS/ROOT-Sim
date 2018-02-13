#include "application.h"

int total_num_bugs = 1;
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
	
	if(BUG_PER_CELL == 1){
		if(me == 0){
			printf("generating REGION_IN for %d\n",me);
			ScheduleNewEvent(me, now + (simtime_t)(20*Random()), REGION_IN, new_event_content, sizeof(new_event_content));
			return;
		}
		else
			return;
	}
	
	//this code left is for further improvements
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
	int count;
	int times;

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

			//initialize data structures+

			total_num_bugs = 1;

			for(i = 0; i < 4; i++){
				pointer->neighbour_bugs[i] = 0;
			}
			
			pointer->present = 0;
			pointer->explored = 0;
			pointer->bug_size = 1;
			pointer->food_availability = 0.0;
			pointer->lvt = 0;
			pointer->food_production = RandomRange(0,MAX_FOOD_PRODUCTION_RATE);
			pointer->food_consumption = (MAX_FOOD_CONSUMPTION_RATE > pointer->food_availability) ? MAX_FOOD_CONSUMPTION_RATE : pointer->food_availability;

			new_event_content.cell = me;
			new_event_content.present = 0;
			new_event_content.bug_size = 1;
			new_event_content.dying = 0;

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
			pointer->bug_size += pointer->food_consumption;
			pointer->food_availability -= pointer->food_consumption;
			if(pointer->food_availability < 0)
				pointer->food_availability = 0.0;
			
			//printf("entering region %d and bug size is %u (event content %u)\n", me, pointer->bug_size, temp);

			new_event_content.cell = me;
			new_event_content.present = pointer->present;
			new_event_content.bug_size = pointer->bug_size;
			new_event_content.dying = 0;

			if(pointer->bug_size >= 10){
				printf("bug in cell %d is reproducing!\n",me);	
				
				//reproduce to 5 new bugs
				for(i = 0; i < 5; i++){ //split in 5 different siblings, so find a direction for each of them
					count = 0; times = 0;
					do{
						count = RandomRange(0,3);
						if(pointer->neighbour_bugs[count] < BUG_PER_CELL){
							pointer->neighbour_bugs[count] += 1;
							receiver = GetReceiver(TOPOLOGY_TORUS, count);
							new_event_content.bug_size = 1; //sibling's initial size is 1
							total_num_bugs++;
							ScheduleNewEvent(receiver, now + (simtime_t) (TIME_STEP * Random()), REGION_IN, &new_event_content, sizeof(new_event_content));
						}
						times++;
					}while(times < 5); //if no location is identified within 5 random rows, the sibling "dies" (i.e.: the REGION_IN event is never sent).
				}
				
				new_event_content.dying = 1; //in any case, the parent dies.
			}
			
			//for every neighbour I have, send them an UPDATE_NEIGHBOUR event to notify them that a bug passed through me
			send_update_neighbours(me, now, &new_event_content);

			//a bug is going outside me, I need to notify myself
			ScheduleNewEvent(me, now + TIME_STEP/100000, REGION_OUT, &new_event_content, sizeof(new_event_content));

			break;
		
		case UPDATE_NEIGHBOURS:
			
			//update only the entry dedicated to the sender cell with the number of bugs that are inside it
			for(i = 0; i < 4; i++){
				if(event_content->cell == GetReceiver(TOPOLOGY_TORUS,i)){
					if(event_content->dying != 0) //if the event was sent by a dying bug, it means that this cell will be empty.
						pointer->neighbour_bugs[i] = 0;
					else
						pointer->neighbour_bugs[i] = event_content->present;
				}
			}

			break;
		
		case PRODUCE_FOOD:
			pointer->food_production = RandomRange(0,MAX_FOOD_PRODUCTION_RATE);
			pointer->food_availability += pointer->food_production;
			
			//printf("producing food at cell %d, product %f and avail %f\n", me, pointer->food_production, pointer->food_availability); 
			ScheduleNewEvent(me, now + TIME_STEP/1000, PRODUCE_FOOD, NULL, 0);

			break; 

		case REGION_OUT:
			
			pointer->present--;

			// if the bug who sent me this event was dying, it means that he already notified its neighbours. Do nothing else then.
			if(event_content->dying != 0)				
				goto die;

			new_event_content.cell = me;
			new_event_content.present = pointer->present;
			// increase bug size every time it moves...
			new_event_content.bug_size = pointer->bug_size;
			new_event_content.dying = 0;

			// for every neighbour I have, send them an UPDATE_NEIGHBOUR event to notify them that a bug is going outside me
			send_update_neighbours(me, now, &new_event_content);
		
			if(total_num_bugs >= TOT_REG) //avoid bugs deadlocks!!
				goto die;
			else{
				// randomly choose if this bug needs to die 
				if(RandomRange(0,100) >= SURVIVAL_PROBABILITY && total_num_bugs > 1)
					goto die;
			}

			//choose a random direction to take, and get the corresponding cell ID (receiver)
			do{
				i = (int) RandomRange(0,3);
				receiver = GetReceiver(TOPOLOGY_TORUS, i);
			}while(pointer->neighbour_bugs[i] >= BUG_PER_CELL);

			//The bug is going to another cell, send a REGION_IN to it. TODO: Only uniform distribution used for timestamp
            ScheduleNewEvent(receiver, now + (simtime_t) (TIME_STEP * Random()), REGION_IN, &new_event_content, sizeof(new_event_content));
			break;
			die:
			total_num_bugs--;

			//Sanity check
			if(total_num_bugs < 0)
				rootsim_error(true,"%s:%d Reached a negative number of bugs: %d\n"__FILE__,__LINE__, total_num_bugs);

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

	printf(" and size of last passed bug is %f. Left bugs are %d\n",snapshot->bug_size, total_num_bugs);

	if(total_num_bugs == 0){
		printf("All bugs died!\n");
		return 1;
	}

	if(snapshot->lvt < EXECUTION_TIME)
		return 0;

	
	return 1;
}
