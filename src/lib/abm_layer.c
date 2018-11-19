/**
 *			Copyright (C) 2008-2018 HPDCS Group
 *			http://www.dis.uniroma1.it/~hpdcs
 *
 *
 * This file is part of ROOT-Sim (ROme OpTimistic Simulator).
 *
 * ROOT-Sim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; only version 3 of the License applies.
 *
 * ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @file abm_layer.c
 * @brief This file contains the implementation of the
 *        the agent based modeling interface offered by ROOT-Sim
 * @author Andrea Piccione
 * @date 16/06/2018
 */

#include <ROOT-Sim.h>
#include <lib/abm_layer.h>

#include <core/init.h>			// for serial execution check
#include <scheduler/scheduler.h> 	// for current_lp and current_state
#include <lib/topology.h>
#include <datatypes/array.h>
#include <datatypes/hash_map.h>

#define CURRENT_LP_ID (gid_to_int(LidToGid(current_lp)))

#define ACTION_START INIT

#define ABM_DEBUG

// my assert implementation still evaluates syntactically the condition even when disabled
#ifdef ABM_DEBUG
#define abm_assert(cond) do{ if(!(cond)) rootsim_error(true, "This shouldn't have happened, report to maintainer (file %s, line %u). Exiting!", __FILE__, __LINE__);} while(0)
#else
#define abm_assert(cond) (void)(cond)
#endif

#define CURRENT_REGION (LPS(current_lp)->region)

struct _visit_abm_t{
	unsigned region;
	unsigned action;
	simtime_t time;
};

struct _region_abm_t {
	rootsim_hash_map_t agents;
	unsigned char *published_data;
	unsigned char *tracked_data;
	unsigned neighbours_count;
	struct _n_info_t{
		void *data;
		unsigned int src_lp;
	} neighbours_info[];
};

struct _agent_abm_t {
	unsigned long long uuid;		//! UUID that uniquely identifies the agent
	rootsim_array(struct _visit_abm_t) past;
	rootsim_array(struct _visit_abm_t) future;
	simtime_t leave_time;
	unsigned int leave_event;
	unsigned user_data_size;
	char user_data[];
};

static unsigned agent_dump_size(const agent_abm_t *agent) {
	return (unsigned)(sizeof(agent->uuid) + sizeof(agent->user_data_size) + agent->user_data_size
			+ array_required_bytes_dump(agent->future) + array_required_bytes_dump(agent->past));
}

static agent_abm_t* agent_from_buffer(const unsigned char* event_content, unsigned event_size){
	// XXX we can simply move around some agent struct members (the arrays) to limit the memcpy() calls to 1 or 2
	// keep track of original pointer
	const unsigned char *buffer = event_content;
	// allocate the memory for the visiting agent
	agent_abm_t *agent = __wrap_malloc(sizeof(agent_abm_t) + ((const agent_abm_t *)buffer)->user_data_size);
	// copy uuid
	memcpy(&agent->uuid, buffer, sizeof(agent->uuid));
	buffer += sizeof(agent->uuid);
	// copy user data size
	memcpy(&agent->user_data_size, buffer, sizeof(agent->user_data_size));
	buffer += sizeof(agent->user_data_size);
	// copy user data
	memcpy(agent->user_data, buffer, agent->user_data_size);
	buffer += agent->user_data_size;
	// load the arrays
	array_load(agent->future, buffer);
	array_load(agent->past, buffer);
	//sanity check
	abm_assert(buffer == event_content + event_size);
	// officially insert the agent into the region (this isn't needed if we passed the agent by pointer)
	return agent;
}

static unsigned char* agent_to_buffer(agent_abm_t *agent, unsigned *ret_size){
	//we are going into another region: let's calculate the buffer size
	unsigned buffer_size = agent_dump_size(agent);
	// XXX this copy isn't actually needed, we just need to pack the messages ourselves!!!
	unsigned char *buffer = __wrap_malloc(buffer_size);
	// we use buffer as a mobile pointer so we need to save the original value
	unsigned char *to_send = buffer;
	// copying relevant info into buffer
	// copy uuid
	memcpy(buffer, &agent->uuid, sizeof(agent->uuid));
	buffer += sizeof(agent->uuid);
	// copy agent data size
	memcpy(buffer, &agent->user_data_size, sizeof(agent->user_data_size));
	buffer += sizeof(agent->user_data_size);
	// copy actual agent data
	memcpy(buffer, agent->user_data, agent->user_data_size);
	buffer += agent->user_data_size;
	// dumping the arrays
	array_dump(agent->future, buffer);
	array_dump(agent->past, buffer);
	// sanity check
	abm_assert(buffer == to_send + buffer_size);
	// return the stuff
	*ret_size = buffer_size;
	return to_send;
}

void abm_layer_init(void) {

	unsigned actual_neighbours, i;
	unsigned char *data;
	region_abm_t *region;

	// settings_abm is a weak symbol, we check for its existence
	if(!&abm_settings){
		return;
	}
	// we still do not support serial execution :(
	if(rootsim_config.serial){
		rootsim_error(true, "[LP %u] :: " "Serial execution is still unsupported...", CURRENT_LP_ID);
		return;
	}

	const unsigned neighbours = NeighboursCount();
	// the ABM API needs an underlying topology for meaningful operations!!!
	if(neighbours == UINT_MAX){
		rootsim_error(true, "[LP %u] :: " "Topology has not been initialized", CURRENT_LP_ID);
		return;
	}

	// we count valid neighbours (this can be relevant in huge graphs with low average fan-out instantiated with the custom API)
	actual_neighbours = ActualNeighboursCount();

	// instantiates region struct
	region = __wrap_malloc(sizeof(region_abm_t) + (sizeof(struct _n_info_t) * neighbours) + (abm_settings.neighbour_data_size * (actual_neighbours + 1)));
	// memory layout is as follows:
	// BASE REGION STRUCT | ARRAY OF NEIGHBOUR INFOS | PUBLISHED DATA BY SELF | ARRAY OF PUBLISHED DATA BY OTHER REGIONS

	// save neighbours count
	region->neighbours_count = neighbours;
	// helper pointer to easier assignment of memory regions
	data = (unsigned char*)&region->neighbours_info[neighbours];
	// set neighbours data regions to 0
	memset(data, 0, abm_settings.neighbour_data_size * (actual_neighbours + 1));

	// the nearest block is used to keep track of published data
	region->published_data = data;
	// here we assign a memory region only to valid neighbours
	for(i = 0; i < neighbours; ++i){
		if((region->neighbours_info[i].src_lp = GetReceiver(CURRENT_LP_ID, i)) == DIRECTION_INVALID){
			region->neighbours_info[i].data = NULL;
		}else{
			data += abm_settings.neighbour_data_size;
			region->neighbours_info[i].data = data;
		}
	}
	// default
	region->tracked_data = NULL;
	// Save pointer in LP state
	CURRENT_REGION = region;
	// initialize the hash_map for agents
	hash_map_init(&region->agents);
}

static void on_abm_visit(void){
	struct _visit_abm_t vis;
	// parse the entering agent
	agent_abm_t *agent = agent_from_buffer(current_evt->event_content, current_evt->size);
	// add him to the region
	hash_map_add(&(CURRENT_REGION->agents), (hash_map_pair_t *)agent);

	if(array_empty(agent->future) || array_get_at(agent->future, 0).region != CURRENT_LP_ID){
		// this is an intermediate objective to reach the next destination
		vis.action = abm_settings.traverse_handler;
		vis.region = CURRENT_LP_ID;
	} else {
		vis = array_remove_at(agent->future, 0);
	}
	if(abm_settings.keep_history){
		vis.time = current_evt->timestamp;
		// move the visit into the past ones
		array_push(agent->past, vis);
	}

	// seems all ok: user, do whatever you want now
	ProcessEvent[lid_to_int(current_lp)](CURRENT_LP_ID, current_evt->timestamp, vis.action, agent, 0, current_state);
}

static void on_abm_leave(void){
	struct _visit_abm_t vis;
	unsigned char* to_send;
	unsigned buffer_size;
	// we search for the agent who's leaving
	agent_abm_t *agent = (agent_abm_t *)hash_map_lookup(&(CURRENT_REGION->agents), *((unsigned long long*)current_evt->event_content));
	if(!agent || agent->leave_time > current_evt->timestamp) {
		// the exiting agent has been killed or already left or the agent is trying to leave too early (can happen if user decides so)
		return; // since this is spurious we can directly return
	}
	// seems all ok: user, do whatever you want now
	ProcessEvent[lid_to_int(current_lp)](CURRENT_LP_ID, current_evt->timestamp, agent->leave_event, agent, 0, current_state);
	// we search again for the agent who's leaving (the user could have possibly killed him)
	agent = (agent_abm_t *)hash_map_lookup(&(CURRENT_REGION->agents), *((unsigned long long*)current_evt->event_content));
	if(!agent || agent->leave_time > current_evt->timestamp){
		// the capricious user has decided against the agent departure
		return;
	}
	// well, the agent is actually leaving after all...
	unsigned next_hop = UINT_MAX;
	switch(topology_settings.type){
		case TOPOLOGY_OBSTACLES:
			// fixme: the user may want to use binary topologies with wandering agents
		case TOPOLOGY_COSTS:
			// if we have no set destination no point in selecting a next hop
			if(!array_empty(agent->future)){
				if(array_peek(agent->future).region == CURRENT_LP_ID){
					// we don't need to move
					next_hop = CURRENT_LP_ID;
				}else{
					// we get one step closer to the next destination
					next_hop = FindReceiverToward(array_peek(agent->future).region);
				}
			}
			break;
		case TOPOLOGY_PROBABILITIES:
			// in topology probabilities the agents just wander
			next_hop = FindReceiver();
			break;
		default:
			rootsim_error(true, "unsupported");
	}

	if(next_hop == UINT_MAX){
		// TODO communicate the user about the failed leave, maybe call user's ProcessEvent()
		// with a proper event type, this can happen legitimately (for example the agents is
		// surrounded by obstacles regions)
		return;
	}

	if(CURRENT_LP_ID == next_hop) {
		// if the next chosen destination is the very same region we are already in
		// we simply call ProcessEvent() again
		if(array_empty(agent->future) || array_peek(agent->future).region != CURRENT_LP_ID){
			vis.region = CURRENT_LP_ID;
			vis.action = abm_settings.traverse_handler;
		}else{
			// we remove the current visit
			vis = array_remove_at(agent->future, 0);
		}
		if(abm_settings.keep_history){
			// set the visit time
			vis.time = current_evt->timestamp;
			// we move the visit into the past ones
			array_push(agent->past, vis);
		}
		// seems all ok: user, do whatever you want now
		ProcessEvent[lid_to_int(current_lp)](CURRENT_LP_ID, current_evt->timestamp, vis.action, agent, 0, current_state);
	} else {
		to_send = agent_to_buffer(agent, &buffer_size);
		// finally we schedule the agent
		UncheckedScheduleNewEvent(next_hop, current_evt->timestamp, ABM_VISITING, to_send, buffer_size);
		// now we can get rid of it
		KillAgent(agent);
		// remember to free that stuff if we mallocated it!
		__wrap_free(to_send);
	}
}

static void receive_update(void){
	if(abm_settings.neighbour_data_size != current_evt->size)
		rootsim_error(true, "Misuse of ABM api, regions do not agree on neighbours info size! EXITING!");

	region_abm_t *region = CURRENT_REGION;
	unsigned i = region->neighbours_count;
	while(i--){// FIXME linear search, maybe sort them at init for log search or sort by direction for constant indexing
		if(region->neighbours_info[i].src_lp == gid_to_int(current_evt->sender)){
			memcpy(region->neighbours_info[i].data, current_evt->event_content, abm_settings.neighbour_data_size);
			return;
		}
	}
}

static void update_neighbours(void){
	region_abm_t *region = CURRENT_REGION;
	// we check whether we need to update our neighbours about some changes in the tracked data
	if(!region->tracked_data || !memcmp(region->published_data, region->tracked_data, abm_settings.neighbour_data_size))
		return;

	// copy the new data into the tracked one
	memcpy(region->published_data, region->tracked_data, abm_settings.neighbour_data_size);

	// let's propagate the changes to other regions too
	unsigned i = region->neighbours_count;
	while(i--){
		if(region->neighbours_info[i].src_lp == DIRECTION_INVALID)
			continue;
		UncheckedScheduleNewEvent(region->neighbours_info[i].src_lp, current_evt->timestamp, ABM_UPDATE, region->published_data, abm_settings.neighbour_data_size);
	}
}

void ProcessEventABM(void) {
	switch (current_evt->type) {

		case ABM_VISITING:
			switch_to_application_mode();
			on_abm_visit();
			switch_to_platform_mode();
			break;

		case ABM_LEAVING:
			switch_to_application_mode();
			on_abm_leave();
			switch_to_platform_mode();
			break;

		case ABM_UPDATE:
			receive_update();
			// we didn't give control to the user, no need to check changes in the
			// neighbours infos memory.
			return;
		case INIT:
			topology_init();
			abm_layer_init();
			switch_to_application_mode();
			ProcessEvent[lid_to_int(current_lp)](CURRENT_LP_ID, current_evt->timestamp,current_evt->type, current_evt->event_content, current_evt->size, current_state);
			switch_to_platform_mode();
			break;
		default:
			// uninteresting stuff, do as we don't exist
			ProcessEventTopology();
			break;

	}

	update_neighbours();
}

unsigned CountNeighbourInfos(void) {
	return CURRENT_REGION->neighbours_count;
}

int GetNeighbourInfo(direction_t i, unsigned int *region_id, void **data_p){
	if(i >= CURRENT_REGION->neighbours_count || CURRENT_REGION->neighbours_info[i].src_lp == DIRECTION_INVALID)
		return -1;

	*region_id = CURRENT_REGION->neighbours_info[i].src_lp;
	*data_p = CURRENT_REGION->neighbours_info[i].data;
	return 0;
}

void TrackNeighbourInfo(void *neighbour_data) {
	(CURRENT_REGION)->tracked_data = neighbour_data;
}

bool IterAgents(agent_abm_t **agent_p) {
	switch_to_platform_mode();
	static __thread map_size_t closure = 0;
	if(!*agent_p) {
		closure = 0;
	}
	*agent_p = (agent_abm_t *)hash_map_iter(&(CURRENT_REGION->agents), &closure);
	switch_to_application_mode();
	return *agent_p != NULL;
}

unsigned CountAgents(void) {
	return hash_map_count(&(CURRENT_REGION->agents));
}

agent_abm_t* SpawnAgent(unsigned user_data_size) {
	switch_to_platform_mode();
	// new agent
	agent_abm_t *ret = __wrap_malloc(sizeof(agent_abm_t) + user_data_size);

	array_init(ret->future);
	array_init(ret->past);

	ret->user_data_size = user_data_size;

	ret->uuid = generate_mark(current_lp);

	hash_map_add(&(CURRENT_REGION->agents), (hash_map_pair_t *)ret);
	// we register the visit to THIS region
	if(abm_settings.keep_history){
		struct _visit_abm_t start_visit = { CURRENT_LP_ID, ACTION_START, current_evt->timestamp };
		array_push(ret->past, start_visit);
	}
	switch_to_application_mode();
	return ret;
}

int KillAgent(agent_abm_t* agent) {
	if(!agent || hash_map_remove(&(CURRENT_REGION->agents), agent->uuid) != (hash_map_pair_t *)agent)
		return -1;

	array_fini(agent->future);
	array_fini(agent->past);
	__wrap_free(agent);

	return 0;
}

void* DataAgent(agent_abm_t *agent, unsigned *data_size_p){
	if(data_size_p)
		*data_size_p = agent->user_data_size;
	return agent->user_data_size ? &agent->user_data[0] : NULL;
}

unsigned long long IdAgent(const agent_abm_t *agent) {
	return agent->uuid;
}

agent_abm_t* FindAgent(unsigned long long agent_id) {
	return (agent_abm_t *)hash_map_lookup(&(CURRENT_REGION->agents), agent_id);
}

void ScheduleNewLeaveEvent(simtime_t time, unsigned int event_type, agent_abm_t *agent) {
	switch_to_platform_mode();
	// we mark the agent with the intended leave time so we can later compare it
	// to check for spurious events
	agent->leave_time = time;
	agent->leave_event = event_type;
	// we schedule the departure, the user can call this multiple time,
	// but we can distinguish the last call thanks to the work we did before
	// todo: directly remove from the msg queue the spurious leave events
	UncheckedScheduleNewEvent(CURRENT_LP_ID, time, ABM_LEAVING, &agent->uuid, sizeof(agent->uuid));
	switch_to_application_mode();
}

unsigned CountPastVisits(const agent_abm_t *agent){
	return array_count(agent->past);
}

int GetPastVisit(const agent_abm_t *agent, unsigned *region_p, unsigned *event_type_p, simtime_t *time_p, unsigned i){
	if(i >= CountPastVisits(agent))
		return -1;
	// we count backward to map the nearest event in the past to index 0
	*region_p = array_get_at(agent->past, array_count(agent->past) - 1 - i).region;
	*event_type_p = array_get_at(agent->past, array_count(agent->past) - 1 - i).action;
	*time_p = array_get_at(agent->past, array_count(agent->past) - 1 - i).time;
	return 0;
}

unsigned CountVisits(const agent_abm_t *agent){
	return array_count(agent->future);
}

int GetVisit(const agent_abm_t *agent, unsigned *region_p, unsigned *event_type_p, unsigned i){
	if(i >= CountVisits(agent))
		return -1;

	*region_p = array_get_at(agent->future, i).region;
	*event_type_p = array_get_at(agent->future, i).action;
	return 0;
}

int SetVisit(const agent_abm_t *agent, unsigned region, unsigned event_type, unsigned i){
	if(i >= array_count(agent->future) || event_type == ACTION_START || region >= RegionsCount()) {
		return -1;
	}

	array_items(agent->future)[i].region = region;
	array_items(agent->future)[i].action = event_type;
	return 0;
}

int EnqueueVisit(agent_abm_t *agent, unsigned region, unsigned event_type){
	if(event_type == ACTION_START || region >= RegionsCount()) {
		return -1;
	}

	struct _visit_abm_t visit = {region, event_type, INFINITY};
	array_push(agent->future, visit);

	return 0;
}

int AddVisit(agent_abm_t *agent, unsigned region, unsigned event_type, unsigned i){
	if(i > array_count(agent->future) || event_type == ACTION_START || region >= RegionsCount()) {
		return -1;
	}

	struct _visit_abm_t visit = {region, event_type, INFINITY};

	if(i == array_count(agent->future))
		array_push(agent->future, visit);
	else
		array_add_at(agent->future, i, visit);

	return 0;
}

int RemoveVisit(agent_abm_t *agent, unsigned i) {
	if(i >= array_count(agent->future)) {
		return -1;
	}

	array_remove_at(agent->future, i);

	return 0;
}
