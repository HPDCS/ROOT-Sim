#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"
#include "utility.c"

#define DEBUG if(0)

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state) {
        enter_t eneter;
	exit_t exit;
	destination_t destination;
	complete_t complete;

        simtime_t timestamp;

	lp_agent_t *agent;
	lp_region_t *region;

	unsigned char *old_agent;
	unsigned int i,j;
	
        switch(event) {

                case INIT: // must be ALWAYS implemented
			
                        if(is_agent(me)){
				agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
				
				agent->complete = false;
				agent->region = random_region();
				agent->map = (unsigned char *)calloc(get_tot_regions(),sizeof(unsigned char));
        			agent->counter = 0;

				SetState(agent);
			}
			else{
				region = (lp_region_t *)malloc(sizeof(lp_region_t));
				
				region->complete = false;
				region->guests = calloc(get_tot_agents(),sizeof(unsigned char *));
        			region->counter = 0;     
        			region->obstacles = get_obstacles();

				SetState(region);	
			}
			
                        timestamp = (simtime_t)(20 * Random());

			if(is_agent(me)){
				SET_BIT_AT(agent->map,agent->region);
				agent->counter++;

                        	enter.map = agent->map;
				ScheduleNewEvent(agent->region, timestamp, ENTER, &enter, sizeof(enter));
                        	DEBUG	printf("AGENT[%d] send ENTER to REGION:%d\n",me,agent->region);
				
				//Send EXIT message
				exit.agent = me;		
                        	exit.map = agent->map;
                                ScheduleNewEvent(agent->region, timestamp + Expent(DELAY), EXIT, &exit, sizeof(exit));
                        	DEBUG	printf("AGENT[%d] send EXIT to REGION:%d\n",me,agent->region);
			}
			else{
                        	DEBUG	printf("REGION[%d] send PING\n",me);
				ScheduleNewEvent(me, timestamp, PING, NULL, 0);	
			}

                        break;

                case PING
                        ScheduleNewEvent(me, now + Expent(DELAY), PING, NULL, 0);
			
			break;

		case ENTER:
			enter = (enter_t) content;
			region = (lp_region_t *) state;
			
			region->guests[region->counter] = enter->map;
			
			for(i=0; i<region->counter; i++){
				old_agent = region->guests[i];

				for(j=0; j<get_tot_regions(); j++){
					if(!CHECK_BIT_AT(enter->map,j) && CHECK_BIT_AT(old_agent,j))
						SET_BIT_AT(enter->map,j);
					else if(CHECK_BIT_AT(enter->map,j) && !CHECK_BIT_AT(old_agent,j))
						SET_BIT_AT(old_agent,j);
				}
			}
			
			region->counter++;	
			
			break;

		case EXIT: 
			exit = (exit_t) content;
			region = (lp_region_t *) state;
			
			destination.region = get_region(me,region->obstacles,exit->agent);

			for(i=0;i<region->counter;i++){
				if(region->guests[i] == exit->map){
					if(i!=(region->counter-1) && region->counter >= 1)
						region->guests[i] = region->guests[region->counter-1];
					
					region->counter--;	
					break;
				}
			}
			
			ScheduleNewEvent(content->sender, now + Expent(DELAY), DESTINATION, &destination, sizeof(destination));
			
			break;

		case DESTINATION: 
			destiantion = (destiantion_t) content;
			agent = (lp_agent_t *) state;

			agent->region = destination->destination;
			SET_BIT_AT(agent->map,destination->region);

                        agent->counter = 0;
			for(i=0; i<get_tot_regions(); i++){
				if(CHECK_BIT_AT(agent->map,i)
					agent->counter++;
			}
				
			if(check_termination(agent)){
				DEBUG	printf("Agente:%d complete! AVC:%d\n",me,agent->visited_counter);
				agent->complete = true;
	                        
				complete.agent = me;
				
				if(me + 1 == n_prc_tot)
			 		ScheduleNewEvent(0, now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
				else	
			 		ScheduleNewEvent(me + 1, now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));

				break;
			}
			
			timestamp = now + Expent(DELAY);			

                        enter.map = agent->map;
			ScheduleNewEvent(content->destination, timestamp, ENTER, &enter, sizeof(enter));

                        exit.agent = me;
                        exit.map = agent->map;
			ScheduleNewEvent(content->destination, timestamp + Expent(DELAY), EXIT, &exit, sizeof(exit));
			break;

		case COMPLETE:
			if(is_agent(me)){
				agent = (lp_agent_t *) state;
				agent->complete = true;
				agent->counter = get_tot_regions();
			}

                        complete.sender = me;

                        if(me + 1 == n_prc_tot)
				ScheduleNewEvent(0,  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
			else
				ScheduleNewEvent(me + 1,  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));

			break;
		
        }
}

bool OnGVT(unsigned int me, void *snapshot) {
	lp_agent_t *agent;
	
	if(is_agent(me)){
		agent = (lp_agent_t *) snapshot;	
		
		DEBUG{	
			unsigned int i;
			printf("Agent[%d] VC:%d \t{",me,agent->visited_counter);
			for(i=0;i<get_tot_regions();i++)
				printf("%d ",agent->visited_regions[i]);
			printf("}\n");
		}
		
        	if(me == get_tot_regions())
                	printf("Completed work: %f\%\n", percentage(agent));
        	
		
			
		if(check_termination(agent)){
			DEBUG	printf("[ME:%d] Complete:%f flag:%d\n",me,percentage(agent),agent->complete);
			return false;
		}
	
		DEBUG printf("%d complete execution  C:%f F:%d\n",me,percentage(agent),agent->complete);
	}
	
	return true;
}
