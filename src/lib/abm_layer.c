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

#define ACTION_START INIT

#define retrieve_agent(agent_id) ({ \
	struct _agent_abm_t *__ret = (struct _agent_abm_t *)hash_map_lookup(&(current->region->agents_table), agent_id); \
	if(unlikely(!__ret)) \
		rootsim_error(true, "Looking for non existing agent id!"); \
	__ret; \
})

struct _visit_abm_t{
	unsigned region;
	unsigned action;
	simtime_t time;
};

struct _agent_abm_t {
	unsigned long long uuid;		//! UUID that uniquely identifies the agent
	unsigned user_data_size;
	unsigned leave_event;
	char *user_data;
	simtime_t leave_time;
	rootsim_array(struct _visit_abm_t) future;
	rootsim_array(struct _visit_abm_t) past;
};

struct _region_abm_t {
	rootsim_hash_map_t agents_table;
	rootsim_array(struct _agent_abm_t) agents_instances;
	rootsim_array(unsigned long long) agents_leaving;
	size_t published_data_offset;
	unsigned char *tracked_data;
	unsigned chkp_size;
	unsigned neighbours_count;
	struct _n_info_t{
		size_t data_offset;
		unsigned int src_lp;
	} neighbours_info[];
};


static unsigned agent_dump_size(const struct _agent_abm_t *agent) {
	return (unsigned)(sizeof(agent->uuid) + sizeof(agent->user_data_size) + agent->user_data_size
			+ array_required_bytes_dump(agent->future) + abm_settings.keep_history * array_required_bytes_dump(agent->past));
}

static struct _agent_abm_t* agent_from_buffer(const unsigned char* event_content, unsigned event_size){
	// keep track of original pointer
	const unsigned char *buffer = event_content;
	// allocate the memory for the visiting agent
	struct _agent_abm_t *agent = array_reserve(current->region->agents_instances, 1);
	// copy uuid and user data size
	memcpy(&agent->user_data_size, buffer, sizeof(agent->user_data_size) + sizeof(agent->uuid));
	buffer += sizeof(agent->user_data_size) + sizeof(agent->uuid);
	// copy user data
	memcpy(agent->user_data, buffer, agent->user_data_size);
	buffer += agent->user_data_size;
	// load the arrays
	array_load(agent->future, buffer);
	if(abm_settings.keep_history)
		array_load(agent->past, buffer);
	//sanity check
	assert(buffer == event_content + event_size);
	return agent;
}

static unsigned char* agent_to_buffer(struct _agent_abm_t *agent, unsigned *ret_size){
	//we are going into another region: let's calculate the buffer size
	unsigned buffer_size = agent_dump_size(agent);
	// XXX this copy isn't actually needed, we just need to pack the messages ourselves!!!
	unsigned char *buffer = rsalloc(buffer_size);
	// we use buffer as a mobile pointer so we need to save the original value
	unsigned char *to_send = buffer;
	// copy relevant fields
	memcpy(buffer, &agent->user_data_size, sizeof(agent->user_data_size) + sizeof(agent->uuid) + agent->user_data_size);
	buffer += sizeof(agent->user_data_size) + sizeof(agent->uuid) + agent->user_data_size;
	// dumping the arrays
	array_dump(agent->future, buffer);
	if(abm_settings.keep_history)
		array_dump(agent->past, buffer);
	// sanity check
	assert(buffer == to_send + buffer_size);
	// return the stuff
	*ret_size = buffer_size;
	return to_send;
}

void abm_layer_init(void) {

	unsigned actual_neighbours, i;
	size_t data_offset;
	region_abm_t *region;

	// settings_abm is a weak symbol, we check for its existence
	if(!&abm_settings){
		return;
	}

	rootsim_error(true, "The abm layer is currently undergoing heavy refactor!");

	// we still do not support serial execution :(
	if(rootsim_config.serial){
		rootsim_error(true, "[LP %u] :: " "Serial execution is still unsupported...", current->gid.to_int);
		return;
	}

	const unsigned directions = DirectionsCount();
	// the ABM API needs an underlying topology for meaningful operations!!!
	if(!directions){
		rootsim_error(true, "[LP %u] :: " "Topology has not been initialized", current->gid.to_int);
		return;
	}

	// we count valid neighbours (this can be relevant in huge graphs with low average fan-out instantiated with the custom API)
	actual_neighbours = NeighboursCount();

	unsigned region_alloc_size = (unsigned)(sizeof(region_abm_t) + (sizeof(struct _n_info_t) * directions) + (abm_settings.neighbour_data_size * (actual_neighbours + 1)));
	// instantiates region struct
	region = rsalloc(region_alloc_size);
	// memory layout is as follows:
	// BASE REGION STRUCT | ARRAY OF NEIGHBOUR INFOS | PUBLISHED DATA BY SELF | ARRAY OF PUBLISHED DATA BY OTHER REGIONS
	region->chkp_size = region_alloc_size;
	// save neighbours count
	region->neighbours_count = actual_neighbours;
	// helper pointer to easier assignment of memory regions
	data_offset = (unsigned char *)&region->neighbours_info[actual_neighbours] - (unsigned char *)region;
	// set neighbours data regions to 0
	memset((unsigned char *)region + data_offset, 0, abm_settings.neighbour_data_size * (actual_neighbours + 1));

	// the nearest block is used to keep track of published data
	region->published_data_offset = data_offset;
	// here we assign a memory region only to valid neighbours
	for(i = 0; i < directions; ++i){
		if((region->neighbours_info[i].src_lp = GetReceiver(current->gid.to_int, i, false)) == DIRECTION_INVALID){
			region->neighbours_info[i].data_offset = SIZE_MAX;
		}else{
			data_offset += abm_settings.neighbour_data_size;
			region->neighbours_info[i].data_offset = data_offset;
		}
	}
	// default
	region->tracked_data = NULL;
	// Save pointer in LP state
	current->region = region;
	// initialize the hash_map for agents
	hash_map_init(&region->agents_table);

	array_init(region->agents_instances);
	array_init(region->agents_leaving);
}

static void on_abm_visit(void){
	struct _visit_abm_t vis;
	// parse the entering agent
	struct _agent_abm_t *agent = agent_from_buffer(current_evt->event_content, current_evt->size);
	// add him to the region
	hash_map_add(&(current->region->agents_table), (hash_map_pair_t *)agent);

	if(array_empty(agent->future) || array_get_at(agent->future, 0).region != current->gid.to_int){
		// this is an intermediate objective to reach the next destination
		vis.action = abm_settings.traverse_handler;
		vis.region = current->gid.to_int;
	} else {
		vis = array_remove_at(agent->future, 0);
	}
	if(abm_settings.keep_history){
		vis.time = current_evt->timestamp;
		// move the visit into the past ones
		array_push(agent->past, vis);
	}

	// seems all ok: user, do whatever you want now
	switch_to_application_mode();
	current->ProcessEvent(current->gid.to_int, current_evt->timestamp, agent->leave_event, (void *)agent->uuid, 0, current->current_base_pointer);
	switch_to_platform_mode();
}

static void on_abm_leave(void){
	struct _visit_abm_t vis;
	unsigned char* to_send;
	unsigned buffer_size;
	// we search for the agent who's leaving
	struct _agent_abm_t *agent = (struct _agent_abm_t *)hash_map_lookup(&(current->region->agents_table), *((unsigned long long*)current_evt->event_content));
	if(!agent || agent->leave_time > current_evt->timestamp) {
		// the exiting agent has been killed or already left or the agent is trying to leave too early (can happen if user decides so)
		return; // since this is spurious we can directly return
	}
	// seems all ok: user, do whatever you want now
	switch_to_application_mode();
	current->ProcessEvent(current->gid.to_int, current_evt->timestamp, agent->leave_event, (void *)agent->uuid, 0, current->current_base_pointer);
	switch_to_platform_mode();
	// we search again for the agent who's leaving (the user could have possibly killed him)
	agent = (struct _agent_abm_t *)hash_map_lookup(&(current->region->agents_table), *((unsigned long long*)current_evt->event_content));
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
				if(array_peek(agent->future).region == current->gid.to_int){
					// we don't need to move
					next_hop = current->gid.to_int;
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

	if(current->gid.to_int == next_hop) {
		// if the next chosen destination is the very same region we are already in
		// we simply call ProcessEvent() again
		if(array_empty(agent->future) || array_peek(agent->future).region != current->gid.to_int){
			vis.region = current->gid.to_int;
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
		switch_to_application_mode();
		current->ProcessEvent(current->gid.to_int, current_evt->timestamp, agent->leave_event, (void *)agent->uuid, 0, current->current_base_pointer);
		switch_to_platform_mode();
	} else {
		to_send = agent_to_buffer(agent, &buffer_size);
		// finally we schedule the agent
		UncheckedScheduleNewEvent(next_hop, current_evt->timestamp, ABM_VISITING, to_send, buffer_size);
		// now we can get rid of it
		KillAgent(agent->uuid);
		// remember to free that stuff if we mallocated it!
		rsfree(to_send);
	}
}

static void receive_update(void){
	if(abm_settings.neighbour_data_size != current_evt->size)
		rootsim_error(true, "Misuse of ABM api, regions do not agree on neighbours info size! EXITING!");

	region_abm_t *region = current->region;
	unsigned i = region->neighbours_count;
	while(i--){// FIXME linear search, maybe sort them at init for log search or sort by direction for constant indexing
		if(region->neighbours_info[i].src_lp == current_evt->sender.to_int){
			memcpy((unsigned char *)region + region->neighbours_info[i].data_offset, current_evt->event_content, abm_settings.neighbour_data_size);
			return;
		}
	}
}

static void dispatch_leavers(void){
	struct _agent_abm_t *agent;
	while(array_count(current->region->agents_leaving)){
		agent = (struct _agent_abm_t *)hash_map_lookup(&(current->region->agents_table), array_pop(current->region->agents_leaving));
		if(agent->leave_event){
			UncheckedScheduleNewEvent(current->gid.to_int, agent->leave_time, ABM_LEAVING, &agent->uuid, sizeof(agent->uuid));
			agent->leave_event = 0;
		}
	}
}

static void update_neighbours(void){
	region_abm_t *region = current->region;
	unsigned char* published_data = (unsigned char *)region + region->published_data_offset;
	// we check whether we need to update our neighbours about some changes in the tracked data
	if(!region->tracked_data || !memcmp(published_data, region->tracked_data, abm_settings.neighbour_data_size))
		return;

	// copy the new data into the tracked one
	memcpy(published_data, region->tracked_data, abm_settings.neighbour_data_size);

	// let's propagate the changes to other regions too
	unsigned i = region->neighbours_count;
	while(i--){
		if(region->neighbours_info[i].src_lp != DIRECTION_INVALID)
			UncheckedScheduleNewEvent(region->neighbours_info[i].src_lp, current_evt->timestamp, ABM_UPDATE, published_data, abm_settings.neighbour_data_size);
	}
}

void ProcessEventABM(void) {
	switch (current_evt->type) {

		case ABM_VISITING:
			on_abm_visit();
			break;

		case ABM_LEAVING:
			on_abm_leave();
			break;

		case ABM_UPDATE:
			receive_update();
			// we didn't give control to the user, no need to check changes in the
			// neighbours infos memory.
			return;
		case INIT:
			abm_layer_init();
			/* fall through */
		default:
			// uninteresting stuff, do as we don't exist
			ProcessEventTopology();
			break;

	}
	dispatch_leavers();
	update_neighbours();
}

int GetNeighbourInfo(direction_t i, unsigned int *region_id, void **data_p){
	region_abm_t *region = current->region;
	if(unlikely(i >= region->neighbours_count))
		rootsim_error(true, "bad argument in abm call");

	if(region->neighbours_info[i].src_lp == DIRECTION_INVALID)
		return -1;

	*region_id = region->neighbours_info[i].src_lp;
	*data_p = (unsigned char *)region + region->neighbours_info[i].data_offset;
	return 0;
}

void TrackNeighbourInfo(void *neighbour_data) {
	current->region->tracked_data = neighbour_data;
}

bool IterAgents(agent_t *agent_p) {
	switch_to_platform_mode();
	// XXX is this correct with preemption?
	static __thread map_size_t closure = 0;
	if(!agent_p) {
		closure = 0;
		return false;
	}
	hash_map_pair_t *pair = hash_map_iter(&(current->region->agents_table), &closure);
	if(pair)
		*agent_p = pair->key;
	switch_to_application_mode();
	return pair != NULL;
}

unsigned CountAgents(void) {
	return hash_map_count(&(current->region->agents_table));
}

agent_t SpawnAgent(unsigned user_data_size) {
	switch_to_platform_mode();

	region_abm_t *region = current->region;
	// new agent
	struct _agent_abm_t *ret = array_reserve(region->agents_instances, 1);

	array_init(ret->future);
	array_init(ret->past);

	ret->user_data_size = user_data_size;
	ret->user_data = rsalloc(user_data_size);

	ret->uuid = generate_mark(current);

	hash_map_add(&(current->region->agents_table), (hash_map_pair_t *)ret);
	// we register the visit to THIS region
	if(abm_settings.keep_history){
		struct _visit_abm_t start_visit = { current->gid.to_int, ACTION_START, current_evt->timestamp };
		array_push(ret->past, start_visit);
	}
	switch_to_application_mode();
	return ret->uuid;
}

void KillAgent(agent_t agent_id) {
	struct _agent_abm_t *agent;

	if(unlikely((agent = (struct _agent_abm_t *)hash_map_remove(&(current->region->agents_table), agent_id)) == NULL))
		rootsim_error(true, "Requested deletion of non existing or non local agent %llu", agent_id);

	unsigned i_rem = agent - array_items(current->region->agents_instances);

	array_fini(agent->future);
	array_fini(agent->past);
	hash_map_remove(&(current->region->agents_table), array_peek(current->region->agents_instances).uuid);
	array_lazy_remove_at(current->region->agents_instances, i_rem);
	hash_map_add(&(current->region->agents_table), (hash_map_pair_t *)&array_get_at(current->region->agents_instances, i_rem));
}

void * DataAgent(agent_t agent_id, unsigned *data_size_p){
	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(data_size_p)
		*data_size_p = agent->user_data_size;
	return agent->user_data;
}

void ScheduleNewLeaveEvent(simtime_t time, unsigned int event_type, agent_t agent_id) {
	switch_to_platform_mode();

	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	// we mark the agent with the intended leave time so we can later compare it
	// to check for spurious events
	agent->leave_time = time;
	agent->leave_event = event_type;

	array_push(current->region->agents_leaving, agent->uuid);

	switch_to_application_mode();
}

unsigned CountPastVisits(agent_t agent_id){
	return array_count(retrieve_agent(agent_id)->past);
}

void GetPastVisit(agent_t agent_id, unsigned *region_p, unsigned *event_type_p, simtime_t *time_p, unsigned i){
	switch_to_platform_mode();

	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(unlikely(i >= array_count(agent->past)))
		rootsim_error(true, "trying to access an out of bounds past visit from agent %llu", agent_id);
	// we count backward to map the nearest event in the past to index 0
	*region_p = array_get_at(agent->past, array_count(agent->past) - 1 - i).region;
	*event_type_p = array_get_at(agent->past, array_count(agent->past) - 1 - i).action;
	*time_p = array_get_at(agent->past, array_count(agent->past) - 1 - i).time;

	switch_to_application_mode();
}

unsigned CountVisits(agent_t agent_id){
	return array_count(retrieve_agent(agent_id)->future);
}

void GetVisit(agent_t agent_id, unsigned *region_p, unsigned *event_type_p, unsigned i){
	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(unlikely(i >= array_count(agent->future)))
		rootsim_error(true, "trying to access an out of bounds future visit from agent %llu", agent_id);

	*region_p = array_get_at(agent->future, i).region;
	*event_type_p = array_get_at(agent->future, i).action;
}

void SetVisit(agent_t agent_id, unsigned region, unsigned event_type, unsigned i){
	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(unlikely(i >= array_count(agent->future) || event_type == ACTION_START || region >= RegionsCount())) {
		rootsim_error(true, "bad argument in abm call for agent %llu", agent_id);
	}

	array_items(agent->future)[i].region = region;
	array_items(agent->future)[i].action = event_type;
}

void EnqueueVisit(agent_t agent_id, unsigned region, unsigned event_type){
	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(unlikely(event_type == ACTION_START || region >= RegionsCount())) {
		rootsim_error(true, "bad argument in abm call for agent %llu", agent_id);
	}

	struct _visit_abm_t visit = {region, event_type, INFINITY};
	array_push(agent->future, visit);
}

void AddVisit(agent_t agent_id, unsigned region, unsigned event_type, unsigned i){
	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(unlikely(i > array_count(agent->future) || event_type == ACTION_START || region >= RegionsCount())) {
		rootsim_error(true, "bad argument in abm call for agent %llu", agent_id);
	}

	struct _visit_abm_t visit = {region, event_type, INFINITY};

	if(i == array_count(agent->future))
		array_push(agent->future, visit);
	else
		array_add_at(agent->future, i, visit);
}

void RemoveVisit(agent_t agent_id, unsigned i) {
	struct _agent_abm_t *agent = retrieve_agent(agent_id);
	if(i >= array_count(agent->future)) {
		rootsim_error(true, "bad argument in abm call for agent %llu", agent_id);
	}

	array_remove_at(agent->future, i);
}
