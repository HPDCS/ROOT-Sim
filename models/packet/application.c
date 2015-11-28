#include <ROOT-Sim.h>
#include "application.h"


void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, event_t *content, unsigned int size, lp_state_t *state) {
	event_t new_event;
	simtime_t timestamp;

	switch(event) {

		case INIT: // must be ALWAYS implemented
			state = (lp_state_t *)malloc(sizeof(lp_state_t));
	 		state->packet_count = 0;
			state->pointer = malloc(sizeof(int));
			SetState(state);
			timestamp = (simtime_t)(20 * Random());
			ScheduleNewEvent(me, timestamp, PACKET, NULL, 0);
			break;

		case PACKET: {
			state->packet_count++;
			if(content != NULL && content->sender == me){
				//free(content->pointer);
				//printf("Free memory lp=%d\n",me);
			}
			new_event.sent_at = now;
			new_event.pointer = state->pointer;
			new_event.sender = me;
			int recv = FindReceiver(TOPOLOGY_MESH);
			timestamp = now + Expent(DELAY);
			ScheduleNewEvent(recv, timestamp, PACKET, &new_event, sizeof(new_event));
		}
	}
}


bool OnGVT(unsigned int me, lp_state_t *snapshot) {

	if(me == 0) {
		printf("Completed work: %f\%\n", (double)snapshot->packet_count/PACKETS*100);
	}

	if (snapshot->packet_count < PACKETS)
		return false;
	return true;
}
