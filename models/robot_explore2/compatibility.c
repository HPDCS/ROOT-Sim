#include <ROOT-Sim.h>
#include "application.h"

void ProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, void *state) {

	switch(event_type) {
		case INIT:
			break;
	}
}

int OnGVT(unsigned int me, void *snapshot) {
}
