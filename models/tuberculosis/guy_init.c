/*
 * guy_init.c
 *
 *  Created on: 04 ott 2018
 *      Author: andrea
 */

#include "guy_init.h"
#include "guy.h"
#include "parameters.h"

const unsigned int scale_factor = 4;

struct _init_t{
	unsigned healthy;
	unsigned infected;
	unsigned sick;
	unsigned treatment;
	unsigned treated;
};

void guy_init(void){
	init_t total = {num_healthy/scale_factor, num_infected/scale_factor, num_sick/scale_factor, num_treatment/scale_factor, num_treated/scale_factor};
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
		to_send.healthy = random_binomial(total.healthy, 1.0/remaining);
		to_send.infected = random_binomial(total.infected, 1.0/remaining);
		to_send.sick = random_binomial(total.sick, 1.0/remaining);
		to_send.treatment = random_binomial(total.treatment, 1.0/remaining);
		to_send.treated = random_binomial(total.treated, 1.0/remaining);
		// we send the actual event
		//printf("\nrequested i:%u s:%u t:%u f:%u h:%u\n", to_send.infected, to_send.sick, to_send.treated, to_send.treatment, to_send.healthy);
		//fflush(stdout);
		ScheduleNewEvent(i, 0.5, GUY_INIT, &to_send, sizeof(init_t));
		// those people just left this region
		total.healthy -= to_send.healthy;
		total.infected -= to_send.infected;
		total.sick -= to_send.sick;
		total.treatment -= to_send.treatment;
		total.treated -= to_send.treated;
		// we processed a region so we decrease the counter
		remaining--;
	}
}

static guy_t* new_base_guy(void){
	agent_t agent = SpawnAgent(sizeof(guy_t));
	ScheduleNewLeaveEvent(0.75 +  Random()/2, GUY_LEAVE, agent);
	return DataAgent(agent, NULL);
}

static void new_infected(void){
	guy_t *guy = new_base_guy();

	// select a random age following the given distribution
	unsigned i = 0;
	double p = Random();
	while(p_infected_age[i] < p)
		++i;

	guy->birth_day = 0 - RandomRange(365 * age_groups[i], 365 * age_groups[i+1] - 1);

	// select random origin
	if(Random() < p_infected_foreign)
		bitmap_set(guy->flags, f_foreigner);
	else
		bitmap_reset(guy->flags, f_foreigner);
	// select random gender
	if(Random() >= p_infected_male)
		bitmap_set(guy->flags, f_female);
	else
		bitmap_reset(guy->flags, f_female);

	set_risk_factors(guy);

	// set a compatible infection time xxx this is fairly different from the original model
	do{
		guy->infection_day = 0 - RandomRange(0, 365 * t_infected_max - 1);
	}while(guy->infection_day < guy->birth_day);
	// set infected state
	bitmap_reset(guy->flags, f_sick);
	bitmap_reset(guy->flags, f_treatment);
}

static void sickened_base_setup(guy_t *guy){
	// select a random age following the given distribution
	unsigned i = 0;
	double p = Random();
	while(p_sickened_age[i] < p)
		++i;

	guy->birth_day = 0 - RandomRange(365 * age_groups[i], 365 * age_groups[i+1] - 1);

	// select random origin
	if(Random() < p_sickened_foreign)
			bitmap_set(guy->flags, f_foreigner);
		else
			bitmap_reset(guy->flags, f_foreigner);
	// select random gender
	if(Random() >= p_sickened_male)
			bitmap_set(guy->flags, f_female);
		else
			bitmap_reset(guy->flags, f_female);
	// set smear flag
	if(Random() < p_smear)
			bitmap_set(guy->flags, f_smear);
		else
			bitmap_reset(guy->flags, f_smear);
}

static void new_sick(void){
	guy_t *guy = new_base_guy();

	sickened_base_setup(guy);

	define_diagnose(guy, 0.0);
	// we suppose this guy has already been sick for some time
	// (notice that this is equivalent to what is done in the original model
	if(guy->treatment_day)
		guy->treatment_day -= RandomRange(0, guy->treatment_day - 1);
	// set sick state
	bitmap_set(guy->flags, f_sick);
	bitmap_reset(guy->flags, f_treatment);
}

static void new_treatment(void){
	guy_t *guy = new_base_guy();

	sickened_base_setup(guy);
	// we suppose this guy has already been under treatment for some time
	// (notice that this is equivalent to what is done in the original model)
	guy->treatment_day = 0 - RandomRange(0, t_treatment_max - 1);
	// set treatment state
	bitmap_set(guy->flags, f_sick);
	bitmap_set(guy->flags, f_treatment);
}

static void new_treated(void){
	guy_t *guy = new_base_guy();

	sickened_base_setup(guy);
	// we suppose this guy has already been under treatment for some time
	// (notice that this is equivalent to what is done in the original model)
	guy->treatment_day = Random() < p_abandon ? 0 - RandomRange(t_treatment_min, t_treatment_max - 1) : 0 - (int)t_treatment_max;
	// compute relapse probability
	compute_relapse_p(guy, 0.0);
	// set treatment state
	bitmap_reset(guy->flags, f_sick);
	bitmap_set(guy->flags, f_treatment);
}

void guy_on_init(init_t *init_data, region_t *region){

	unsigned i = init_data->infected;

	while(i--)
		new_infected();

	i = init_data->sick;

	while(i--)
		new_sick();

	i = init_data->treatment;

	while(i--)
		new_treatment();

	i = init_data->treated;

	while(i--)
		new_treated();

	region->healthy = init_data->healthy;
}

