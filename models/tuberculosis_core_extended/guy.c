/*
 * guy.c
 *
 *  Created on: 20 lug 2018
 *      Author: andrea
 */

#include <assert.h>
#include "application.h"
#include "guy.h"
#include "parameters.h"

static const double move_avg = 2.0;

// this flags are used in the infection structure, read below for more infos
enum _infl_t{
	infl_middle_age,
	infl_elder_age,
	infl_foreigner,
	infl_female,
	infl_smear,
	infl_count
};

// since in the original model sick guys spread the disease in their NEIGHBOURHOOD (and not in their same cell)
// we need to send infection messages to handle those remote interactions
struct _infection_t{
	rootsim_bitmap flags[bitmap_required_size(infl_count)];
};

static unsigned int find_neighbhour(region_t *region)
{
	const unsigned neighbours = DirectionsCount();
	unsigned int tentative, direction;

	do {
		direction = neighbours * Random() + 1;
		tentative = GetReceiver(region->me, direction, false);
	} while(tentative == DIRECTION_INVALID);

	return tentative;
}

void schedule_guy_for_leave(region_t *region, struct guy_t *guy)
{
	struct guy_msg_t msg;
	msg.id = guy->id;
	msg.state = guy->state;
	msg.dest = find_neighbhour(region);

	ScheduleNewEvent(region->me, region->now + Expent(move_avg), GUY_LEAVE, &msg, sizeof(msg));
}

void guy_remove_entry(struct guy_t **head, struct guy_t *const entry)
{
	while((*head) != entry) {
		head = &(*head)->next;
	}
	*head = entry->next;
	free(entry);
}

void guy_init_list(struct guy_t **const head)
{
	*head = malloc(sizeof(**head));
	memset(*head, 1, sizeof(**head));
	(*head)->next = NULL;
}

void guy_fini_list(struct guy_t **head)
{
	while(*head)
		guy_remove_entry(head, *head);
}

struct guy_t *guy_list_head(struct guy_t **const head)
{
	return (*head)->next;
}

struct guy_t *guy_add_head(struct guy_t **head, struct guy_t *node)
{
	node->next = (*head)->next;
	(*head)->next = node;

	return node;
}

// this handles the infection step done by guys in the sick state
static void infect(unsigned me, struct guy_t *guy, simtime_t now){
	// prepare infection data
	infection_t inf_data = {0};
	// set gender
	if(bitmap_check(guy->flags, f_female))
		bitmap_set(inf_data.flags, infl_female);
	else
		bitmap_reset(inf_data.flags, infl_female);
	// set origin
	if(bitmap_check(guy->flags, f_foreigner))
		bitmap_set(inf_data.flags, infl_foreigner);
	else
		bitmap_reset(inf_data.flags, infl_foreigner);
	// set smear
	if(bitmap_check(guy->flags, f_smear))
		bitmap_set(inf_data.flags, infl_smear);
	else
		bitmap_reset(inf_data.flags, infl_smear);

	// set age bits
	if(now >= age_class_boundaries[1] + guy->birth_day){
		bitmap_set(inf_data.flags, infl_elder_age);
	}else{
		if(now >= age_class_boundaries[0] + guy->birth_day){
			bitmap_set(inf_data.flags, infl_middle_age);
		}
	}
	// send to the neighbours the infection MUAHAHAH >:)
	unsigned target = 0;
	unsigned i = DirectionsCount();
	while(i--){
		if((target = GetReceiver(me, i, false)) !=  DIRECTION_INVALID)
			ScheduleNewEvent(target, now + 0.0001, INFECTION, &inf_data, sizeof(infection_t));
	}
}

static void guy_sick_update(struct guy_t *guy, unsigned me, simtime_t now, region_t *region){
	// we check whether this is a sick guy, else we exit
	if(!bitmap_check(guy->flags, f_sick) || bitmap_check(guy->flags, f_treatment))
		return;

	// we check if this guy has been finally diagnosed
	if(guy->treatment_day <= now){
		// if there's no error this isn't necessary
		guy->treatment_day = now;
		// set the guy to an under treatment state
		bitmap_set(guy->flags, f_treatment);
		if (region->sick > 0) {
			region->sick--;
		}
		region->treatment++;
		//TODO: decrement region->sick and increment region->treatment
		return;
	}

	// we spread the disease >:)
	infect(me, guy, now);
}

// this sets the guy with the time on when to start the treatment
void define_diagnose(struct guy_t *guy, simtime_t now){
	double delay = Normal()*diagnose_std + diagnose_mean[bitmap_check(guy->flags, f_foreigner)];
	// with our data this is extremely rare, but why risk a very rare bug for something that simple?
	delay = delay < 0 ? 0: delay;
	guy->treatment_day = now + delay;
}

void set_risk_factors(struct guy_t *guy){
	// set risks factor
	if(Random() < p_HIV)
		bitmap_set(guy->flags, f_has_hiv);
	else
		bitmap_reset(guy->flags, f_has_hiv);

	if(Random() < p_smoking)
		bitmap_set(guy->flags, f_smokes);
	else
		bitmap_reset(guy->flags, f_smokes);

	if(Random() < p_diabetes)
		bitmap_set(guy->flags, f_has_diabetes);
	else
		bitmap_reset(guy->flags, f_has_diabetes);
}

void compute_relapse_p(struct guy_t *guy, simtime_t now){
	// pre-computed value
	// static const double p_scale = (1 - p_relapse_min)/(t_treatment_max - t_treatment_min);
	static const double p_scale = (1 - 0.01)/(180 - 15);
	// compute the relapse probability
	guy->p_relapse = ((double)t_treatment_max - now + guy->treatment_day)*p_scale + p_relapse_min;
	// we do this here to save some divisions (I guess this computes the daily relapse probability)
	// guy->p_relapse /= t_to_healthy + guy->treatment_day - now
	guy->p_relapse /= 714;
}

static bool guy_infected_update(struct guy_t *guy, region_t *region, simtime_t now){
	// we check whether this is a infected guy, else we exit
	if(bitmap_check(guy->flags, f_sick) || bitmap_check(guy->flags, f_treatment))
		return false;

	// we check whether this guy is healing since he has reached the maximum infection time
	if(now >= guy->infection_day + t_infected_max){
		// we don't need this agent anymore...
//		KillAgent(agent);
		// ...since he becomes a healthy person
		region->healthy++;
		if (region->infected > 0) {
			region->infected--;
		}
		//TODO: decrement region->infected
		return true;
	}

	// how many years passed since initial infection imply a different probability of getting worse
	double prob = p_sicken[((unsigned) now - guy->infection_day)%365];
	// age also has a role
	if(now < guy->birth_day + 5*365) {
		prob *= f_sicken_child;
	} else if(now < guy->birth_day + 15*365) {
		prob *= f_sicken_young;
	}

	// we also take into account other factors: HIV, diabetes, smoking
	if(bitmap_check(guy->flags, f_has_hiv)){
		prob *= f_sicken_HIV;
	}
	if(bitmap_check(guy->flags, f_has_diabetes)) {
		prob *= f_sicken_diabetes;
	}
	if(bitmap_check(guy->flags, f_smokes)) {
		prob *= f_sicken_smoking;
	}

	// factor to fit desired behaviour (XXX: need to understand better)
	prob *= 0.9;

	// the actual check is done here
	if(Random() < prob){
		// decide if this is a smear positive case
		if(Random() < p_smear)
			bitmap_set(guy->flags, f_smear);
		else
			bitmap_reset(guy->flags, f_smear);
		// decide when to start the treatment
		define_diagnose(guy, now);
		// set this guy to a sick state
		bitmap_set(guy->flags, f_sick);
		region->sick++;
		if (region->infected > 0) {
			region->infected--;
		}
		//TODO: increment here region->sick and decrement region->infected
	}
	return false;
}

static void guy_treatment_update(struct guy_t *guy, simtime_t now, region_t *region){
	// we check whether this is a guy under treatment, else we exit
	if(!bitmap_check(guy->flags, f_sick) || !bitmap_check(guy->flags, f_treatment))
		return;
	// pre-computed value
	// static const double daily_prob = p_abandon/t_treatment_max;
	static const double daily_prob = 0.022/180;
	// a guy can either prematurely abandon the treatment or just complete it
	if(Random() < daily_prob || now >= guy->treatment_day + t_treatment_max) {
		compute_relapse_p(guy, now);
		// set the guy to a treated state
		if (region->treatment > 0) {
			region->treatment--;
		}
		region->treated++;
		bitmap_reset(guy->flags, f_sick);

		//TODO: decrement here region->treatment and increment region->treated
	}
}

static bool guy_treated_update(struct guy_t *guy, region_t *region, simtime_t now){
	// we check whether this is a treated guy, else we exit
	if(bitmap_check(guy->flags, f_sick) || !bitmap_check(guy->flags, f_treatment))
		return false;

	if (region->treated > 0) {
		region->treated--;
	}

	if(now >= t_to_healthy + guy->treatment_day) {
		// this guy can either recover...
//		KillAgent(agent);
		region->healthy++;
		//TODO: decrement here region->treated
		return true;
	} else if(Random() < guy->p_relapse) {
		// ... or can get sick again...
		define_diagnose(guy, now);
		bitmap_set(guy->flags, f_sick);
		region->sick++;

		//TODO: increment here region->sick and decrement region->treated
	}

	return false;
	// ... or finally live "normally" for another day
}

// this is the routine every guy follows when he enters a region.
// I remained faithful to the original model:
// for example, a guy in the "sick" state can potentially
// switch to the "under treatment" state,
// then switch to the "treated" state, and then finally switch back to
// the "sick" state, all in a single iteration of this function.
void guy_on_visit(struct guy_t *guy, unsigned me, region_t *region, simtime_t now){

	guy_sick_update(guy, me, now, region);

	if(guy_infected_update(guy, region, now))
		return; // the agent pointer is invalid cause we freed the agent

	guy_treatment_update(guy, now, region);
	if(guy_treated_update(guy, region, now))
		return;

//	ScheduleNewLeaveEvent(now + 0.75 + Random()/2, GUY_LEAVE, guy);
}

static double die_probability(unsigned age){
	if(age < p_die_age[0])
		return p_die[0];

	if(age < p_die_age[1])
		return p_die[1];

	return p_die[2];
}

struct guy_t *find_guy_from_id(region_t *region, struct guy_msg_t *guy_msg)
{
	struct guy_t *list = region->agents[guy_msg->state];

	struct guy_t *current = guy_list_head(&list);
	while(current) {
		if(current->id == guy_msg->id)
			break;
		current = current->next;
	}
	assert(current->id == guy_msg->id);
	return current;
}


void guy_on_leave(struct guy_msg_t *guy_msg, simtime_t now, region_t *region)
{
	struct guy_t *guy = find_guy_from_id(region, guy_msg);
	bool guy_dies = false;
	bool sick = bitmap_check(guy->flags, f_sick);
	bool treatment = bitmap_check(guy->flags, f_treatment);
	if(!sick || treatment){
		// non-sick guy case
		guy_dies = Random() < die_probability(now - guy->birth_day);
	}else{
		// sick guy case
		guy_dies = Random() < p_die_sick;
	}

	if(guy_dies){
		bool infected = !sick && !treatment;
		bool inTreatment = sick && treatment;
		bool inSick = sick && !treatment;
		bool treated = !sick && treatment; 
		if (infected && region->infected > 0) {
			region->infected--;
			//TODO: decrement region->infected
		} else if (inTreatment && region->treatment > 0) {
			region->treatment--;
			//TODO: decrement region->treatment
		} else if (inSick && region->sick > 0) {
			region->sick--;
			//TODO: decrement region->sick
		} else if (treated && region->treated > 0) {
			region->treated--;
			//TODO: decrement region->treated
		}

		guy_remove_entry(&(region->agents[guy_msg->state]), guy);
	} else {
		ScheduleNewEvent(guy_msg->dest, region->now, GUY_VISIT, guy, sizeof(*guy));
	}
}

// this function does some shenanigans on the some parameters array
// (notice that a I changed the parameters arrays layout to a more consistent one)
static int infected_age(infection_t *inf){
	unsigned i = 0, j = 0, k = 0;
	double r;
	// we select the suitable probability table for this case
	if (bitmap_check(inf->flags, infl_middle_age)){
		if(bitmap_check(inf->flags, infl_female)){
			j = 2;
		}else{
			j = 1;
		}
	}else{
		if(bitmap_check(inf->flags, infl_elder_age)){
			j = 3;
		}else{
			j = 0;
		}
	}
	// now we choose at random an age group for the infected
	r = Random();
	while(p_contacts_age_class[j][i] < r)
		i++;
	// now, between the age group we choose a specific age range
	r = Random();
	while(p_contacts_age_groups[i][k] < r)
		k++;
	// finally we random some specific age value in the range
	return RandomRange(contacts_age_groups[i][k], contacts_age_groups[i][k+1] - 1);
}

unsigned infected_gender_origin(infection_t *inf){
	unsigned i = 0, j = 0;
	double r;

	i = bitmap_check(inf->flags, infl_female) + 2*bitmap_check(inf->flags, infl_foreigner);
	// i. 0: native male, 1: native female, 2: foreign male, 3: foreign female

	r = Random();
	while(p_origin_gender[i][j] < r)
		j++;

	// j. 0: native male, 1: native female, 2: foreign male, 3: foreign female
	return j;
}

// this handles an infection message
void guy_on_infection(infection_t *inf, region_t *region, simtime_t now){
	struct guy_t *guy = NULL;
	// as in the original model, each healthy person has the same probability
	// of becoming infected (but we do this more efficiently using a binomial PRNG)
	unsigned infections =
			random_binomial(region->healthy, (1 + bitmap_check(inf->flags, infl_smear))*p_infect);
	// the infected guys of course diminish the healthy population
	region->healthy -= infections;
	unsigned aux;

	while(infections--){
		// boilerplate initialization
//		agent = SpawnAgent(sizeof(struct guy_t));
//		guy = DataAgent(agent, NULL);
		// set infection day
		guy->infection_day = now;
		// set age
		guy->birth_day = now - infected_age(inf);
		// set origin and gender
		aux = infected_gender_origin(inf);

		if(aux & (1 << 0))
			bitmap_set(guy->flags, f_female);
		else
			bitmap_reset(guy->flags, f_female);

		if(aux & (1 << 1))
			bitmap_set(guy->flags, f_foreigner);
		else
			bitmap_reset(guy->flags, f_foreigner);

		set_risk_factors(guy);
		//set infected state
		bitmap_reset(guy->flags, f_sick);
		bitmap_reset(guy->flags, f_treatment);
		region->infected++;
		//TODO: increment region->infected
		// we immediately schedule the first agent hop
//		ScheduleNewLeaveEvent(now + 0.001, GUY_LEAVE, guy); // TODO: guy rompe tutto: serve id?
	}
}

void guy_stats(unsigned guy_counts[4])
{
//	guy_counts[0] = 0;
//	guy_counts[1] = 0;
//	guy_counts[2] = 0;
//	guy_counts[3] = 0;
//	unsigned tot = CountAgents();
//	uint32_t closure = 0;
//	while(IterAgents(&agent, &closure)){
//		struct guy_t *guy = DataAgent(agent, NULL); // TODO
//		guy_counts[2 * bitmap_check(guy->flags, f_sick) + bitmap_check(guy->flags, f_treatment)]++;
//		tot--;
//	}
//	if(tot)
//		printf("wrong iteration!");
}
