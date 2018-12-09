
#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"

#define DEBUG if(0)
#define DEBUG2 if(1)
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

	unsigned char **new_group;
	unsigned char **old_group;
	void **new_group_pages;
	void **old_group_pages;
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
		
				if(is_agent(me)){
					agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
					DEBUG printf("AGENT ADD: %d \n", me);
					fflush(stdout);
					agent->complete = false;
					
					agent->id = me;
					agent->region = random_region();
					
					agent->map = ALLOCATE_BITMAP(get_tot_regions());
					BITMAP_BZERO(agent->map,get_tot_regions());

					agent->group = calloc(get_tot_agents(),sizeof(unsigned char *));
					agent->group[me-get_tot_regions()] = agent->map;
					agent->pages = malloc(TOT_REG * sizeof(void *));
					agent->group_pages = malloc(TOT_REG * sizeof(void *));
					agent->count = 0;

					SetState(agent);
				} else {
					printf("ADD REGION %d\n", me);
					region = (lp_region_t *)malloc(sizeof(lp_region_t));
					region->guests = calloc(get_tot_agents(),sizeof(lp_agent_t *));
					region->group_infos = malloc(TOT_REG * sizeof(void *));
					region->group_infos[me] = malloc(4096);
					region->count = 0;     
					region->obstacles = get_obstacles();

					SetState(region);	
				}
		
				timestamp = (simtime_t)(20 * Random());

				if(is_agent(me)) {
					BITMAP_SET_BIT(agent->map,agent->region);
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

			case PING:
				for(i = 0; i < 1000000; i++);
				ScheduleNewEvent(me, now + (simtime_t)Expent(DELAY_PING), PING, NULL, 0);
				break;

			case ENTER:
				enter_p = (enter_t *) content;
				region = (lp_region_t *) state;
				lp_agent_t *curr_agent;
				curr_agent = enter_p->agent;
				DEBUG printf("Region %d process ENTER of %d\n",me,enter_p->agent->id);
				
				curr_agent->pages[curr_agent->region] = region->group_infos[me];
				for(i = 0; i < TOT_REG; i++){
					if(i!= me && curr_agent->pages[i] == NULL && region->group_infos[i] != NULL){
						curr_agent->pages[i] = region->group_infos[i];
					}else if(i!= me && curr_agent->pages[i] != NULL && region->group_infos[i] == NULL)
						region->group_infos[i] = curr_agent->pages[i];
				}
				
				region->guests[region->count] = enter_p->agent;
				new_group = enter_p->agent->group;	
				for(i=0;i<region->count;i++){
					old_group = region->guests[i]->group;
					for(j=0;j<get_tot_agents();j++){
						if(new_group[j] == NULL && old_group[j] != NULL)
							new_group[j] = old_group[j];
						else if(new_group[j] != NULL && old_group[j] == NULL)
							old_group[j] = new_group[j];
					}
				}

				region->count++;	
				DEBUG	printf("End enter Region:%d\n",me);
				
				break;

			case EXIT: 
				exit_p = (exit_t *) content;
				region = (lp_region_t *) state;
				
				destination.region = get_region(me,region->obstacles,exit_p->agent);
				for(i=0;i<region->count; i++){
					if(region->guests[i]->id == exit_p->agent){
						if(i!=(region->count-1) && region->count >= 1)
							region->guests[i] = region->guests[region->count-1];
						region->guests[region->count-1] = NULL;
						region->count--;	
						break;
					}
				}
				
				DEBUG2 	printf("%d send DESTINATION to %d at time %f\n",me,exit_p->agent,  now + (simtime_t)Expent(DELAY));
				ScheduleNewEvent(exit_p->agent, now + (simtime_t)Expent(DELAY), DESTINATION, &destination, sizeof(destination));
				
				break;

			case DESTINATION: 
				destination_p = (destination_t *) content;
				agent = (lp_agent_t *) state;
				agent->region = destination_p->region;
				send_updated_info(agent);			
				
				if(check_termination(agent)){
					
					agent->complete = true;
								
					complete.agent = me;
					
					timestamp = now + (simtime_t)Expent(DELAY);
					
					//printf("LP %d is agent? %d\n", agent->id, is_agent(agent->id));
					//break;
					if(me + 1 == n_prc_tot){
						DEBUG2 printf("%d send COMPLETE from DESTINATION to %d add at %f: %p \n",me,get_tot_regions(),timestamp,agent);
						ScheduleNewEvent(get_tot_regions(), timestamp, COMPLETE, &complete, sizeof(complete));
					}
					else{	
						DEBUG2 printf("%d send COMPLETE from DESTINATION to %d add at %f: %p\n",me, me+1, timestamp,agent);
						ScheduleNewEvent(me + 1, timestamp, COMPLETE, &complete, sizeof(complete));
					}
					
					//Unnote break command to stop exploration if the termination condiction is true
				
	//				printf("%d send COMPLETE to %d add:%p \n",me,get_tot_regions(),agent);
					//break;
				}
				
				timestamp = now + (simtime_t)Expent(DELAY);			
				
				enter.agent = agent;

				DEBUG2 printf("%d send ENTER to %d at time %f\n",me,destination_p->region, timestamp);
				ScheduleNewEvent(destination_p->region, timestamp, ENTER, &enter, sizeof(enter));

				exit.agent = me;
				
				DEBUG2 printf("%d send EXIT to %d at time %f\n",me,destination_p->region,  timestamp + (simtime_t)Expent(DELAY));
				ScheduleNewEvent(destination_p->region, timestamp + (simtime_t)Expent(DELAY), EXIT, &exit, sizeof(exit));
				break;

			case COMPLETE:
				/*DEBUG2*/ printf("%d executes COMPLETE at %f\n", me, now);
				complete_p = (complete_t *) content;
				if(is_agent(me)){
					agent = (lp_agent_t *) state;
					agent->complete = true;
					agent->count = get_tot_regions();
				}
				
				if(complete_p->agent == me) break;

				complete.agent = complete_p->agent;
				timestamp = now + (simtime_t)Expent(DELAY);
				if(me + 1 == n_prc_tot){
					DEBUG		printf("%d send COMPLETE from COMPLETE to %d at %f\n",me,get_tot_regions() , timestamp);
					ScheduleNewEvent(get_tot_regions(),  timestamp, COMPLETE, &complete, sizeof(complete));
				}
				else{
					DEBUG		printf("%d send COMPLETE from COMPLETE to %d at %f\n",me,me+1, timestamp);
					ScheduleNewEvent(me + 1,  timestamp, COMPLETE, &complete, sizeof(complete));
				}

			break;
			
		}
}

bool OnGVT(unsigned int me, void *snapshot) {
	lp_agent_t *agent;
	
	if(is_agent(me)){
		agent = (lp_agent_t *) snapshot;	
		
		//DEBUG{	
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
		//}
		
		if(me == get_tot_regions())
			printf("Completed work: %f%%\n", percentage(agent));	
		if(!check_termination(agent)){
			/*DEBUG*/	printf("[ME:%d] Complete:%f flag:%d\n",me,percentage(agent),agent->complete);
			return false;
		}
	
		DEBUG printf("%d complete execution  C:%f F:%d\n",me,percentage(agent),agent->complete);
	}
	
	return true;
}
