unsigned int get_tot_regions(void){
	unsigned int tot_region,check_value;

	tot_region = n_prc_tot * PERC_REGION;
	check_value = sqrt(tot_region);

	if(check_value * check_value != tot_region) {
                rootsim_error(true, "Hexagonal map wrongly specified!\n");
        }
		
	return tot_region;
}


unsigned int random_region(void){
	return get_tot_region()*Random();
}

unsigned char get_obstacles(void){
	return 0;
}

unsigned int get_region(unsigned int me, unsigned char obstacle,unsigned int){
	unsigned int edge,temp;
	temp = 3 * Random();
	edge = sqrt(tot_region);

	switch(temp){
		case 0:
			if(me>edge)
				return me - edge;
			else if( me == to
			
	}

}
