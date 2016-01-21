#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"

#define DEBUG if(0)

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state) {
        (void)size;
	enter_t *enter_p;
	exit_t *exit_p;
	destination_t *destination_p;
	complete_t *complete_p;
        
	enter_t enter;
	exit_t exit;
	destination_t destination;
	complete_t complete;

        simtime_t timestamp;

	lp_agent_t *agent;
	lp_region_t *region;

	unsigned int i,j;
	
        switch(event) {

                case INIT: // must be ALWAYS implemented
			
                        if(is_agent(me)){
				agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
				printf("AGENT ADD:%p\n",agent);	
				agent->complete = false;

				agent->region = random_region();
				agent->map = ALLOCATE_BITMAP(get_tot_regions());
				BITMAP_BZERO(agent->map,get_tot_regions());

				agent->count = 0;

				SetState(agent);
			}
			else{
				region = (lp_region_t *)malloc(sizeof(lp_region_t));
				
				region->map = ALLOCATE_BITMAP(get_tot_regions());
                                BITMAP_BZERO(region->map,get_tot_regions());

        			region->count = 0;     
        			region->obstacles = get_obstacles();

				SetState(region);	
			}
			
                        timestamp = (simtime_t)(20 * Random());

			if(is_agent(me)){
				bzero(&enter, sizeof(enter));
				bzero(&exit, sizeof(exit));
				BITMAP_SET_BIT(agent->map,agent->region);
				agent->count++;
				
				copy_map(agent->map,DIM_ARRAY,enter.map);
				
				DEBUG printf("%d send ENTER to %d\n",me,agent->region);
				ScheduleNewEvent(agent->region, timestamp, ENTER, &enter, sizeof(enter));
				
				exit.agent = me;		
				
				timestamp += Expent(DELAY);
				DEBUG printf("%d send EXIT to %d\n",me,agent->region);
				
				ScheduleNewEvent(agent->region, timestamp, EXIT, &exit, sizeof(exit));
			}
			else{
				ScheduleNewEvent(me, timestamp, PING, NULL, 0);	
			}

                        break;

                case PING:
		//	DEBUG printf("Send PING\n");
			for(j = 0; j < 100000; j++);
                        ScheduleNewEvent(me, now + Expent(DELAY), PING, NULL, 0);
			break;

		case ENTER:
			enter_p = (enter_t *) content;
			region = (lp_region_t *) state;

			DEBUG printf("Region%d process ENTER of %d\n",me,enter_p->agent);
		
			for(j=0; j<get_tot_regions(); j++){
                        	if(BITMAP_CHECK_BIT(&(enter_p->map),j) && !BITMAP_CHECK_BIT(region->map,j))
					BITMAP_SET_BIT(region->map,j);
                        }

			region->count++;	
			DEBUG printf("End enter Region:%d\n",me);
			
			break;

		case EXIT: 
			bzero(&destination, sizeof(destination));
			exit_p = (exit_t *) content;
			region = (lp_region_t *) state;
			
			destination.region = get_region(me,region->obstacles,exit_p->agent);
			
			copy_map(region->map,DIM_ARRAY,destination.map);

			if(region->count == 1)
				BITMAP_BZERO(region->map,get_tot_regions());	
			
			region->count--;
			
			DEBUG 	printf("%d send DESTINATION to %d\n",me,exit_p->agent);
			ScheduleNewEvent(exit_p->agent, now + Expent(DELAY), DESTINATION, &destination, sizeof(destination));
			
			break;

		case DESTINATION: 
			bzero(&enter, sizeof(enter));
			bzero(&exit, sizeof(exit));
			destination_p = (destination_t *) content;
			agent = (lp_agent_t *) state;
			
			copy_map(agent->map,DIM_ARRAY,destination_p->map);

			BITMAP_SET_BIT(agent->map,destination_p->region);

                        agent->count = 0;
			for(i=0; i<get_tot_regions(); i++){
				if(BITMAP_CHECK_BIT(agent->map,i))
					agent->count++;
			}
				
			if(check_termination(agent)){
				bzero(&complete, sizeof(complete));
				agent->complete = true;
	                        
				complete.agent = me;
				
				if(me + 1 == n_prc_tot){
					printf("%d send COMPLETE to %d add:%p \n",me,get_tot_regions(),agent);
			 		ScheduleNewEvent(get_tot_regions(), now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
				}
				else{	
					printf("%d send COMPLETE to%d add:%p \n",me,me+1,agent);
			 		ScheduleNewEvent(me + 1, now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
				}
				
				//Unnote break command to stop exploration if the termination condiction is true
				break;
			}
			
			timestamp = now + Expent(DELAY);			
			
			copy_map(agent->map,DIM_ARRAY,enter.map);

					
			DEBUG printf("%d send ENTER to %d\n",me,destination_p->region);
			ScheduleNewEvent(destination_p->region, timestamp, ENTER, &enter, sizeof(enter));

			exit.agent = me;
			
			DEBUG printf("%d send EXIT to %d\n",me,destination_p->region);
			ScheduleNewEvent(destination_p->region, timestamp + Expent(DELAY), EXIT, &exit, sizeof(exit));
			break;

		case COMPLETE:
			bzero(&complete, sizeof(complete));
			complete_p = (complete_t *) content;
			if(is_agent(me)){
				agent = (lp_agent_t *) state;
				agent->complete = true;
				agent->count = get_tot_regions();
			}
		
			if(complete_p->agent == me) break;

                        complete.agent = complete_p->agent;

                        if(me + 1 == n_prc_tot){
				printf("%d send COMPLETE to %d\n",me,get_tot_regions());
				ScheduleNewEvent(get_tot_regions(),  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
			}
			else{
				printf("%d send COMPLETE to %d\n",me,me+1);
				ScheduleNewEvent(me + 1,  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
			}

			break;
		
        }
}

bool OnGVT(unsigned int me, void *snapshot) {
	lp_agent_t *agent;
	
	if(is_agent(me)){
		agent = (lp_agent_t *) snapshot;	
		
		DEBUG{	
			unsigned int i;
			printf("Agent[%d]\t",me);
			printf("C:%s \t", agent->complete ? "true" : "false");
			printf("VC:%d \t{",agent->count);
			for(i=0;i<get_tot_regions();i++){
				if(BITMAP_CHECK_BIT(agent->map,i))
					printf("1 ");
				else
					printf("0 ");
			}
			printf("}\n");
		}
		
        	if(me == get_tot_regions())
                	printf("Completed work: %f%%\n", percentage(agent));
        	
		
			
		if(!check_termination(agent)){
			DEBUG	printf("[ME:%d] Complete:%f flag:%d\n",me,percentage(agent),agent->complete);
			return false;
		}
	
		DEBUG printf("%d complete execution  C:%f F:%d\n",me,percentage(agent),agent->complete);
	}
	
	return true;
}
