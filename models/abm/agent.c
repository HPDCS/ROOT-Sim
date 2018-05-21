#include "agent.h"
#include "user.h"
#include "logger.h"

static obstacles_t* a_obstacles;
static topology_t a_topology = TOPOLOGY_INVALID;
static unsigned int reg_actions_count = 0;
static const char* reg_actions_names[MAX_USER_ACTIONS] = { 0 };

static unsigned int valid_directions_count = 0;
static direction_t valid_directions[] = { DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_INVALID, DIRECTION_INVALID,
		DIRECTION_INVALID, DIRECTION_INVALID };

typedef struct _visit_t {
	simtime_t time;					//! The logical time in which the agent has visited the region
	unsigned int region;			//! The region subjected to the visit
	agent_action_t action;			//! The action the agent has done in the region
} visit_t;

struct _agent_t {
	unsigned long long uuid;		//! UUID that uniquely identifies the agent
	agent_t *next;					//! The next agent on the region's list, NULL if there isn't anyone
	agent_data_t a_data;			//! Custom user data (see user.h)
	size_t visited;					//! The number of visits in visit_list which are actually completed
	size_t visit_list_size;			//! The number of the visit path size
	visit_t visit_list[];			//! The set of completed, planned and desired visits to regions
};

struct _region_t {
	unsigned int lp_id;				//! The LP id which manages the region
	size_t num_agents;				//! The number of agents currently hosted in the region
	agent_t *agents;				//! The first agent of the list of agents residing in this region
	region_data_t r_data;			//! Custom user data
	struct _update_t {
		neighbour_state_t n_data;	//! The region data exposed to the neighbours
		unsigned int direction_i;//! Tells the recipient the sender of the update; also used to iterate over neighbours
	} update;						//! This is the message that regions will pass around neighbours
	neighbour_state_t neighbours[];	//! The neighbours information necessary to take local choices
};

int set_topology(topology_t topology) {
	if(valid_directions_count) {
		return -1;
	}

	switch (topology) {
		/**
		 * IMPORTANT: directions are arranged so that opposite directions are near each other.
		 * This is needed in order to efficiently find the index of the opposite direction
		 * of the direction found from a given index: this will be used in the neighbours update arrangement
		 */
		case TOPOLOGY_HEXAGON:
			valid_directions_count = 6;
			valid_directions[0] = DIRECTION_NE;
			valid_directions[1] = DIRECTION_SW;
			valid_directions[2] = DIRECTION_E;
			valid_directions[3] = DIRECTION_W;
			valid_directions[4] = DIRECTION_SE;
			valid_directions[5] = DIRECTION_NW;
			break;
		case TOPOLOGY_SQUARE:
			valid_directions_count = 4;
			valid_directions[0] = DIRECTION_N;
			valid_directions[1] = DIRECTION_S;
			valid_directions[2] = DIRECTION_E;
			valid_directions[3] = DIRECTION_W;
			break;
		default:
			return -1;
	}

	a_topology = topology;
	a_obstacles = NewObstacles();

	return 0;
}

int set_obstacle_region_id(unsigned int lp_id) {
	return AddObstacle(a_obstacles, lp_id);
}

bool is_obstacle_region_id(unsigned int lp_id) {
	return IsObstacle(a_obstacles, lp_id);
}

region_t* new_region(unsigned int lp_id) {
	if(lp_id >= n_prc_tot) {
		abm_log(ABM_LOG_WARN, "[LP %u] :: Invalid region, non existing LP", lp_id);
		return NULL;
	}
	region_t* state = malloc(sizeof(region_t) + sizeof(neighbour_state_t) * valid_directions_count);
	if(!state) {
		abm_log(ABM_LOG_ERROR, "[LP %u] :: Can't allocate memory in region initialization", lp_id);
		return NULL;
	}
	memset(state, 0, sizeof(region_t) + sizeof(neighbour_state_t) * valid_directions_count);
	state->lp_id = lp_id;

	return state;
}

bool is_obstacle_region(const region_t *region) {
	return is_obstacle_region_id(region->lp_id);
}

region_data_t* data_region(const region_t *region) {
	return (region_data_t*) (&region->r_data);
}

bool done_region(const region_t *region) {
	return user_done_region(region);
}

void free_region(region_t *region) {
	if(region == NULL)
		return;

	agent_t *agent = region->agents;
	while (agent) {
		free_agent(agent);
		agent = agent->next;
	}
	free(region);
}

agent_action_t register_action(const char *name) {
	unsigned int cnt;
	if(reg_actions_count >= MAX_USER_ACTIONS)
		return ACTION_INVALID;

	if(strcmp(ACTION_STR_TRAVERSE, name) == 0 || strcmp(ACTION_STR_START, name) == 0)
		return ACTION_INVALID;

	cnt = reg_actions_count;
	while (cnt--)
		if(strcmp(reg_actions_names[cnt], name) == 0)
			return ACTION_INVALID;

	reg_actions_names[reg_actions_count] = name;
	reg_actions_count++;
	return reg_actions_count - 1;
}

agent_action_t retrieve_action(const char *name, unsigned name_len) {
	unsigned int cnt = reg_actions_count;
	/* we intentionally do not check for traverse and start actions because
	 * the user is not allowed to specify those in the json agent description
	 */
	while (cnt--)
		if(strlen(reg_actions_names[cnt]) == name_len && strncmp(reg_actions_names[cnt], name, name_len) == 0)
			return cnt;

	return ACTION_INVALID;
}

const char* name_action(agent_action_t action) {

	if(action == ACTION_TRAVERSE)
		return ACTION_STR_TRAVERSE;

	if(action == ACTION_START)
		return ACTION_STR_START;

	if(reg_actions_count <= action)
		return NULL;

	return reg_actions_names[action];
}

agent_t *new_agent(unsigned int start_region_id, unsigned int dest_count, const unsigned int destinations[dest_count],
		const agent_action_t actions[dest_count]) {

	visit_t *visit_list;
	unsigned int list[n_prc_tot];
	unsigned int i;
	unsigned int steps = 0;

	/* SANITY CHECKS */
	if(start_region_id >= n_prc_tot) {
		abm_log(ABM_LOG_WARN, "[LP %u] :: Invalid region, non existing LP", start_region_id);
		return NULL;
	}

	i = dest_count;
	while (i--) {

		if(destinations[i] >= n_prc_tot) {
			abm_log(ABM_LOG_WARN, "[LP %u] :: Invalid destination (%u) found while initializing an agent",
					start_region_id, destinations[i]);
			return NULL;
		}

		if(actions[i] >= reg_actions_count) {
			abm_log(ABM_LOG_WARN, "[LP %u] :: Invalid action (%u) found while initializing an agent", start_region_id,
					actions[i]);
			return NULL;
		}

	}

	/* if we have destinations compute first part of the tour */
	if(dest_count) {
		steps = ComputeMinTour(list, a_obstacles, a_topology, start_region_id, destinations[0]);
		if(steps == UINT_MAX) {
			abm_log(ABM_LOG_WARN, "[LP %u] :: The first destination of an agent is unreachable", start_region_id);
			return NULL;
		}
		// We aren't interested in copying the arrival cell, since we already know it
		steps--;
		//print_tour_grid(a_topology, a_obstacles, steps, list, start_region_id, destinations[0]);
	}

	agent_t *agent = malloc(sizeof(agent_t) + sizeof(visit_t) * (dest_count + steps + 1)); // the 1 is for the starting region
	if(!agent) {
		abm_log(ABM_LOG_ERROR, "[LP %u] :: Can't allocate memory in agent initialization", start_region_id);
		return NULL;
	}

	memset(agent, 0, sizeof(agent_t));
	agent->uuid = GenerateUniqueId();
	agent->visit_list_size = dest_count + steps + 1;
	agent->visited = 0;

	visit_list = agent->visit_list;
	visit_list[0].time = 0;
	visit_list[0].region = start_region_id;
	visit_list[0].action = ACTION_START;

// this saves some sums
	visit_list++;

	for (i = 0; i < steps; ++i) {
		visit_list[i].time = INFTY;
		visit_list[i].region = list[i];
		visit_list[i].action = ACTION_TRAVERSE;
	}

	for (i = 0; i < dest_count; ++i) {
		visit_list[i + steps].time = INFTY;
		visit_list[i + steps].region = destinations[i];
		visit_list[i + steps].action = actions[i];
	}
	return agent;
}

void free_agent(agent_t *agent) {
	free(agent);
}

unsigned long long uuid_agent(const agent_t *agent) {
	return agent->uuid;
}

size_t size_agent(const agent_t *agent) {
	return sizeof(agent_t) + sizeof(visit_t) * agent->visit_list_size;
}

agent_action_t current_action_agent(const agent_t *agent) {
	return (agent->visit_list[agent->visited - 1].action);
}

unsigned int current_region_agent(const agent_t *agent) {
	return (agent->visit_list[agent->visited - 1].region);
}

unsigned int next_region_agent(const agent_t *agent) {
	if(agent->visited >= agent->visit_list_size)
		return UINT_MAX;
	return (agent->visit_list[agent->visited].region);
}

unsigned int current_destination_agent(const agent_t *agent) {
	unsigned i;
	const visit_t *visit_list = agent->visit_list;
	for (i = agent->visited; i < agent->visit_list_size; i++)
		if(visit_list[i].action != ACTION_TRAVERSE)
			return visit_list[i].region;

	return UINT_MAX;
}

simtime_t residence_time_region_agent(region_t *region, agent_t *agent, simtime_t now) {
	return user_residence_time(region, agent, now);
}

agent_data_t* data_agent(const agent_t *agent) {
	return (agent_data_t*) (&(agent->a_data));
}

bool is_chilling_agent(const agent_t *agent) {
	return agent->visited >= agent->visit_list_size;
}

static int add_agent_region(region_t *region, agent_t *agent) {

// Link the new agent node to the linked list of the
// simulation state.
	agent->next = region->agents;
	region->agents = agent;
	region->num_agents++;
	return 0;
}

static agent_t* remove_agent_by_id_region(region_t *region, unsigned long long uuid) {
	agent_t *prev_agent, *agent;

	if(!region->agents)
		return NULL;

	prev_agent = region->agents;
	if(prev_agent->uuid == uuid) {
		region->agents = prev_agent->next;
		prev_agent->next = NULL;
		region->num_agents--;
		return prev_agent;
	}
// Look for the agent identified by the provided uuid
	agent = prev_agent->next;
	while (agent != NULL) {
		if(agent->uuid == uuid) {
			// and unlink it
			prev_agent->next = agent->next;
			agent->next = NULL;
			region->num_agents--;
			break;
		}
		prev_agent = agent;
		agent = agent->next;
	}
	return agent;
}

unsigned int compute_path_agent(agent_t **agent_p, region_t *region) {
	unsigned int steps;
	unsigned int list[n_prc_tot];
	agent_t *new_agent, *agent_aux;
	visit_t *new_visit_list;

	/* SANITY CHECKS */
	if(current_region_agent(*agent_p) != region->lp_id) {
		abm_log(ABM_LOG_WARN, "[LP %u] :: Trying to recompute an agent %llu's tour from the wrong region",
				region->lp_id, uuid_agent(*agent_p));
		return UINT_MAX;
	}
	if(is_chilling_agent(*agent_p)) {
		abm_log(ABM_LOG_WARN, "[LP %u] :: Agent %llu has completed his mission, can't send him around just now",
				region->lp_id, uuid_agent(*agent_p));
		return UINT_MAX;
	}

	if(region->lp_id == current_destination_agent(*agent_p) || /* if the destination we are seeking is the current region we don't need to do anything */
	(current_action_agent(*agent_p) == ACTION_TRAVERSE)) /* or if we are traversing an already planned tour we also don't need to do anything */
		return 0;

	/* compute next tour */
	steps = ComputeMinTour(list, a_obstacles, a_topology, region->lp_id, current_destination_agent(*agent_p));
	if(steps == UINT_MAX) {
		abm_log(ABM_LOG_WARN, "[LP %u] :: Current destination '%u' of agent %llu is unreachable", region->lp_id,
				current_destination_agent(*agent_p), uuid_agent(*agent_p));
		return UINT_MAX;
	}

// We aren't interested in copying the arrival cell, since it already appears in the agent's visit list
	steps--;

	if(!steps)
		// there's no need to do all these gymnastics, the destination is adjacent to the current region
		return 1;

// Allocate a new agent
	new_agent = malloc(sizeof(agent_t) + sizeof(visit_t) * ((*agent_p)->visit_list_size + steps));
	if(new_agent == NULL) {
		abm_log(ABM_LOG_ERROR, "Can't allocate memory in agent %llu recomputation", uuid_agent(*agent_p));
		return UINT_MAX;
	}

// Copy the first visited part of the path
	memcpy(new_agent, *agent_p, sizeof(agent_t) + sizeof(visit_t) * ((*agent_p)->visited));

// Build the path of visit_t list
	new_visit_list = new_agent->visit_list + new_agent->visited;
	for (unsigned int i = 0; i < steps; ++i) {
		new_visit_list[i].time = INFTY;
		new_visit_list[i].region = list[i];
		new_visit_list[i].action = ACTION_TRAVERSE;
	}

// Copy the remaining part of the visit list
	memcpy(new_visit_list + steps, (*agent_p)->visit_list + new_agent->visited,
			sizeof(visit_t) * (new_agent->visit_list_size - new_agent->visited));

// Refresh visit_list_size
	new_agent->visit_list_size += steps;

// Check if old agent is in region's list
	agent_aux = remove_agent_by_id_region(region, uuid_agent(*agent_p));
	// if it's in it we have to swap it with the new agent
	if(agent_aux)
		add_agent_region(region, new_agent);
// Free the old agent's descriptor
	free(*agent_p);

// Return the new agent's descriptor
	*agent_p = new_agent;

	return steps + 1;
}

int visit_agent_region(region_t *region, agent_t *agent, simtime_t now) {
	if(next_region_agent(agent) != region->lp_id || is_obstacle_region(region))
		return -1;

// Link the new agent node to the linked list of the
// simulation state.
	add_agent_region(region, agent);
	agent->visit_list[agent->visited].time = now;
	agent->visited++;

	return user_on_visit(region, agent, now);
}

agent_t* find_agent_by_id_region(region_t *region, unsigned long long uuid) {
	agent_t *agent;

// Look for the agent identified by the provided uuid
	agent = region->agents;
	while (agent != NULL) {
		if(agent->uuid == uuid)
			return agent;

		agent = agent->next;
	}

	return NULL;
}

agent_t* leave_agent_by_id_region(region_t *region, unsigned long long uuid, simtime_t now) {
	agent_t *agent = remove_agent_by_id_region(region, uuid);
	if(agent && user_on_leave(region, agent, now) < 0)
		return NULL;
	return agent;
}

size_t count_agent_region(const region_t *region) {
	return region->num_agents;
}

bool iterate_agent_region(region_t *region, agent_t **agent_p) {
	if(!(*agent_p)) {
		*agent_p = region->agents;
		return *agent_p != NULL;
	}
	*agent_p = (*agent_p)->next;
	return *agent_p != NULL;
}

bool iterate_c_agent_region(const region_t *region, const agent_t **agent_p) {
	if(!(*agent_p)) {
		*agent_p = region->agents;
		return *agent_p != NULL;
	}
	*agent_p = (*agent_p)->next;
	return *agent_p != NULL;
}

void print_region(region_t *state) {
	size_t agents_cnt = 20;

	printf("\n\x1b[36m");
	while (agents_cnt--)
		printf("####");

	agents_cnt = state->num_agents;
	printf("\n[LP %u] :: Region %p is hosting %zu agent%s. User data:\n\x1b[0m", state->lp_id, state, agents_cnt,
			agents_cnt != 1 ? "s" : "");

	user_print_region(state, stdout);

	agent_t *agent = state->agents;
	while (agents_cnt--) {
		printf("\x1b[34m[LP %u] :: Agent %llu (%p) is here as well. User data:\n\x1b[0m", state->lp_id, agent->uuid,
				agent);

		user_print_agent(agent, stdout);

		agent = agent->next;
	}

	agents_cnt = 20;
	printf("\x1b[36m");
	while (agents_cnt--)
		printf("####");
	printf("\n\x1b[0m\n");

}

/**
 * TODO: this is the neighbours state update part.
 * This part more than others needs refinement and work
 */

bool iterate_neighbour_data_region(const region_t *region, const neighbour_state_t **neighbour_p,
		unsigned int *neighbour_id, direction_t *direction) {
	unsigned int i, n_id;
	if(*neighbour_p == NULL) {
		i = 0;
	} else {
		i = region->update.direction_i;
	}
	while ((n_id = GetReceiver(a_topology, region->lp_id, valid_directions[i])) == DIRECTION_INVALID) {
		i++;

	}
	if(i >= valid_directions_count)
		return false;

	((region_t*) region)->update.direction_i = i;
	*neighbour_id = n_id;
	*direction = valid_directions[i];
	*neighbour_p = &(region->neighbours[i]);

	return true;
}

bool need_refresh_region(region_t *region, simtime_t now) {
	neighbour_state_t new_n_data = region->update.n_data;
	user_compile_neighbour_state(region, &(region->update.n_data), now);
	return memcmp(&(region->update.n_data), &new_n_data, sizeof(neighbour_state_t)) != 0;
}

size_t size_update_region(void) {
	return sizeof(struct _update_t);
}

bool iterate_neighbour_update_region(region_t *region, unsigned int *neighbour_id, void **update) {
	unsigned int i, n_id;
	if(*update == NULL) {
		i = 0;
	} else {
		i = region->update.direction_i;
	}
	while ((n_id = GetReceiver(a_topology, region->lp_id, valid_directions[i])) == DIRECTION_INVALID) {
		i++;

	}
	if(i >= valid_directions_count)
		return false;

	// we leverage the fact that opposite directions are near each other
	region->update.direction_i = i % 2 ? i - 1 : i + 1;
	*update = &(region->update);
	*neighbour_id = n_id;

	return true;
}

void apply_update_region(region_t *region, void* update) {
	struct _update_t *upd = update;
	memcpy(&(region->neighbours[upd->direction_i]), update, size_update_region());
}
