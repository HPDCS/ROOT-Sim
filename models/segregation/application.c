#include "application.h"

#include <stdio.h>

typedef struct _region_t{
	struct n_data{
		bool has_engineer;
		unsigned agents;
	}n;
	unsigned violation;
	bool happy, started;
}region_t;

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .default_geometry = TOPOLOGY_HEXAGON, .write_enabled = false};
struct _abm_settings_t abm_settings = {.neighbour_data_size = sizeof(struct n_data), .traverse_handler = _TRAVERSE, .keep_history = false};

typedef struct _guy_t {
	bool engineer;
} guy_t;

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, const void *payload, size_t size, void *st)
{
	(void)size;
	
	unsigned i, j, directions, dest;
	float unlike;
	agent_t this_agent;
	guy_t *guy;
	region_t *neighbour_data;
	bool can_exit;
	region_t *state = (region_t *)st;
	agent_t *agent_p = (agent_t *)payload;
	
	if(event != INIT)
		state->started = true;
		
	switch (event) {
		case INIT:
			// standard stuff
			state = malloc(sizeof(region_t));

			SetState(state);

			state->n.agents = 0;
			state->n.has_engineer = false;
			state->violation = 0;
			state->happy = false;
			state->started = false;
			
			// here we do what I explained earlier
			TrackNeighbourInfo(state);
			
			// for simplicity we spawn a single bug at region 0
			if(Random() < AGENT_SPAWN_PROBABILITY) {
				state->n.agents++;
				this_agent = SpawnAgent(sizeof(guy_t));
				guy = DataAgent(this_agent, NULL);
				guy->engineer = Random() < AGENT_IS_ENGINEER_PROBABILITY;
				ScheduleNewLeaveEvent(now + Random()*TIME_STEP + 0.0001, GUY_LEAVE, this_agent);
			}

			ScheduleNewEvent(me, now + 10*Random()*TIME_STEP + 0.001, KEEP_ALIVE, NULL, 0);
			break;

		case KEEP_ALIVE:
			ScheduleNewEvent(me, now + 10*Random()*TIME_STEP + 0.001, KEEP_ALIVE, NULL, 0);
			break;

		case GUY_DELAYED_VISIT:
			ScheduleNewEvent(me, now + 0.0001, GUY_VISIT, agent_p, sizeof(*agent_p));
			break;

		case GUY_VISIT:
			guy = DataAgent(*agent_p, NULL);
			state->n.agents++;
			state->n.has_engineer = guy->engineer;

			if(CountAgents() > 1) {
				state->violation++;
				KillAgent(*agent_p);
				break;
			}

			ScheduleNewLeaveEvent(now + Random()*TIME_STEP + 0.0001, GUY_LEAVE, *agent_p);
			break;

		case GUY_LEAVE:
			guy = DataAgent(*agent_p, NULL);
			unlike = 0.0;
			directions = DirectionsCount();
			can_exit = false;
			
			for(i = 0; i < directions; ++i) {
				if(GetNeighbourInfo(i, &dest, (void**)&neighbour_data) < 0)
					continue;

				if(neighbour_data->n.agents && guy->engineer != neighbour_data->n.has_engineer)
					unlike += 1.0;

				if(neighbour_data->n.agents < 1) {
					can_exit = true;
				}
			}

			state->happy = unlike/DirectionsCount() < AGENT_THRESHOLD;

			if(!state->happy && can_exit) {
				do {
					j = RandomRange(0, directions - 1);
				} while(GetNeighbourInfo(j, &dest, (void**)&neighbour_data) < 0 || neighbour_data->n.agents >= 1);
				
				// this is our planned visit
				state->n.agents--;
				EnqueueVisit(*agent_p, dest, GUY_DELAYED_VISIT);
			} else {
				ScheduleNewLeaveEvent(now + Random()*TIME_STEP + 0.0001, GUY_LEAVE, *agent_p);
			}
			break;

		case _TRAVERSE: // we only schedule visits to neighbours, we shouldn't cross any "intermediate" region
			/* no break */
		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event);
			exit(EXIT_FAILURE);
	}

}

bool CanTerminate(unsigned int me, const void *snapshot, simtime_t now) {
	(void)me;
	(void)now;
	
	region_t *state = (region_t *)snapshot;
	
	return state->started && (state->happy || state->n.agents == 0);
}

void OnCommittedState(unsigned int me, const void *snapshot, simtime_t now)
{
	(void)me;
	(void)snapshot;
	(void)now;
}
