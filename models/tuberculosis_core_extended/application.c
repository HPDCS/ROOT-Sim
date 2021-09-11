/*
 * application.c
 *
 *  Created on: 19 lug 2018
 *      Author: andrea
 */

#include "application.h"
#include "parameters.h"
#include "guy.h"
#include "guy_init.h"

#include <math.h>
#include <stdio.h>

static bool approximated = true;
static bool autonomic = false;
double infection_p = P_INFECT;
static FILE *stats_file;

enum {
    OPT_PREC = 128, /// this tells argp to not assign short options
    OPT_AUTON,
	OPT_INF
};

const struct argp_option model_options[] = {{"precise-mode",   OPT_PREC,  NULL, 0, NULL, 0},
					    {"autonomic-mode", OPT_AUTON, NULL, 0, NULL, 0},
						{"infection-p", OPT_INF, "VALUE", 0, NULL, 0},
					    {0}};

#define HANDLE_ARGP_CASE(label, fmt, var)        \
        case label: \
                if(sscanf(arg, fmt, &var) != 1){ \
                        return ARGP_ERR_UNKNOWN; \
                } \
        break

static error_t model_parse(int key, char *arg, struct argp_state *state)
{
	(void) state;
	(void) arg;

	switch(key) {
		case OPT_PREC:
			approximated = false;
			break;

		case OPT_AUTON:
			approximated = false;
			autonomic = true;
			break;

		HANDLE_ARGP_CASE(OPT_INF, "%lf", infection_p);

		case ARGP_KEY_SUCCESS:
			printf("\t* ROOT-Sim's TBC model - Current Configuration *\n");
			printf("approximated: %d\n", approximated);
			printf("autonomic: %d\n", autonomic);
			stats_file = fopen("tbc_stats.txt", "w");
			if(!stats_file) {
				printf("Unable to open tbc stats file");
				exit(EXIT_FAILURE);
			}
			if(autonomic) {
				fprintf(stats_file, "*** autonomic\n");
			} else {
				fprintf(stats_file, "*** %s\n", approximated ? "approximated" : "precise");
			}
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#undef HANDLE_ARGP_CASE

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .write_enabled = false, .default_geometry = TOPOLOGY_SQUARE};
struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};

// From Luc Devroye's book "Non-Uniform Random Variate Generation." p. 522
unsigned random_binomial(unsigned trials, double p, struct drand48_data *rng_state)
{ // this is exposed since it is used also in guy.c
	if(p >= 1.0 || !trials) {
		return trials;
	}
	unsigned x = 0;
	double sum = 0, log_q = log(1.0 - p); // todo cache those logarithm value
	while(1) {
		double r;
		drand48_r(rng_state, &r);
		sum += log(r) / (trials - x);
		if(sum < log_q || trials == x) {
			return x;
		}
		x++;
	}
	return 0;
}

// we handle infects visits move at slightly randomized timesteps 1.0, 2.0, 3.0...
// healthy people are moved at slightly randomized timesteps 0.5, 1.5, 2.5, 3.5...
// this way we preserve the order of operation as in the original model
void ProcessEvent(unsigned int me, simtime_t now, int event_type, union {
	struct guy_t *agents;
	infection_t *i_m;
	init_t *in_m;
} payload, unsigned int event_size, region_t *state)
{
	if(event_type != INIT) {
		state->now = now;
	}

	switch(event_type) {
		case INIT:
			(void) event_size;
			// standard stuff
			region_t *region = malloc(sizeof(region_t));
			memset(region, 0, sizeof(*region));
			SetState(region);

			if(autonomic) {
				RollbackModeSet(AUTONOMIC);
			} else {
				if(approximated) {
					RollbackModeSet(APPROXIMATED);
				} else {
					RollbackModeSet(PRECISE);
				}
			}

			region->me = me;

			srand48_r(Random() * INT_MAX, &(region->random_initialization_buf));

			// initialize lists
			int j = END_STATES;
			while (j--) {
				guy_init_list(&(region->agents[j]));
			}

			if(!me) {
				// this function let LP0 coordinate the init phase
				guy_init(&region->random_initialization_buf);
				printf("INIT 0 complete\n");
			}

			break;

		case INFECTION:
			guy_on_infection(payload.i_m, state);
			break;

		case GUY_RECV:
			guy_recv(me, payload.agents, event_size, state);
			break;

		case GUY_MOVE:
			guy_move(me, state);
			break;

		case GUY_INIT:
			guy_on_init(payload.in_m, state);
			guy_move(me, state);
			break;

		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
			exit(EXIT_FAILURE);
	}

}

int OnGVT(unsigned int me, region_t *snapshot)
{

	fprintf(stats_file, "%lf %u %u %u %u %u %u\n", snapshot->now, me, snapshot->agents_count[0],
		snapshot->agents_count[1], snapshot->agents_count[2], snapshot->agents_count[3],
		snapshot->agents_count[4]);

	return snapshot->now > END_TIME;

}

void RestoreApproximated(void *ptr)
{
	region_t *region = ptr;
	init_t init_data;
	memcpy(init_data.agents_count, region->agents_count, sizeof(region->agents_count));
	int j = END_STATES;
	while (j--) {
		if (j == HEALTHY)
			continue;
		region->agents_count[j] = 0;
		region->agents[j].next = NULL;
		region->agents[j].prev = NULL;
	}
	init_data.agents_count[HEALTHY] = 0;
	guy_on_init(&init_data, region);
}
