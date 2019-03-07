/*
 * guy.h
 *
 *  Created on: 20 lug 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_GUY_H_
#define MODELS_TUBERCOLOSIS_GUY_H_

#include <ROOT-Sim.h>
#include "application.h"
#include "bitmap.h"

// the flags used in the guy struct
enum _flags_t{
	f_sick,		/// the guy is either in the "sick" state or the "under treatment" state IFF this flag is set
	f_treatment,	/// the guy is either in the "treated" state or the "under treatment" state IFF this flag is set
	f_foreigner,
	f_female,
	f_smear,
	f_smokes,
	f_has_diabetes,
	f_has_hiv,
	flags_count
};

// notice that we use absolute times for guys fields so that we don't need to refresh all those memory location
// at each time step with only a minor increase in operations (some integer additions)
// TODO the guy struct can be even more space efficient with some tricks: implement them
typedef struct _guy_t{
	rootsim_bitmap flags[bitmap_required_size(flags_count)];
	int birth_day; /// this is an int since a guy could be born in the past (before the simulation start)
	int infection_day;
	int treatment_day;
	double p_relapse;
} guy_t;

typedef struct _infection_t infection_t;

void guy_on_visit(agent_t agent, unsigned me, region_t *region, simtime_t now);
void guy_on_leave(agent_t agent, simtime_t now);
void guy_on_infection(infection_t *inf, region_t *region, simtime_t now);


void define_diagnose(guy_t *guy, simtime_t now);
void set_risk_factors(guy_t *guy);
void compute_relapse_p(guy_t* guy, simtime_t now);

#endif /* MODELS_TUBERCOLOSIS_GUY_H_ */
