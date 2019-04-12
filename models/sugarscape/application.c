#include "../sugarscape/application.h"

#include <stdio.h>
#include <limits.h>


struct _topology_settings_t topology_settings = {.type = TOPOLOGY_OBSTACLES, .write_enabled = false, .default_geometry = TOPOLOGY_SQUARE};
struct _abm_settings_t abm_settings = {.neighbour_data_size = 2 * sizeof(unsigned), .traverse_handler = _TRAVERSE, .keep_history = false};


static unsigned init_capacity(unsigned lp_id){

	static const unsigned sugar_sources[] = {130, 400};

	unsigned unused_path[RegionsCount()];
	unsigned capacity = 0;
	unsigned i = sizeof(sugar_sources)/sizeof(unsigned);
	double distance;
	while(i--){
		if(lp_id == sugar_sources[i]){
			capacity = 4;
			break;
		}
		distance = ComputeMinTour(lp_id, sugar_sources[i], unused_path);

		if(distance < SOURCEBASERADIUS){
			capacity = 4;
			break;
		}
		if(distance < 2 * SOURCEBASERADIUS && capacity < 3)
			capacity = 3;

		if(distance < 3 * SOURCEBASERADIUS && capacity < 2)
			capacity = 2;

		if(distance < 4 * SOURCEBASERADIUS && capacity < 1)
			capacity = 1;
	}
	return capacity;
}

static void sugar_eater_new(unsigned me){
	agent_t agent = SpawnAgent(sizeof(sugar_eater_t));
	sugar_eater_t *sugar_eater = DataAgent(agent, NULL);
	sugar_eater->wealth = RandomRange(MIN_INITIAL_WEALTH, MAX_INITIAL_WEALTH);
	sugar_eater->eat_rate = RandomRange(MIN_EAT_RATE, MAX_EAT_RATE);
	sugar_eater->remaining_steps = RandomRange(MIN_MAX_AGE, MAX_MAX_AGE);
	ScheduleNewEvent(me, TIME_STEP -0.1 + Random()/5, SUGAR_VISIT, &agent, sizeof(agent));
}

static void sugar_eater_on_visit(agent_t agent, region_t *region, simtime_t now){
	sugar_eater_t *sugar_eater = DataAgent(agent, NULL);
	// get sugar
	sugar_eater->wealth += region->n.sugar;
	region->n.sugar = 0;
	// get older
	sugar_eater->remaining_steps--;
	// die :(
	if(sugar_eater->wealth == 0 || sugar_eater->wealth < sugar_eater->eat_rate || !sugar_eater->remaining_steps){
		KillAgent(agent);
		return;
	}
	// increment eaters count the region
	region->n.eaters++;
	// eat
	sugar_eater->wealth -= sugar_eater->eat_rate;
	// prepare to leave
	ScheduleNewLeaveEvent(now + TIME_STEP + Random()/5, SUGAR_LEAVE, agent);
}

static void sugar_eater_on_leave(agent_t agent, unsigned me, region_t *region){
	unsigned *info_p = NULL; // this points to 2 consecutive unsigned ints, the sugar and the eaters in the region respectively
	unsigned receiver = me;
	unsigned max_sugar = region->n.sugar;

	// get the max sugar available in the neighbourhood
	unsigned i = DirectionsCount();
	while(i--){
		if(	GetNeighbourInfo(i, &receiver, (void**)&info_p) != 1 &&
			info_p &&
			!info_p[1] &&
			info_p[0] > max_sugar)
			max_sugar = info_p[0];
	}
	// decrement eaters
	region->n.eaters--;
	// remain here if this is the richest region
	if(region->n.sugar == max_sugar){
		EnqueueVisit(agent, me, SUGAR_VISIT);
		return;
	}
	// else find a random one with max sugar and no eaters
	do{
		i = Random()*DirectionsCount();
	}
	while(GetNeighbourInfo(i, &receiver, (void**)&info_p) == -1 || info_p[1] || info_p[0] < max_sugar);

	EnqueueVisit(agent, receiver, SUGAR_VISIT);
}


void ProcessEvent(unsigned me, simtime_t now, int event_type, agent_t *agent_p, unsigned event_size, region_t *region) {

	(void) event_size;

	switch(event_type) {

		case INIT:

			region = malloc(sizeof(region_t));
			if(region == NULL){
				printf("Out of memory!\n");
				exit(EXIT_FAILURE);
			}
			SetState(region);

			region->capacity = init_capacity(me);
			region->n.sugar = region->capacity;
			region->n.eaters = 0;

			TrackNeighbourInfo(&(region->n));
			if(!me){
				unsigned i = INIT_EATERS;
				while(i--)
					ScheduleNewEvent(Random()*RegionsCount(), now+0.1, SUGAR_INIT, NULL, 0);
			}

			ScheduleNewEvent(me, now + TIME_STEP - 0.25 + Random()/5, SUGAR_REFILL, NULL, 0);

			break;

		case SUGAR_INIT:
			sugar_eater_new(me);
			break;

		case SUGAR_VISIT:
			sugar_eater_on_visit(*agent_p, region, now);
			break;

		case SUGAR_LEAVE:
			sugar_eater_on_leave(*agent_p, me, region);
			break;

		case SUGAR_REFILL:
			if(region->n.sugar < region->capacity)
				region->n.sugar++;

			ScheduleNewEvent(me, now + TIME_STEP - 0.25 + Random()/5, SUGAR_REFILL, NULL, 0);
			break;

		case _TRAVERSE:
      		default:
      			printf("Unsupported event!\n");
      			exit(EXIT_FAILURE);
			break;
	}
}

int OnGVT(unsigned int me, region_t *snapshot) {
	if(snapshot->n.eaters != 0)
		printf("%u: cap %u eaters %u sugar %u\n", me, snapshot->capacity, snapshot->n.eaters, snapshot->n.sugar);

	return (snapshot->n.eaters == 0);
}
