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

void ProcessEvent(unsigned int me, simtime_t now, int event_type, agent_t *agent_p, unsigned int event_size, region_t *state) {

	unsigned i, j, directions, dest;
	float unlike;
	agent_t this_agent;
	guy_t *guy;
	region_t *neighbour_data;
	bool can_exit;
	if(event_type != INIT)
		state->started = true;
	switch (event_type) {
		case INIT:
			(void)event_size;
			// standard stuff
			region_t *region = malloc(sizeof(region_t));

			SetState(region);

			region->n.agents = 0;
			region->n.has_engineer = false;
			region->violation = 0;
			region->happy = false;
			region->started = false;
			// here we do what I explained earlier
			TrackNeighbourInfo(region);
			// for simplicity we spawn a single bug at region 0
			if(Random() < AGENT_SPAWN_PROBABILITY){
				region->n.agents++;
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

			if(!state->happy && can_exit){
				do{
					j = RandomRange(0, directions - 1);
				}while(GetNeighbourInfo(j, &dest, (void**)&neighbour_data) < 0 || neighbour_data->n.agents >= 1);
				// this is our planned visit
				state->n.agents--;
				EnqueueVisit(*agent_p, dest, GUY_DELAYED_VISIT);
			}else{
				ScheduleNewLeaveEvent(now + Random()*TIME_STEP + 0.0001, GUY_LEAVE, *agent_p);
			}
			break;

		case _TRAVERSE: // we only schedule visits to neighbours, we shouldn't cross any "intermediate" region
			/* no break */
		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
			exit(EXIT_FAILURE);
	}

}

int OnGVT(unsigned int me, region_t *snapshot) {
	(void)me;
	return snapshot->started && (snapshot->happy || snapshot->n.agents == 0);
}
