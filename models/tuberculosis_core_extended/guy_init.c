/*
 * guy_init.c
 *
 *  Created on: 04 ott 2018
 *      Author: andrea
 */

#include "guy_init.h"
#include "parameters.h"
#include <math.h>

const double scale_factor = 40.0;

void guy_init(double randomNumber)
{
	init_t total = {.agents_count = {num_healthy / scale_factor, num_infected / scale_factor, num_sick / scale_factor,
			num_treatment / scale_factor, num_treated / scale_factor}};
	init_t to_send = {0};

	const unsigned regions = RegionsCount();
	unsigned remaining = regions, i;
	rootsim_bitmap initialized[bitmap_required_size(regions)];
	bitmap_initialize(initialized, regions);

	while(remaining){
		// we pick a new random region
		i = Random()*regions;
		if(bitmap_check(initialized, i))
			continue;
		// we mark this region as initialized
		bitmap_set(initialized, i);
		// we compute the number of healthy, sick, infected etc etc people who choose this region
		unsigned j = END_STATES;
		while (j--)
		{
			to_send.agents_count[j] = random_binomial(total.agents_count[j], 1.0 / remaining, randomNumber);
			total.agents_count[j] -= to_send.agents_count[j];
		}

		// we send the actual event
		ScheduleNewEvent(i, 0.5, GUY_INIT, &to_send, sizeof(init_t));

		// we processed a region so we decrease the counter
		remaining--;
	}
}

void init_infected(struct guy_t *guy, region_t *region)
{
	// select a random age following the given distribution
	unsigned i = 0;
	double p;
	drand48_r(&(region->random_initialization_buf), &p);

	while(p_infected_age[i] < p) {
		++i;
	}

	guy->birth_day = 0 - RandomRangeCustom(region, 365 * age_groups[i], 365 * age_groups[i + 1] - 1);

	drand48_r(&(region->random_initialization_buf), &p);
	// select random origin
	if(p < p_infected_foreign)
		bitmap_set(guy->flags, f_foreigner);
	else
		bitmap_reset(guy->flags, f_foreigner);

	drand48_r(&(region->random_initialization_buf), &p);
	// select random gender
	if(p >= p_infected_male)
		bitmap_set(guy->flags, f_female);
	else
		bitmap_reset(guy->flags, f_female);

	set_risk_factors(guy, region);

	// set a compatible infection time xxx this is fairly different from the original model
	do {
		guy->infection_day = 0 - RandomRangeCustom(region, 0, 365 * t_infected_max - 1);
	} while(guy->infection_day < guy->birth_day);

}

static void sickened_base_setup(struct guy_t *guy, region_t *region)
{
	// select a random age following the given distribution
	unsigned i = 0;
	double p;
	drand48_r(&(region->random_initialization_buf), &p);
	while(p_sickened_age[i] < p) {
		++i;
	}

	guy->birth_day = 0 - RandomRangeCustom(region, 365 * age_groups[i], 365 * age_groups[i + 1] - 1);

	drand48_r(&(region->random_initialization_buf), &p);
	// select random origin
	if(p < p_sickened_foreign)
		bitmap_set(guy->flags, f_foreigner);
	else
		bitmap_reset(guy->flags, f_foreigner);
	
	drand48_r(&(region->random_initialization_buf), &p);
	// select random gender
	if(p >= p_sickened_male)
		bitmap_set(guy->flags, f_female);
	else
		bitmap_reset(guy->flags, f_female);

	drand48_r(&(region->random_initialization_buf), &p);
	// set smear flag
	if(p < p_smear)
		bitmap_set(guy->flags, f_smear);
	else
		bitmap_reset(guy->flags, f_smear);
}

void init_sick(struct guy_t *guy, region_t *region)
{
	sickened_base_setup(guy, region);

	define_diagnose(guy, region->now);
	// we suppose this guy has already been sick for some time
	// (notice that this is equivalent to what is done in the original model
	if(guy->treatment_day) {
		guy->treatment_day -= RandomRangeCustom(region, 0, guy->treatment_day - 1);
	}
}

void init_treatment(struct guy_t *guy, region_t *region)
{
	sickened_base_setup(guy, region);
	// we suppose this guy has already been under treatment for some time
	// (notice that this is equivalent to what is done in the original model)
	guy->treatment_day = 0 - RandomRangeCustom(region, 0, t_treatment_max - 1);//RandomRange(0, t_treatment_max - 1);
}

void init_treated(struct guy_t *guy, region_t *region)
{
	sickened_base_setup(guy, region);
	// we suppose this guy has already been under treatment for some time
	// (notice that this is equivalent to what is done in the original model)
	double p;
	drand48_r(&(region->random_initialization_buf), &p);
	guy->treatment_day = p < p_abandon ? 0 - RandomRangeCustom(region, t_treatment_min, t_treatment_max - 1) : 0 - (int) t_treatment_max;
	// compute relapse probability
	compute_relapse_p(guy, 0.0);

}

struct guy_t *init_guy(region_t *region, enum agent_state state)
{
	struct guy_t *guy;

	if(!(guy = malloc(sizeof(*guy))))
		abort();
	memset(guy, 0, sizeof(*guy));

	unsigned long long k1 = (unsigned long long)region->me;
	unsigned long long k2 = region->counter++;

	guy->id = (unsigned long long)(((k1 + k2) * (k1 + k2 + 1) / 2) + k2);

	guy->state = state;
	// add the guy to the corresponding list
	guy_add_head(&(region->agents[state]), guy);
	region->agents_count[state]++;

	guy_mark_by_state(guy);
	
	return guy;
}

void guy_on_init(init_t *init_data, region_t *region)
{
	struct guy_t *guy;

	enum agent_state j = END_STATES;
	while (j--)
	{
		unsigned i = init_data->agents_count[j];
		
		while (i--)
		{
			guy = init_guy(region, j);
			switch (j)
			{
			case HEALTHY:
				break;
			case INFECTED:
				init_infected(guy, region);
				break;
			case TREATED:
				init_treated(guy, region);
				break;
			case TREATMENT:
				init_treatment(guy, region);
				break;
			case SICK:
				init_sick(guy, region);
				break;
			}
		}
	}
}

int RandomRangeCustom(region_t *region, int min, int max) {
	double ret;

	ret = drand48_r(&(region->random_initialization_buf), &ret);

	return (int) floor(ret  * (max - min + 1)) + min;
}

