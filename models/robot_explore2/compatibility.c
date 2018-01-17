#include <ROOT-Sim.h>
#include "application.h"

void ProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, void *state) {

	switch(event_type) {
		case INIT:
			number_of_agents = GetParameterInt(event_content, "agents");
			number_of_regions = GetParameterInt(event_content, "regions");
			
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
