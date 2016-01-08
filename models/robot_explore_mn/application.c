#include "application.h"


void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, event_t *content, unsigned int size, lp_state_t *state) {
        event_t new_event;
        simtime_t timestamp;
	unsigned int i,j;
	char* new_agent,old_agent;

        switch(event) {

                case INIT: // must be ALWAYS implemented
                        if(is_agent(me)){
				state->type = AGENT;
				state->region = random_region();
        			state->visited_regions = (char *)malloc(get_tot_regions()*sizeof(char));
				bzero((void *)state->visited_regions,get_tot_regions()*sizeof(char));
        			state->visited_counter = 0;
			}
			else{
				state->type = REGION;
				state->actual_agent = (char **)malloc(get_tot_agents()*sizeof(char*));
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

			for(i=0; i<state->agent_counter-1; i++){
				old_agent = state->actual_agent[i];
				for(j=0; j<get_tot_region(); j++){
					if(new_agent[j] == 1 && old_agent[j] == 0)
						old_agent[j] = 1;
					else  if(new_agent[j] == 0 && old_agent[j] == 1)
						new_agent[j] = 1;
				}
			}
			
			break;

		case EXIT: 
			new_event.destination = get_region(me,state->obstacles,content->sender);
			
			for(i=0; i<state->agent_counter; i++){
				if(state->actual_agent[i] == content->visited_region){
					if(state->agent_counter != 1){
						state->actual_agent[i] = state->actual_agent[state->agent_counter-1]
					}
						
					state->agent_counter--;
					break;
				}
			}
			
			ScheduleNewEvent(me, now, DESTINATION, &new_event, sizeof(new_event));

			break;

		case DESTINATION: 
			//Send ENTER message
			state->region = content->destination;
			state->visited_regions[state->region] = 1;
                        state->visited_counter++;
			
			new_event.visited_regions = state->visited_regions;
			new_event.sender = me;

			ScheduleNewEvent(content->destination, now, ENTER, &new_event, sizeof(new_event));

			//Send EXIT message             
			new_event.sender = me;
			new_event.visited_regions = state->visited_regions;
			ScheduleNewEvent(state->region, now + Expent(DELAY), EXIT, &new_event, sizeof(new_event));

			break;
        }
}

bool OnGVT(unsigned int me, lp_state_t *snapshot) {

        if(me == 0) {
                printf("Completed work: %f\%\n", (double)snapshot->packet_count/PACKETS*100);
        }

        if (snapshot->packet_count < PACKETS)
                return false;
        return true;
}
