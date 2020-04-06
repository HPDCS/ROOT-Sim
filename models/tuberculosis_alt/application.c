/*
 * application.c
 *
 *  Created on: 19 lug 2018
 *      Author: andrea
 */

#include "application.h"
#include "guy.h"
#include "guy_init.h"

#include <math.h>
#include <stdio.h>

static bool approximated = true;
static FILE *stats_file;

static unsigned class_stats[1600][END_TIME][5];

enum{
	OPT_PREC = 128 /// this tells argp to not assign short options
};

const struct argp_option model_options[] = {
		{"precise-mode", OPT_PREC, NULL, 0, NULL, 0},
		{0}
};

#define HANDLE_ARGP_CASE(label, fmt, var)	\
	case label: \
		if(sscanf(arg, fmt, &var) != 1){ \
			return ARGP_ERR_UNKNOWN; \
		} \
	break

static error_t model_parse(int key, char *arg, struct argp_state *state) {
	(void)state;

	switch (key) {
		case OPT_PREC:
			approximated = false;
		break;

		case ARGP_KEY_SUCCESS:
			printf("\t* ROOT-Sim's TBC model - Current Configuration *\n");
			printf("approximated: %d\n", approximated);
			stats_file = fopen("tbc_stats.txt", "a");
			if(!stats_file){
				printf("Unable to open tbc stats file");
				exit(EXIT_FAILURE);
			}
			fprintf(stats_file, "*** %s\n", approximated ? "approximated" : "precise");
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef HANDLE_ARGP_CASE

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .write_enabled = false, .default_geometry = TOPOLOGY_SQUARE};
struct _abm_settings_t abm_settings = {0, _TRAVERSE, false};
struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};

// From Luc Devroye's book "Non-Uniform Random Variate Generation." p. 522
unsigned random_binomial(unsigned trials, double p) { // this is exposed since it is used also in guy.c
	if(p >= 1.0 || !trials)
		return trials;
	unsigned x = 0;
	double sum = 0, log_q = log(1.0 - p); // todo cache those logarithm value
	while(1) {
		sum += log(Random()) / (trials - x);
		if(sum < log_q || trials == x) {
			return x;
		}
		x++;
	}
	return 0;
}
// this is better than calling FindReceiver() for each healthy person
static void move_healthy_people(unsigned me, region_t *region, simtime_t now){
	// these are the theoretical neighbours (that is, NeighboursCount ignores borders)
	const unsigned neighbours = DirectionsCount();
	unsigned i = neighbours, to_send;
	// we use this to keep track of the already dispatched neighbours
	rootsim_bitmap explored[bitmap_required_size(neighbours)];
	bitmap_initialize(explored, neighbours);
	// now we count the actual neighbours TODO this is probably a common operation: APIFY IT!
	unsigned actual_neighbours = 0;
	while(i--){
		if(GetReceiver(me, i, false) != DIRECTION_INVALID)
			actual_neighbours++;
		else
			bitmap_set(explored, i); // this way we won't ask for that direction again
	}
	// we do this in 2 rounds to minimize the effect of the bias of the binomial generator xxx: is it needed? Perform some tests!
	while(actual_neighbours){
		// we pick a new random direction
		i = Random()*neighbours;
		if(bitmap_check(explored, i))
			continue;
		// we mark this direction to "explored"
		bitmap_set(explored, i);
		// we compute the number of healthy people who choose this direction
		to_send = random_binomial(region->healthy, 1.0/actual_neighbours);
		// we send the actual event
		ScheduleNewEvent(GetReceiver(me, i, false), now, RECEIVE_HEALTHY, &to_send, sizeof(unsigned));
		// those people just left this region
		region->healthy -= to_send;
		// we processed a valid neighbour so we decrease the counter
		actual_neighbours--;
	}
}

// we handle infects visits move at slightly randomized timesteps 1.0, 2.0, 3.0...
// healthy people are moved at slightly randomized timesteps 0.5, 1.5, 2.5, 3.5...
// this way we preserve the order of operation as in the original model
void ProcessEvent(unsigned int me, simtime_t now, int event_type,
		union {agent_t *agent; unsigned *n; infection_t *i_m; init_t *in_m;} payload, unsigned int event_size, region_t *state) {

	if(event_type != INIT)
		state->now = now;

	switch (event_type) {
		case INIT:
			(void)event_size;
			// standard stuff
			region_t *region = malloc(sizeof(region_t));
			SetState(region);
			region->healthy = 0;
			region->now = 0.0;
			if(!me){
				// this function let LP0 coordinate the init phase
				guy_init();
				printf("INIT 0 complete\n");
			}
			ScheduleNewEvent(me, now + 1.25 + Random()/2, MIDNIGHT, NULL, 0);
			ScheduleNewEvent(me, 1.0, STATS_COMPUTE, NULL, 0);
			RollbackModeSet(approximated);
			break;

		case STATS_COMPUTE:
			if(now < END_TIME)
				ScheduleNewEvent(me, now + 1.0, STATS_COMPUTE, NULL, 0);

			class_stats[me][(unsigned)now][0] = state->healthy;


			guy_stats(&class_stats[me][(unsigned)now][1]);
			break;

		case MIDNIGHT:
			move_healthy_people(me, state, now);
			ScheduleNewEvent(me, now + 0.25 + Random()/2, MIDNIGHT, NULL, 0);
			break;

		case RECEIVE_HEALTHY:
			state->healthy += *(payload.n);
			break;

		case INFECTION:
			guy_on_infection(payload.i_m, state, now);
			break;

		case GUY_VISIT:
			guy_on_visit(*payload.agent, me, state, now);
			break;

		case GUY_LEAVE:
			guy_on_leave(*payload.agent, now);
			break;

		case GUY_INIT:
			guy_on_init(payload.in_m, state);
			break;

		case _TRAVERSE:
		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
			exit(EXIT_FAILURE);
	}

}

int OnGVT(unsigned int me, region_t *snapshot) {
	if(snapshot->now > END_TIME){
		for(unsigned i = 0; i < END_TIME; ++i){
			fprintf(stats_file, "%u %u %u %u %u %u %u\n", i, me,
					class_stats[me][i][0],
					class_stats[me][i][1],
					class_stats[me][i][2],
					class_stats[me][i][3],
					class_stats[me][i][4]);
		}
	}

	return snapshot->now > END_TIME;
}

void RestoreApproximated(void *ptr)
{
}

