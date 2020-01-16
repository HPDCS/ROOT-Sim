#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ROOT-Sim.h>

#include "application.h"

#define HOUR			3600
#define DAY			(24 * HOUR)
#define WEEK			(7 * DAY)

#define EARLY_MORNING		8.5 * HOUR
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

static void initialize_channel(channel *c, double sum);

double recompute_ta(double ref_ta, simtime_t time_now)
{

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

double generate_cross_path_gain(void)
{
	double value;
	double variation;

	variation = 10 * Random();
	variation = pow (10.0, (variation / 10));
	value = CROSS_PATH_GAIN * variation;
	return (value);
}

double generate_path_gain(void)
{
	double value;
	double variation;

	variation = 10 * Random();
	variation = pow (10.0, (variation / 10));
	value = PATH_GAIN * variation;
	return (value);
}

void deallocation(lp_state_type *pointer, unsigned ch)
{
	if(!CHECK_CHANNEL(pointer, ch)){
		printf("Unable to deallocate channel is %d\n", ch);
		abort();
		return;
	}
	RESET_CHANNEL(pointer, ch);
}

void fading_recheck(lp_state_type *pointer)
{
	unsigned i;

	for(i = 0; i < channels_per_cell; i++){
		if(!pointer->core_data->channel_state[i/(sizeof(unsigned) * CHAR_BIT)]){
			i += (sizeof(unsigned) * CHAR_BIT) - 1;
			continue;
		}

		if(CHECK_CHANNEL(pointer, i)){
			pointer->channels[i].fading = Expent(1.0);
		}
	}
}

unsigned allocation(lp_state_type *pointer)
{
	unsigned int i;
	channel *c;
	double summ = 0.0;

	for(i = 0; i < channels_per_cell; ++i){
		c = &pointer->channels[i];

		if(!CHECK_CHANNEL(pointer, i)){
			SET_CHANNEL(pointer, i);
			initialize_channel(c, summ);
			return i;
		}

		summ += generate_cross_path_gain() * c->power * c->fading;
	}

	printf("Unable to allocate channel, but the counter says I have %d available channels\n", pointer->channel_counter);
	fflush(stdout);
	abort();
	return UINT_MAX;
}

unsigned reallocate_channels(lp_state_type *pointer)
{
	unsigned int i;
	unsigned int count = 0;
	channel *c;
	double summ = 0.0;

	for(i = 0; i < channels_per_cell; ++i){
		c = &pointer->channels[i];

		if(CHECK_CHANNEL(pointer, i)){
			initialize_channel(c, summ);
			++count;
		}

		summ += generate_cross_path_gain() * c->power * c->fading;
	}
	return count;
}

static void initialize_channel(channel *c, double sum)
{
	if (fabsf(sum) < FLT_EPSILON) {
		// The newly allocated channel receives the minimal power
		c->power = MIN_POWER;
	} else {
		c->fading = Expent(1.0);
		c->power = ((SIR_AIM * sum) / (generate_path_gain() * c->fading));
		if (c->power < MIN_POWER) c->power = MIN_POWER;
		if (c->power > MAX_POWER) c->power = MAX_POWER;
	}
}
