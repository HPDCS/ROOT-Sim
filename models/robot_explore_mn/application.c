#include "application.h"
#include "utility.c"
#include <string.h>

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, event_t *content, unsigned int size, lp_state_t *state) {
        event_t new_event;
        simtime_t timestamp;
	unsigned int i,j;
	unsigned int* new_agent;
	unsigned int* old_agent;
	unsigned int** temp_agents;

        switch(event) {

                case INIT: // must be ALWAYS implemented
			state = (lp_state_t *)malloc(sizeof(lp_state_t));
                        if(is_agent(me)){
				state->type = AGENT;
				state->region = random_region();
				/*
				// Allocate the presence bitmap
				state->visited_regions = ALLOCATE_BITMAP(get_tot_regions());
				bzero(state->visited_regions, BITMAP_SIZE(get_tot_regions()));
				*/
				
				state->visited_regions = (unsigned int *)malloc(get_tot_regions()*sizeof(unsigned int));
        			state->visited_counter = 0;
			}
			else{
				state->type = REGION;
				//state->actual_agent = (unsigned char **)malloc(get_tot_agents()*sizeof(unsigned char*));
				state->actual_agent = (unsigned int **)malloc(get_tot_agents()*sizeof(unsigned int *));
        			state->agent_counter = 0;     
        			state->obstacles = get_obstacles();
			}
                        
                        SetState(state);
			
                        timestamp = (simtime_t)(20 * Random());

			if(is_agent(me)){
				//Send ENTER message
				state->visited_regions[state->region] = 1;
				state->visited_counter++;

                        	new_event.visited_regions = state->visited_regions;
                        	new_event.sender = me;
                        	ScheduleNewEvent(state->region, timestamp, ENTER, &new_event, sizeof(new_event));
				
				//Send EXIT message		
                                new_event.sender = me;
                        	new_event.visited_regions = state->visited_regions;
                                ScheduleNewEvent(state->region, timestamp + Expent(DELAY), EXIT, &new_event, sizeof(new_event));
			}
			else{
				ScheduleNewEvent(me, timestamp, PING, NULL, 0);	
			}

                        break;

                case PING:
                        timestamp = now + Expent(DELAY);

                        ScheduleNewEvent(me, timestamp, PING, NULL, 0);
			
			break;

		case ENTER: 
			state->actual_agent[state->agent_counter] = content->visited_regions;
			state->agent_counter++;
			
			new_agent = content->visited_regions;
			temp_agents = state->actual_agent;
			for(i=0; i<state->agent_counter-1; i++){
				old_agent = (unsigned int *)temp_agents[i];
				for(j=0; j<get_tot_regions(); j++){
					/*
					if(BITMAP_CHECK_BIT(new_agent, j)  && BITMAP_CHECK_BIT(old_agent, j) == 0){
						BITMAP_SET_BIT(old_agent, j);
					}
					else if(BITMAP_CHECK_BIT(new_agent, j) == 0  && BITMAP_CHECK_BIT(old_agent, j)){
                                                BITMAP_SET_BIT(new_agent, j);
                                        }
					*/
					if(new_agent[j] == 1 && old_agent[j] == 0)
						old_agent[j] = 1;
					else if(new_agent[j] == 0 && old_agent[j] == 1)
                                                new_agent[j] = 1;
				}
			}
			
			break;

		case EXIT: 
			new_event.destination = get_region(me,state->obstacles,content->sender);
			
			for(i=0; i<state->agent_counter; i++){
				if(state->actual_agent[i] == content->visited_regions){
					if(state->agent_counter != 1){
						state->actual_agent[i] = state->actual_agent[state->agent_counter-1];
					}
						
					state->agent_counter--;
					break;
				}
			}
			
			ScheduleNewEvent(content->sender, now, DESTINATION, &new_event, sizeof(new_event));

			break;

		case DESTINATION: 
			
			//Send ENTER message
			state->region = content->destination;
			state->visited_regions[state->region] = 1;
                        state->visited_counter = 0;
			for(i=0;i<get_tot_regions();i++){
				if(state->visited_regions[i] == 1)
		                        state->visited_counter++;
				
			}

			new_event.sender = me;
			if(state->visited_counter/get_tot_regions() >= VISITED){
				if(me + 1 == n_prc_tot)
					ScheduleNewEvent(0,now,COMPLETE,&new_event, sizeof(new_event));
				else
					ScheduleNewEvent(me+1,now,COMPLETE,&new_event, sizeof(new_event));
				break;
			}
			
			new_event.visited_regions = state->visited_regions;
			ScheduleNewEvent(content->destination, now, ENTER, &new_event, sizeof(new_event));

			//Send EXIT message             
			new_event.sender = me;
			new_event.visited_regions = state->visited_regions;
			ScheduleNewEvent(state->region, now + Expent(DELAY), EXIT, &new_event, sizeof(new_event));

			break;

		case COMPLETE:
			state->visited_counter = get_tot_regions();
			
			if(me + 1 != content->sender){
				if(me + 1 == n_prc_tot)
                                        ScheduleNewEvent(0,now,COMPLETE,content, sizeof(event_t));
                                else
                                        ScheduleNewEvent(me+1,now,COMPLETE,content, sizeof(event_t));
			}
		
			break;		
        }
}

bool OnGVT(unsigned int me, lp_state_t *snapshot) {
	unsigned int tot_reg = get_tot_regions();
	bool is_agent = false;	
	if(snapshot->type == AGENT)
		is_agent = true;

        if(is_agent && me == tot_reg){
                printf("Completed work: %f\%\n", (double)(snapshot->visited_counter/tot_reg)*100);
        }
	
	if(snapshot->visited_counter/tot_reg < VISITED)
		return false;
	return true;
}
