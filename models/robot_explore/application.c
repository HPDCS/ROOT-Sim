#include <ROOT-Sim.h>
#include <stdio.h>
#include <limits.h>

#include "application.h"



void *states[4096];
unsigned int num_cells;

#include "agent.h"
#include "cell.h"


void ProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, void *pointer) {

	if(event_type == INIT) {
/*		if(!IsParameterPresent(event_content, "num_cells")) {
			rootsim_error(true, "You must specify the number of cells (num_cells)\n");
		}
		num_cells = GetParameterInt(event_content, "num_cells");

		if(num_cells >= n_prc_tot) {
			rootsim_error(true, "With %d cells I need at least %d LPs\n", num_cells, num_cells + 1);
		}*/
		num_cells = 4096;
	}


	if(is_agent(me)) {
		AgentProcessEvent(me, now, event_type, event_content, event_size, pointer);
	} else {
		CellProcessEvent(me, now, event_type, event_content, event_size, pointer);
	}
}


int OnGVT(unsigned int me, void *snapshot) {

	if(is_agent(me)) {
		return AgentOnGVT(me, snapshot);
	} else  {
		return CellOnGVT(me, snapshot);
	}

	return true;
}
