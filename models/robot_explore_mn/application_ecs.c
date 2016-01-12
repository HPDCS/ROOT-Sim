#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"
#include "utility.c"

#define DEBUG if(0)

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state) {
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

	unsigned char *old_agent;
	unsigned int i,j;
	
        switch(event) {

                case INIT: // must be ALWAYS implemented
			
                        if(is_agent(me)){
				agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
				
				agent->complete = false;
				agent->region = random_region();
			//	agent->map = (unsigned char *)calloc(get_tot_regions(),sizeof(unsigned char));
				agent->map = ALLOCATE_BITMAP(get_tot_regions());
				BITMAP_BZERO(agent->map,get_tot_regions());
        			agent->count = 0;

				SetState(agent);
			}
			else{
				region = (lp_region_t *)malloc(sizeof(lp_region_t));
				#ifdef ECS_TEST	
				region->guests = calloc(get_tot_agents(),sizeof(unsigned char *));
				#else
				region->map = ALLOCATE_BITMAP(get_tot_regions());
                                BITMAP_BZERO(region->map,get_tot_regions());
				#endif
        			region->count = 0;     
        			region->obstacles = get_obstacles();

				SetState(region);	
			}
			
                        timestamp = (simtime_t)(20 * Random());

			if(is_agent(me)){
				BITMAP_SET_BIT(agent->map,agent->region);
				agent->count++;
				
				#ifdef ECS_TEST
                        	enter.map = agent->map;
				#else
				enter.map = ALLOCATE_BITMAP(get_tot_regions());
				memcpy(enter.map,agent->map,get_tot_regions());
				#endif
				
				ScheduleNewEvent(agent->region, timestamp, ENTER, &enter, sizeof(enter));
				
				exit.agent = me;		
                        	#ifdef ECS_TEST
				exit.map = agent->map;
				#endif
				
				timestamp += Expent(DELAY);
                                ScheduleNewEvent(agent->region, timestamp, EXIT, &exit, sizeof(exit));
			}
			else{
				ScheduleNewEvent(me, timestamp, PING, NULL, 0);	
			}

                        break;

                case PING:
                        ScheduleNewEvent(me, now + Expent(DELAY), PING, NULL, 0);
			break;

		case ENTER:
			enter_p = (enter_t *) content;
			region = (lp_region_t *) state;
		
			#ifdef ECS_TEST	
			region->guests[region->count] = enter_p->map;
			
			for(i=0; i<region->count; i++){
				old_agent = region->guests[i];

				for(j=0; j<get_tot_regions(); j++){
					if(!BITMAP_CHECK_BIT(enter_p->map,j) && BITMAP_CHECK_BIT(old_agent,j))
						BITMAP_SET_BIT(enter_p->map,j);
					else if(BITMAP_CHECK_BIT(enter_p->map,j) && !BITMAP_CHECK_BIT(old_agent,j))
						BITMAP_SET_BIT(old_agent,j);
				}
			}
			#else
			for(j=0; j<get_tot_regions(); j++){
                        	if(BITMAP_CHECK_BIT(enter_p->map,j) && !BITMAP_CHECK_BIT(region->map,j))
					BITMAP_SET_BIT(region->map,j);
                        }
			#endif

			region->count++;	
			
			break;

		case EXIT: 
			exit_p = (exit_t *) content;
			region = (lp_region_t *) state;
			
			destination.region = get_region(me,region->obstacles,exit_p->agent);
			
			#ifdef ECS_TEST
			for(i=0;i<region->count; i++){
				if(region->guests[i] == exit_p->map){
					if(i!=(region->count-1) && region->count >= 1)
						region->guests[i] = region->guests[region->count-1];
					
					region->count--;	
					break;
				}
			}
			#else
			destination.map = ALLOCATE_BITMAP(get_tot_regions());
                        memcpy(destination.map,region->map,get_tot_regions());

			if(region->count == 1)
				BITMAP_BZERO(region->map,get_tot_regions());	
			
			region->count--;
			#endif
			
			ScheduleNewEvent(exit_p->agent, now + Expent(DELAY), DESTINATION, &destination, sizeof(destination));
			
			break;

		case DESTINATION: 
			destination_p = (destination_t *) content;
			agent = (lp_agent_t *) state;
			
			#ifdef ECS_TEST
			agent->region = destination_p->region;
			#else	
			memcpy(agent->map,destination_p->map,get_tot_regions());
			#endif
			BITMAP_SET_BIT(agent->map,destination_p->region);

                        agent->count = 0;
			for(i=0; i<get_tot_regions(); i++){
				if(BITMAP_CHECK_BIT(agent->map,i))
					agent->count++;
			}
				
			if(check_termination(agent)){
				agent->complete = true;
	                        
				complete.agent = me;
				
				if(me + 1 == n_prc_tot)
			 		ScheduleNewEvent(get_tot_regions(), now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
				else	
			 		ScheduleNewEvent(me + 1, now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));

				break;
			}
			
			timestamp = now + Expent(DELAY);			
			
			#ifdef ECS_TEST
			enter.map = agent->map;
			#else
			enter.map = ALLOCATE_BITMAP(get_tot_regions());
			memcpy(enter.map,agent->map,get_tot_regions());
			#endif

			ScheduleNewEvent(destination_p->region, timestamp, ENTER, &enter, sizeof(enter));

			exit.agent = me;
			#ifdef ECS_TEST
                        exit.map = agent->map;
                        #endif
			
			ScheduleNewEvent(destination_p->region, timestamp + Expent(DELAY), EXIT, &exit, sizeof(exit));
			break;

		case COMPLETE:
			complete_p = (complete_t *) content;
			if(is_agent(me)){
				agent = (lp_agent_t *) state;
				agent->complete = true;
				agent->count = get_tot_regions();
			}

                        complete.agent = me;

                        if(me + 1 == n_prc_tot)
				ScheduleNewEvent(get_tot_regions(),  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
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
