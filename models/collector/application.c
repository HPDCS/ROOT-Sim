#include <ROOT-Sim.h>
#include "application.h"

struct _topology_settings_t topology_settings = {.default_geometry = TOPOLOGY_STAR};

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, event_t *content, unsigned int size, lp_state_t *state) {
	(void)content;
	(void)size;
	event_t new_event;
	simtime_t timestamp;

	switch(event) {

		case INIT: // must be ALWAYS implemented
			state = (lp_state_t *)malloc(sizeof(lp_state_t));
	 		state->packet_count = 0;
			SetState(state);
			timestamp = (simtime_t)(20 * Random());
			ScheduleNewEvent(me, timestamp, PACKET, NULL, 0);
			break;

		case PACKET: {
			state->packet_count++;
			new_event.sent_at = now;
			new_event.sender = me;

			int recv = FindReceiver();

			timestamp = now + Expent(DELAY);

			ScheduleNewEvent(recv, timestamp, PACKET, &new_event, sizeof(new_event));
		}
	}
}


bool OnGVT(unsigned int me, lp_state_t *snapshot) {

	if(me == 0) {
		if (snapshot->packet_count < PACKETS)
			return false;
	}
	return true;
}
