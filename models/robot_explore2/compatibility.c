#include <ROOT-Sim.h>
#include "application.h"


enum{
	OPT_A = 128, /// this tells argp to not assign short options
	OPT_R
};

const struct argp_option model_options[] = {
		{"agents", OPT_A, "UINT", 0, NULL, 0},
		{"regions", OPT_R, "UINT", 0, NULL, 0},
		{0}
};

static error_t model_parse(int key, char *arg, struct argp_state *state) {
	switch (key) {
		case OPT_A:
			if(sscanf(arg, "%u", &number_of_agents) != 1)
				return ARGP_ERR_UNKNOWN;
			break;
		case OPT_R:
			if(sscanf(arg, "%u", &number_of_regions) != 1)
				return ARGP_ERR_UNKNOWN;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

struct argp model_argp = {model_options, model_parse, NULL, NULL, NULL, NULL, NULL};

void ProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, void *state) {

	switch(event_type) {
		case INIT:
			state = region_init(me);
			SetState(state);
			
			if(me < number_of_agents) {
				// Place an agent in this region
				((cell_state_type *)state)->agents = agent_init();
				
			}
				
			break;
			
		default:
			fprintf(stderr, "Error: event type %d is unknown.\n");
			exit(EXIT_FAILURE);
	}
}

int OnGVT(unsigned int me, void *snapshot) {
	return false;
}
