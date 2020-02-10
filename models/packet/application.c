#include <ROOT-Sim.h>
#include "application.h"

struct _topology_settings_t topology_settings = {.default_geometry = TOPOLOGY_GRAPH};

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, const void *payload, size_t size, void *st)
{
	(void)now;
	(void)payload;
	(void)size;
	
	event_t new_event;
	simtime_t timestamp;
	lp_state_t *state = (lp_state_t *)st;

	switch(event) {

		case INIT:
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
			new_event.sent_at = now;
			new_event.pointer = state->pointer;
			new_event.sender = me;

			int recv = FindReceiver();

			timestamp = now + Expent(DELAY);

			ScheduleNewEvent(recv, timestamp, PACKET, &new_event, sizeof(new_event));
		}
	}
}


bool CanTerminate(unsigned int me, const void *snapshot, simtime_t now) {
	(void)me;
	(void)now;

	if (((lp_state_t *)snapshot)->packet_count >= PACKETS)
		return true;
	return false;
}

void OnCommittedState(unsigned int me, const void *snapshot, simtime_t now)
{
	(void)me;
	(void)snapshot;
	(void)now;
}

