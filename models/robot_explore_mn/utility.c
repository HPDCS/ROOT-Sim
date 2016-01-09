#include <ROOT-Sim.h>
#include <math.h>

unsigned int get_tot_regions(void){
	unsigned int tot_region,check_value;

	tot_region = n_prc_tot * PERC_REGION;
	check_value = sqrt(tot_region);

	if(check_value * check_value != tot_region) {
               printf("Hexagonal map wrongly specified!\n");
        }
		
	return tot_region;
}

unsigned int get_tot_agents(void){
	return n_prc_tot - get_tot_regions();
}

bool is_agent(unsigned int me){

	if(me < get_tot_regions()){
		return false;
	}
	return true;
}


unsigned int random_region(void){
	return get_tot_regions()*Random();
}

unsigned char get_obstacles(void){
	return 0;
}

unsigned int get_region(unsigned int me, unsigned int obstacle,unsigned int agent){
	unsigned int edge,temp,tot_region;
	double random;
	tot_region = get_tot_regions();
	edge = sqrt(tot_region);
	
	// CASE corner up-left
	if(me == 0){
		//random = Random();
		//temp = 1 * random;
		temp = 1 * Random();
		switch(temp){
			case 0: // go rigth
				return me + 1;
			case 1:	// go down
				return me + edge;	
		}
	}
	// CASE corner up-rigth
	else if(me == edge - 1){
		temp = 1 * Random();
                switch(temp){
                        case 0: // go left
                                return me - 1;
                        case 1: // go down
                                return me + edge;
                }
	}
	// CASE corner down-rigth
	else if(me == tot_region - 1){
                temp = 1 * Random();
                switch(temp){
                        case 0: // go left
                                return me - 1;
                        case 1: // go up
                                return me - edge;
                }
        }
	// CASE corner down-left
	else if(me == tot_region - edge){
                temp = 1 * Random();
                switch(temp){
                        case 0: // go rigth
                                return me + 1;
                        case 1: // go up
                                return me - edge;
                }
        }
	// CASE first row
	else if(me <  edge){
                temp = 2 * Random();
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
                temp = 2 * Random();
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
                temp = 2 * Random();
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
                temp = 2 * Random();
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
                temp = 3 * Random();
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
