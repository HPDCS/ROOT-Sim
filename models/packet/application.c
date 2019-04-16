#include <ROOT-Sim.h>
#include "application.h"

struct _topology_settings_t topology_settings = {.default_geometry = TOPOLOGY_GRAPH};

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, event_t *content, unsigned int size, lp_state_t *state) {
	(void)size;
	event_t new_event;
	simtime_t timestamp;

	switch(event) {

		case INIT: // must be ALWAYS implemented
			state = (lp_state_t *)malloc(sizeof(lp_state_t));
	 		state->packet_count = 0;
			state->pointer = (int *)malloc(sizeof(int));
			state->pointer[0] = 0;
			SetState(state);
			timestamp = (simtime_t)(20 * Random());
			ScheduleNewEvent(me, timestamp, PACKET, NULL, 0);
			break;

		case PACKET: {
			state->packet_count++;
			if(content != NULL && content->sender != me) {
			//	if(content->pointer!=NULL){
			//		content->pointer[0]++;
			//	}
			}
			new_event.sent_at = now;
			new_event.pointer = state->pointer;
			new_event.sender = me;

			int recv = FindReceiver();

			timestamp = now + Expent(DELAY);

			ScheduleNewEvent(recv, timestamp, PACKET, &new_event, sizeof(new_event));
		}
	}
}


bool OnGVT(unsigned int me, lp_state_t *snapshot) {
	(void)me;

	if (snapshot->packet_count >= PACKETS)
		return true;
	return false;
}
