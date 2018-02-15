#include <ROOT-Sim.h>
#include "application.h"


void initialize_obstacles(obstacles_t **obstacles) {
	unsigned int i;
	
	SetupObstacles(&obstacles);
	for(i = 0; i < OBSTACLES_PERCENT * n_prc_tot; i++) {
		AddObstacle(ostacles, FindReceiver(TOPOLOGY_MESH));
	}
}

int add_agent(lp_state_type *state, agent_t *agent) {
	int i;
  	int index;
	double summ;

	channel *c, *ch;

	index = -1;
	for(i = 0; i < pointer->channels_per_cell; i++){
		if(!CHECK_CHANNEL(pointer,i)){
			index = i;
			break;
		}
	}

	if(index != -1){

		SET_CHANNEL(pointer,index);

		c = (channel*)malloc(sizeof(channel));
		if(c == NULL){
			printf("malloc error: unable to allocate channel!\n");
			exit(-1);void deallocation(unsigned int me, lp_state_type *pointer, int ch, simtime_t lvt) {
	channel *c;

	c = pointer->channels;
	while(c != NULL){
		if(c->channel_id == ch)
			break;
		c = c->prev;
	}
	if(c != NULL){
		if(c == pointer->channels){
			pointer->channels = c->prev;
			if(pointer->channels)
				pointer->channels->next = NULL;
		}
		else{
			if(c->next != NULL)
				c->next->prev = c->prev;
			if(c->prev != NULL)
				c->prev->next = c->next;
		}
		RESET_CHANNEL(pointer, ch);
		free(c->sir_data);

		free(c);
	} else {
		printf("(%d) Unable to deallocate on %p, channel is %d at time %f\n", me, c, ch, lvt);
		abort();
	}
	return;
}
		}

		c->next = NULL;
		c->prev = pointer->channels;
		c->channel_id = index;
		c->sir_data = (sir_data_per_cell*)malloc(sizeof(sir_data_per_cell));
		if(c->sir_data == NULL){
			printf("malloc error: unable to allocate SIR data!\n");
			exit(-1);
		}

		if(pointer->channels != NULL)
			pointer->channels->next = c;
		pointer->channels = c;

		summ = 0.0;

	//	if (pointer->check_fading) {
	//force this

		if(1){
			ch = pointer->channels->prev;

			while(ch != NULL){
				ch->sir_data->fading = Expent(1.0);

				summ += generate_cross_path_gain() *  ch->sir_data->power * ch->sir_data->fading ;
				ch = ch->prev;
			}
		}

		if (summ == 0.0) {
			// The newly allocated channel receives the minimal power
			c->sir_data->power = MIN_POWER;
		} else {
		  	c->sir_data->fading = Expent(1.0);
			c->sir_data->power = ((SIR_AIM * summ) / (generate_path_gain() * c->sir_data->fading));
			if (c->sir_data->power < MIN_POWER) c->sir_data->power = MIN_POWER;
			if (c->sir_data->power > MAX_POWER) c->sir_data->power = MAX_POWER;
		}

	} else {
		printf("Unable to allocate channel, but the counter says I have %d available channels\n", pointer->channel_counter);
		abort();
		fflush(stdout);
	}

        return index;
}



agent_t *remove_agent(lp_state_type *state, unsigned long long uuid) {
	channel *c;

	c = pointer->channels;
	while(c != NULL){
		if(c->channel_id == ch)
			break;
		c = c->prev;
	}
	if(c != NULL){
		if(c == pointer->channels){
			pointer->channels = c->prev;
			if(pointer->channels)
				pointer->channels->next = NULL;
		}
		else{
			if(c->next != NULL)
				c->next->prev = c->prev;
			if(c->prev != NULL)
				c->prev->next = c->next;
		}
		RESET_CHANNEL(pointer, ch);
		free(c->sir_data);

		free(c);
	} else {
		printf("(%d) Unable to deallocate on %p, channel is %d at time %f\n", me, c, ch, lvt);
		abort();
	}
	return;
}

/**
 * This (helper) function schedules a new UPDATE_NEIGHBOURS
 * event towards the cells that represent my neighbours
 *
 * @author Matteo Principe
 * @date 8 feb 2018
 *
 * @param me The id of the cell who is calling the function
 * @param now The current time of the cell
 * @param present The number of bugs that are inside the cell
*/

void send_update_neighbours(simtime_t now, event_content_type *new_event_content){
	int receiver;
	int i;

	// TODO: send state->num_agents

	for(i = 0; i < 4; i++){
		receiver = GetReceiver(TOPOLOGY_TORUS,i);
		if(receiver >= (int) n_prc_tot || receiver < 0){
			printf("%s:%d Got receiver out of bounds!\n",__FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}	
		ScheduleNewEvent(receiver, now + TIME_STEP/100000, UPDATE_NEIGHBOURS, new_event_content, sizeof(new_event_content));
	}

}

