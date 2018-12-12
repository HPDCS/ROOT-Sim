
#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"

#define DEBUG if(0)
#define DEBUG2 if(0)

lp_agent_t *agents[MAX_AGENTS];

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

	lp_agent_t *agent, *other_agent;
	lp_region_t *region;

	unsigned int i,j;

	bzero(&enter, sizeof(enter));
	bzero(&exit, sizeof(exit));
	bzero(&destination, sizeof(destination));
	bzero(&complete, sizeof(complete));

	if(is_agent(me) && event != INIT)
		((lp_agent_t *)state)->lvt = now;
	
	switch(event) {

			case INIT: // must be ALWAYS implemented
			
				if(n_prc_tot < TOT_REG + 1) {
					printf("You must use at least %d LPs\n", TOT_REG + 1);
					abort();
				}

				if(get_tot_agents() >= MAX_AGENTS) {
					printf("You must use at most %d LPs or change MAX_AGENTS\n", TOT_REG + MAX_AGENTS);
					abort();
				}
		
				if(is_agent(me)){
					agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
					DEBUG printf("AGENT ADD: %d \n", me);
					fflush(stdout);
					agent->complete = false;
					
					agent->id = me - get_tot_regions();
					agent->region = random_region();
					
					agent->map = ALLOCATE_BITMAP(get_tot_regions());
					BITMAP_BZERO(agent->map,get_tot_regions());

					agent->exploration = malloc(get_tot_regions() * sizeof(measure_t));

					agent->count = 0;

					// Back pointer
					agents[agent->id] = agent;

					SetState(agent);
				} else {
					DEBUG printf("ADD REGION %d\n", me);
					region = (lp_region_t *)malloc(sizeof(lp_region_t));
					region->id = me;
					region->count = 0;     
					region->agents = ALLOCATE_BITMAP(get_tot_agents());
					generate_random_data((unsigned char *)&region->data, sizeof(measure_t));

					SetState(region);	
				}
		
				timestamp = (simtime_t)(20 * Random());

				if(is_agent(me)) {
					BITMAP_SET_BIT(agent->map, agent->region);
					agent->count++;
					
					enter.agent = agent;
					
					DEBUG printf("%d send ENTER to %d\n",me,agent->region);
					ScheduleNewEvent(agent->region, timestamp, ENTER, &enter, sizeof(enter));
					
					exit.agent = me;		
					
					timestamp += (simtime_t)Expent(DELAY);
					DEBUG printf("%d send EXIT to %d\n",me,agent->region);
					ScheduleNewEvent(agent->region, timestamp, EXIT, &exit, sizeof(exit));
				} else {
					ScheduleNewEvent(me, timestamp + (simtime_t)Expent(DELAY_PING), PING, NULL, 0);	
				}

				break;

			// In newer versions of ROOT-Sim this keepalive is no longer needed
			case PING:
				for(i = 0; i < 1000000; i++);
				ScheduleNewEvent(me, now + (simtime_t)Expent(DELAY_PING), PING, NULL, 0);
				break;

			case ENTER:
				enter_p = (enter_t *) content;
				region = (lp_region_t *) state;
				agent = enter_p->agent;
				
				DEBUG printf("Region %d process ENTER of %d\n",me,enter_p->agent->id);

				// Register me in this region
				BITMAP_SET_BIT(region->agents, agent->id);
				region->count++;

				// Measure the data in this region if it is a new region
				if(BITMAP_CHECK_BIT(agent->map, region->id) == 0) {
					agent->exploration[region->id] = region->data;
					BITMAP_SET_BIT(agent->map, region->id);
					agent->count++;
				}

				// Check if other agents are around, and in case exchange data
				for(i = 0; i < get_tot_agents(); i++) {
					if(i != agent->id && BITMAP_CHECK_BIT(region->agents, i)) {
						other_agent = agents[i];

						for(j = 0; j < get_tot_agents(); j++) {
							if(BITMAP_CHECK_BIT(agent->map, j) == 0 && BITMAP_CHECK_BIT(agent->map, j) == 1) {
								// Copy from other to me
								BITMAP_SET_BIT(agent->map, j);
								agent->exploration[j] = other_agent->exploration[j];
								
							} else if(BITMAP_CHECK_BIT(agent->map, j) == 1 && BITMAP_CHECK_BIT(agent->map, j) == 0) {
								// Copy from me to other
								BITMAP_SET_BIT(other_agent->map, j);
								other_agent->exploration[j] = agent->exploration[j];
							}
						}
					}
				}
				
				DEBUG	printf("End enter Region:%d\n",me);
				
				break;

			case EXIT: 
				exit_p = (exit_t *) content;
				region = (lp_region_t *) state;
				
				destination.region = get_region(me);
				region->count--;
				
				DEBUG2 	printf("%d send DESTINATION to %d at time %f\n",me,exit_p->agent,  now + (simtime_t)Expent(DELAY));
				ScheduleNewEvent(exit_p->agent, now + (simtime_t)Expent(DELAY), DESTINATION, &destination, sizeof(destination));
				
				break;

			case DESTINATION: 
				destination_p = (destination_t *)content;
				agent = (lp_agent_t *) state;
				agent->region = destination_p->region;
	
				if(check_termination(agent))
					agent->complete = true;
				
				timestamp = now + (simtime_t)Expent(DELAY);			
				
				enter.agent = agent;

				DEBUG2 printf("%d send ENTER to %d at time %f\n",me,destination_p->region, timestamp);
				ScheduleNewEvent(destination_p->region, timestamp, ENTER, &enter, sizeof(enter));

				exit.agent = me;
				
				DEBUG2 printf("%d send EXIT to %d at time %f\n",me,destination_p->region,  timestamp + (simtime_t)Expent(DELAY));
				ScheduleNewEvent(destination_p->region, timestamp + (simtime_t)Expent(DELAY), EXIT, &exit, sizeof(exit));
				break;

			break;
			
		}
}

bool OnGVT(unsigned int me, void *snapshot) {
	lp_agent_t *agent;
	
	if(is_agent(me)){
		agent = (lp_agent_t *) snapshot;	
		
		DEBUG {	
			printf("Agent[%d]\t",me);
			printf("LVT:%f\t",agent->lvt);
			printf("ADD:%p \t", agent);
			printf("C:%s \t", agent->complete ? "true" : "false");
			printf("VC:%d \n{",agent->count);
			/*for(i=0;i<get_tot_regions();i++){
				if(BITMAP_CHECK_BIT(agent->map,i))
					printf("1 ");
				else
					printf("0 ");
			}
			printf("}\n");*/
		}
		
		if(is_agent(me))
			DEBUG2 printf("Completed work: %f%%\n", percentage(agent));	

		if(!check_termination(agent)){
			DEBUG printf("[ME:%d] Complete:%f flag:%d\n",me,percentage(agent),agent->complete);
			return false;
		}
	
		DEBUG printf("%d complete execution  C:%f F:%d\n",me,percentage(agent),agent->complete);
	}
	
	return true;
}
