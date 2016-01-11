#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"
#include "utility.c"

#define DEBUG if(1)

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, event_t *content, unsigned int size, void *state) {
        event_t new_event;
        simtime_t timestamp;
	lp_agent_t *agent;
	lp_region_t *region;
	unsigned int *old_agent;
	unsigned int i,j;
	
        switch(event) {

                case INIT: // must be ALWAYS implemented
			
                        if(is_agent(me)){
				agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
				agent->type = AGENT;
				agent->complete = false;
				agent->region = random_region();
				agent->visited_regions = (unsigned int *)calloc(get_tot_regions(),sizeof(unsigned int));
        			agent->visited_counter = 0;
				SetState(agent);
			}
			else{
				region = (lp_region_t *)malloc(sizeof(lp_region_t));
				region->type = REGION;
				region->complete = false;
				region->actual_agents = calloc(get_tot_agents(),sizeof(void *));
        			region->agent_counter = 0;     
        			region->obstacles = get_obstacles();
				SetState(region);	
			}
			
                        timestamp = (simtime_t)(20 * Random());

			if(is_agent(me)){
				//Send ENTER message
				agent->visited_regions[agent->region] = 1;
				agent->visited_counter++;

                        	new_event.visited_regions = agent->visited_regions;
                        	new_event.sender = me;
                        	printf("AGENT[%d] send ENTER to REGION:%d\n",me,agent->region);
				ScheduleNewEvent(agent->region, timestamp, ENTER, &new_event, sizeof(new_event));
				
				//Send EXIT message		
                        	printf("AGENT[%d] send EXIT to REGION:%d\n",me,agent->region);
                                ScheduleNewEvent(agent->region, timestamp + Expent(DELAY), EXIT, &new_event, sizeof(new_event));
			}
			else{
                        	printf("REGION[%d] send PING\n",me);
				ScheduleNewEvent(me, timestamp, PING, NULL, 0);	
			}

                        break;

                case PING:
                        timestamp = now + Expent(DELAY);

                        ScheduleNewEvent(me, timestamp, PING, NULL, 0);
			
			break;

		case ENTER:
			region = (lp_region_t *) state;
			region->actual_agents[region->agent_counter] = content->visited_regions;
			
			for(i=0;i<region->agent_counter;i++){
				old_agent = region->actual_agents[i];
				for(j=0;j<get_tot_regions();j++){
					if(content->visited_regions[j]==0 && old_agent[j]==1)
						content->visited_regions[j]=1;
					else if(content->visited_regions[j]==1 && old_agent[j]==0)
                                                old_agent[j]=1;
				}
				region->actual_agents[i] = old_agent;
			}
			
			region->agent_counter++;	
			
			break;

		case EXIT: 
			region = (lp_region_t *) state;
			
			new_event.destination = get_region(me,region->obstacles,content->sender);
			new_event.visited_regions = NULL;
			new_event.sender = me;

			for(i=0;i<region->agent_counter;i++){
				if(region->actual_agents[i] == content->visited_regions){
					if(i==(region->agent_counter-1) || region->agent_counter == 1){
						region->actual_agents[i] = NULL;
					}
					else{
						region->actual_agents[i] = region->actual_agents[region->agent_counter-1];
					}
					region->agent_counter--;	
					break;
				}
			}
			
			ScheduleNewEvent(content->sender, now, DESTINATION, &new_event, sizeof(new_event));
			
			break;

		case DESTINATION: 
			agent = (lp_agent_t *) state;
			agent->visited_regions[content->destination] = 1;
                        agent->visited_counter = 0;
			agent->region = content->destination;

			for(i=0;i<get_tot_regions();i++){
				if(agent->visited_regions[i] == 1){
					agent->visited_counter++;
				}
			}
				
			if(check_termination((double) agent->visited_counter)){
				printf("Agente:%d complete! AVC:%d\n",me,agent->visited_counter);
				agent->complete = true;
	                        
				new_event.destination = content->destination;
        	                new_event.visited_regions = NULL;
                	        new_event.sender = me;
				
				if(me + 1 == n_prc_tot)
			 		ScheduleNewEvent(0, now + Expent(DELAY), COMPLETE, &new_event, sizeof(new_event));
				else	
			 		ScheduleNewEvent(me + 1, now + Expent(DELAY), COMPLETE, &new_event, sizeof(new_event));

				break;
			}
			
			new_event.destination = content->destination;
                        new_event.visited_regions = agent->visited_regions;
                        new_event.sender = me;
			ScheduleNewEvent(content->destination, now, ENTER, &new_event, sizeof(new_event));

			ScheduleNewEvent(content->destination, now + Expent(DELAY), EXIT, &new_event, sizeof(new_event));
			break;

		case COMPLETE:
			if(is_agent(me)){
				agent = (lp_agent_t *) state;
				agent->complete = true;
				agent->visited_counter = get_tot_regions();
				new_event.destination = agent->region;
			}
			else{
				region = (lp_region_t *) state;
				region->complete = true;
				new_event.destination = me;
			}

			new_event.visited_regions = NULL;
                        new_event.sender = me;

                        if(me + 1 == n_prc_tot)
				ScheduleNewEvent(0,  now + Expent(DELAY), COMPLETE, &new_event, sizeof(new_event));
			else
				ScheduleNewEvent(me + 1,  now + Expent(DELAY), COMPLETE, &new_event, sizeof(new_event));

			break;
		
        }
}

bool OnGVT(unsigned int me, void *snapshot) {
	double tot_reg, counter, result;
	unsigned int i;
	lp_agent_t *agent;
	lp_region_t *region;
	
	if(is_agent(me)){
		agent = (lp_agent_t *) snapshot;	
		tot_reg = (double)get_tot_regions();
		counter = (double)(agent->visited_counter);
		result = counter/tot_reg;
		printf("Agent[%d] VC:%d \t{",me,agent->visited_counter);
		for(i=0;i<get_tot_regions();i++)
			printf("%d ",agent->visited_regions[i]);
		printf("}\n");
        	if(me == get_tot_regions()){
                	printf("Completed work: %f\%\n", result);
        	}
		
		if(!check_termination(counter) && !agent->complete){
			printf("[ME:%d] Complete:%f flag:%d\n",me,result,agent->complete);
			return false;
		}
		printf("%d complete execution  C:%f F:%d\n",me,result,agent->complete);
	}
	else{ 
		region = (lp_region_t *) snapshot;
		if(!region->complete){
			printf("[ME:%d] flag:%d\n",me,region->complete);
			return false;
		}
		printf("[ME:%d] flag:%d\n",me,region->complete);
	}
	
	return true;
}
