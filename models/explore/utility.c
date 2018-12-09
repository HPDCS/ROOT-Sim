#include <ROOT-Sim.h>
#include <math.h>

#include "application.h"
unsigned int get_tot_regions(void){
	unsigned int check_value = sqrt(TOT_REG);

	if(check_value * check_value != TOT_REG) {
		   printf("Hexagonal map wrongly specified!\n");
	}
		
	return TOT_REG;
}

unsigned int get_tot_agents(void){
	return n_prc_tot - get_tot_regions();
}

int is_agent(unsigned int me){

	if(me < get_tot_regions()){
		return 0;
	}
	return 1;
}


unsigned int random_region(void){
	return get_tot_regions()*Random();
}

unsigned char get_obstacles(void){
	return 0;
}

unsigned int get_region(unsigned int me, unsigned int obstacle,unsigned int agent){
	(void)obstacle;
	(void)agent;
	unsigned int edge,temp,tot_region;
	double random;
	tot_region = get_tot_regions();
	edge = sqrt(tot_region);

	random = Random();

	// CASE corner up-left
	if(me == 0){
		temp = 2 * random;
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1:	// go down
				return me + edge;	
		}
	}
	// CASE corner up-rigth
	else if(me == edge - 1){
		temp = 2 * random;
		switch(temp){
			case 0: // go left
				return me - 1;
			case 1: // go down
				return me + edge;
		}
	}
	// CASE corner down-rigth
	else if(me == tot_region - 1){
		temp = 2 * random;
		switch(temp){
			case 0: // go left
				return me - 1;
			case 1: // go up
				return me - edge;
		}
	}
	// CASE corner down-left
	else if(me == tot_region - edge){
		temp = 2 * random;
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1: // go up
				return me - edge;
		}
	}
	// CASE first row
	else if(me <  edge){
		temp = 3 * random;
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1: // go down
				return me + edge;
			case 2:
				// go left
				return me - 1;
		}
	}
	// CASE last row
	else if(me > tot_region - edge && me <  tot_region - 1){
		temp = 3 * random;
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1: // go up
				return me - edge;
			case 2:
				// go left
				return me - 1;
		}
	}
	// CASE first column
	else if(me % edge == 0){
		temp = 3 * random;
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1: // go down
				return me + edge;
			case 2:
				// go up
				return me - edge;
		}
	}
	// CASE last column
	else if((me+1) % edge == 0){
		temp = 3 * random;
		switch(temp){
			case 0: // go left
				return me - 1;
			case 1: // go down
				return me + edge;
			case 2:
				// go up
				return me - edge;
		}
	}
	// Normal case
	else{
		temp = 4 * random;
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1: // go down
				return me + edge;
			case 2:
				// go up
				return me - edge;
			case 3:
				// go left
				return me - 1;
		}
	}


	return me;
}

bool check_termination(lp_agent_t *agent){
	double regions = (double)agent->count;
	double tot_region = get_tot_regions();
	double result = regions/tot_region;
	if(result >= VISITED || agent->complete){
		printf("agent %d has visited all regions!!!!\n", agent->id);	
		return true;
	}
	
	printf("agent %d has visited %d ( VISITED %f) regions and is %d\n", agent->id, agent->count, VISITED, agent->complete);	
	return false;
}

double percentage(lp_agent_t *agent){
	double regions = (double)agent->count;
	double tot_region = get_tot_regions();
	double result = (regions/tot_region)*100;

	return result;
}

#ifndef ECS_TEST
void copy_map(unsigned char *pointer, int n, unsigned char (vector)[n]){
	unsigned int i;

	for(i=0; i<get_tot_regions(); i++){
		if(!BITMAP_CHECK_BIT(pointer,i) && BITMAP_CHECK_BIT(vector,i))
			BITMAP_SET_BIT(pointer,i);
		else if(BITMAP_CHECK_BIT(pointer,i) && !BITMAP_CHECK_BIT(vector,i))
			BITMAP_SET_BIT(vector,i);
	}
}
#else
void send_updated_info(lp_agent_t *agent){	
	unsigned char *group_map;
	if(!BITMAP_CHECK_BIT(agent->map, agent->region)){
		BITMAP_SET_BIT(agent->map,agent->region);
		agent->count++;
		agent->pages[agent->region] = malloc(4096);
	}
	//agent->count = 0;
	unsigned int i,j;
	for(i=0; i<get_tot_agents(); i++){
		if(agent->group[i] != NULL){
			group_map = agent->group[i];
			for(j=0;j<get_tot_regions();j++){
				if(BITMAP_CHECK_BIT(agent->map,j) && !BITMAP_CHECK_BIT(group_map,j))
					BITMAP_SET_BIT(group_map,j);
				if(!BITMAP_CHECK_BIT(agent->map,j) && BITMAP_CHECK_BIT(group_map,j)){
					agent->count++; //TODO check
					BITMAP_SET_BIT(agent->map,j);
				}
			}
		}
	}
	/*for(j=0;j<get_tot_regions();j++){
		if(BITMAP_CHECK_BIT(agent->map,j)){
			agent->count++;
		}
	}*/
} 
#endif 
