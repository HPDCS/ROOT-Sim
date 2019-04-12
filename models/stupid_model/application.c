#include "../stupid_model/application.h"

#include <stdio.h>

struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .default_geometry = TOPOLOGY_HEXAGON, .write_enabled = false};
struct _abm_settings_t abm_settings = {.neighbour_data_size = sizeof(size_t), .traverse_handler = BUG_TRAVERSE, .keep_history = false};

typedef struct _region_t {
	simtime_t lvt;
	double food_available; 	// cell's amount of food
	double last_bug_size;	// last bug size
	size_t bugs;		// XXX here we count bugs in the cell. Notice that CountAgentsABM() already provides us this information
	 	 	 	// but since we need to track it we have to replicate it here: if basic values of regions are frequently requested
	 	 	 	// for event modeling we can render them available by default without the need of these tricks
	unsigned is_explored;
	unsigned violation;
} region_t;

typedef struct _bug_t {
	double size;
	bool first; // to render the runs consistent in time we render the first spawned bug immortal
} bug_t;

void ProcessEvent(unsigned int me, simtime_t now, int event_type, agent_t *agent_p, unsigned int event_size, region_t *state) {

	unsigned i, j, dest, tries, directions;
	bug_t *this_bug;
	double consumption;
	size_t *bugs_count;
	agent_t this_agent;

	if(state != NULL)
		state->lvt = now;

	switch (event_type) {
		case INIT:
			(void)event_size;
			// standard stuff
			region_t *region = malloc(sizeof(region_t));

			SetState(region);

			region->is_explored = 0;
			region->last_bug_size = 0;
			region->food_available = RandomRange(0, MAX_FOOD_PRODUCTION_RATE);
			region->bugs = 0;
			region->violation = 0;
			region->lvt = 0;
			// here we do what I explained earlier
			TrackNeighbourInfo(&region->bugs);
			// for simplicity we spawn a single bug at region 0
			if(me < NUM_OCCUPIED_CELLS){
				// here we call ourselves
				ProcessEvent(me, 0, SPAWN_BUG, NULL, 0, region);
			}

			ScheduleNewEvent(me, now + TIME_STEP, PRODUCE_FOOD, NULL, 0);
			break;

		case BUG_DELAYED_VISIT:
			ScheduleNewEvent(me, now + 0.0001, BUG_VISIT, agent_p, sizeof(*agent_p));
			break;

		case BUG_VISIT:
			state->is_explored = 1;
			state->bugs++;

			this_bug = DataAgent(*agent_p, NULL);

			if(CountAgents() > BUG_PER_CELL) {
				state->violation++;
				KillAgent(*agent_p);
				break;
			}

			consumption = state->food_available > MAX_CONSUMPTION_RATE ? MAX_CONSUMPTION_RATE : state->food_available;

			this_bug->size += consumption;
			state->food_available -= consumption;
			if(state->food_available < 0)
				state->food_available = 0;

			state->last_bug_size = this_bug->size;

			if(this_bug->size >= REPRODUCTION_SIZE) {

				//reproduce
				for (i = 0; i < CHILD_COUNT; ++i) {
					// here we get the count of possible neighbours
					directions = DirectionsCount();
					// this is (for me) arbitrary: I extrapolated it from the old model
					tries = directions + 1;
					while (tries--) {
						// we select a direction, more on direction is explained in the topology section of ROOT-Sim.h
						j = RandomRange(0, directions - 1);
						// we have to check this call, because we may be asking for a neighbour of a border cell
						if(GetNeighbourInfo(j, &dest, (void **)&bugs_count) < 0)
							continue;
						// our requirement is to make sure we always have a bug threshold respected per region
						if(*bugs_count < BUG_PER_CELL) {
							ScheduleNewEvent(dest, now + (simtime_t) (TIME_STEP * Random()), SPAWN_BUG, NULL, 0);
							// this doesn't get remotely updated, it's just to keep track of currently scheduled children
							(*bugs_count)++;
							break;
						}
					}
				}
				// the father bug dies...
				KillAgent(*agent_p);
				state->bugs--;

			} else {
				ScheduleNewLeaveEvent(now + (simtime_t) (TIME_STEP * Random()), BUG_LEAVING, *agent_p);
			}
			break;

		case PRODUCE_FOOD:

			state->food_available += RandomRange(0, MAX_FOOD_PRODUCTION_RATE);

			ScheduleNewEvent(me, now + TIME_STEP, PRODUCE_FOOD, NULL, 0);

			break;

		case SPAWN_BUG:
			if(CountAgents() < BUG_PER_CELL) {
				// instantiate a new agent (I reuse the parameter variable just for simplicity)
				this_agent = SpawnAgent(sizeof(bug_t));
				// we initialize our custom fields
				this_bug = DataAgent(this_agent, NULL);
				this_bug->size = 1;
				this_bug->first = now <= 0.0;
				// we call ourselves to make the bug eat and eventually leave this region
				ProcessEvent(me, now, BUG_VISIT, &this_agent, sizeof(this_agent), state);
			}

			break;

		case BUG_LEAVING:
			// the bug wants to leave but he still hasn't got a destination and horrible things may happen to him

			state->bugs--; //one way or another we get rid of this bug (lulz a word pun!)

			if(RandomRange(0, 100) >= SURVIVAL_PROBABILITY) {
				this_bug = DataAgent(*agent_p, NULL);
				if(!this_bug->first){
					KillAgent(*agent_p); // :(
					break;
				}
			}

			// here we verify there is at least a suitable neighbour to visit before randomly picking one
			directions = DirectionsCount();
			for(i = 0; i < directions; ++i) {

				if(GetNeighbourInfo(i, &dest, (void**)&bugs_count) < 0)
					continue;

				if(*bugs_count < BUG_PER_CELL) {
					// since we just found a suitable neighbour we can be sure this cycle will eventually complete
					do{
						j = RandomRange(0, directions - 1);
					}while(GetNeighbourInfo(j, &dest, (void**)&bugs_count) < 0 || *bugs_count >= BUG_PER_CELL);
					// this is our planned visit
					EnqueueVisit(*agent_p, dest, BUG_DELAYED_VISIT);
					break;
				}
			}

			if(i >= directions){ // this bug tried and tried to exit without succeeding
				KillAgent(*agent_p);
			}
			break;

		case BUG_TRAVERSE: // we only schedule visits to neighbours, we shouldn't cross any "intermediate" region
			/* no break */
		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
			exit(EXIT_FAILURE);
	}

}

int OnGVT(unsigned int me, region_t *snapshot) {
	(void)me;

	return snapshot->is_explored;
}
