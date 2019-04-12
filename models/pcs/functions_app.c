#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ROOT-Sim.h>

#include "application.h"

#define HOUR			3600
#define DAY				(24 * HOUR)
#define WEEK			(7 * DAY)

#define EARLY_MORNING	8.5 * HOUR
#define MORNING			13 * HOUR
#define LUNCH			15 * HOUR
#define AFTERNOON		19 * HOUR
#define EVENING			21 * HOUR


#define EARLY_MORNING_FACTOR	4
#define MORNING_FACTOR		0.8
#define LUNCH_FACTOR		2.5
#define AFTERNOON_FACTOR	2
#define EVENING_FACTOR		2.2
#define NIGHT_FACTOR		4.5
#define WEEKEND_FACTOR		5



double recompute_ta(double ref_ta, simtime_t time_now) {

	int now = (int)time_now;
	now %= WEEK;

	if (now > 5 * DAY)
		return ref_ta * WEEKEND_FACTOR;

	now %= DAY;

	if (now < EARLY_MORNING)
		return ref_ta * EARLY_MORNING_FACTOR;
	if (now < MORNING)
		return ref_ta * MORNING_FACTOR;
	if (now < LUNCH)
		return ref_ta * LUNCH_FACTOR;
	if (now < AFTERNOON)
		return ref_ta * AFTERNOON_FACTOR;
	if (now < EVENING)
		return ref_ta * EVENING_FACTOR;

	return ref_ta * NIGHT_FACTOR;
}


double generate_cross_path_gain(void) {
	double value;
	double variation;

	variation = 10 * Random();
	variation = pow ((double)10.0 , (variation / 10));
	value = CROSS_PATH_GAIN * variation;
	return (value);
}

double generate_path_gain(void) {
	double value;
	double variation;

	variation = 10 * Random();
	variation = pow ((double)10.0 , (variation / 10));
	value = PATH_GAIN * variation;
	return (value);
}

void deallocation(unsigned int me, lp_state_type *pointer, int ch, simtime_t lvt) {
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

void fading_recheck(lp_state_type *pointer) {
	channel *ch;

	ch = pointer->channels;

        while(ch != NULL){
        	ch->sir_data->fading = Expent(1.0);
                ch = ch->prev;
        }
}

int allocation(lp_state_type *pointer) {

	unsigned int i;
  	int index;
	double summ;

	channel *c, *ch;

	index = -1;
	for(i = 0; i < channels_per_cell; i++){
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
			exit(-1);
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

		if (fabsf(summ) < FLT_EPSILON) {
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

