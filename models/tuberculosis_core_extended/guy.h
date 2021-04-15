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
struct guy_t {
	unsigned long long id;
	rootsim_bitmap flags[bitmap_required_size(flags_count)];
	int birth_day; /// this is an int since a guy could be born in the past (before the simulation start)
	int infection_day;
	int treatment_day;
	double p_relapse;
	enum agent_state state;
	struct guy_t *next;
};

struct guy_msg_t {
	unsigned id;
	enum agent_state state;
	unsigned int dest;
};

typedef struct _infection_t infection_t;

void guy_on_visit(struct guy_t *, unsigned me, region_t *region, simtime_t now);
void guy_on_leave(struct guy_msg_t *, simtime_t now, region_t *region);
void guy_on_infection(infection_t *inf, region_t *region, simtime_t now);

void schedule_guy_for_leave(region_t *region, struct guy_t *guy);

void guy_stats(unsigned guy_counts[4]);

void define_diagnose(struct guy_t *guy, simtime_t now);
void set_risk_factors(struct guy_t *guy);
void compute_relapse_p(struct guy_t* guy, simtime_t now);

void guy_init_list(struct guy_t **const head);
void guy_remove_entry(struct guy_t **head, struct guy_t *const entry);
void guy_fini_list(struct guy_t **head);
struct guy_t *guy_list_head(struct guy_t **const head);
struct guy_t *guy_add_head(struct guy_t **head, struct guy_t *node);

#endif /* MODELS_TUBERCOLOSIS_GUY_H_ */
