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

#include <core/init.h>
#include <scheduler/scheduler.h>
#include <lib/topology.h>
#include <datatypes/array.h>
#include <datatypes/hash_map.h>

#define ACTION_START INIT

#define retrieve_agent(agent_id) ({ \
	struct _agent_abm_t *__ret = hash_map_lookup(current->region->agents_table, agent_id); \
	if(unlikely(!__ret)) \
		rootsim_error(true, "Looking for non existing agent id!"); \
	assert(agent_id == __ret->key); \
	__ret; \
})

struct _visit_abm_t{
	unsigned region;
	unsigned action;
	simtime_t time;
};

struct _agent_abm_t {
	unsigned long long key;		//! UUID that uniquely identifies the agent (this must be the first field of the struct for various reasons)
	unsigned user_data_size;	//! This must be the seonc filed of this struct for serialization reasons
	char *user_data;
	simtime_t leave_time;
	rootsim_array(struct _visit_abm_t) future;
	rootsim_array(struct _visit_abm_t) past;
};

struct _region_abm_t {
	rootsim_hash_map(struct _agent_abm_t) agents_table;
	unsigned long long next_mark;
	unsigned published_data_offset;
	unsigned char *tracked_data;
	unsigned chkp_size;
	struct _n_info_t{
		unsigned data_offset;
		unsigned int src_lp;
	} neighbours_info[];
};

struct _leave_evt{
	unsigned long long key;
	unsigned leave_code;
};

static inline unsigned long long get_agent_mark(region_abm_t *region){
	unsigned long long ret = region->next_mark;
	region->next_mark += RegionsCount();
	return ret;
}


/**
* Compute the size in bytes of an eventual agent serialization.
*
* @param agent A pointer to the agent
*/
static unsigned agent_dump_size(const struct _agent_abm_t *agent) {
	return (unsigned)(sizeof(agent->key) + sizeof(agent->user_data_size) + agent->user_data_size
			+ array_dump_size(agent->future) + abm_settings.keep_history * array_dump_size(agent->past));
}


/**
* Deserialize an agent from a buffer.
*
* @param event_content A pointer to the buffer containing the serialzied agent
* @param event_size The size in bytes of the buffer
* @return a newly instantiated agent struct, the deserialized agent
*/
static struct _agent_abm_t* agent_from_buffer(const unsigned char* event_content, unsigned event_size){
	// keep track of original pointer
	const unsigned char *buffer = event_content;
	// allocate the memory for the visiting agent
	struct _agent_abm_t *agent = hash_map_reserve_elem(current->region->agents_table, *((const unsigned long long *)event_content));
	agent->leave_time = -1.0;
	// copy uuid and user data size
	memcpy(agent, buffer, sizeof(agent->user_data_size) + sizeof(agent->key));
	buffer += sizeof(agent->user_data_size) + sizeof(agent->key);
	// copy user data
	agent->user_data = rsalloc(agent->user_data_size);
	memcpy(agent->user_data, buffer, agent->user_data_size);
	buffer += agent->user_data_size;
	// load the arrays
	array_load(agent->future, buffer);
	if(abm_settings.keep_history)
		array_load(agent->past, buffer);
	else
		memset(&agent->past, 0, sizeof(agent->past));

	//sanity check
	assert(buffer == event_content + event_size);
	return agent;
}


/**
* Serialize an agent into a buffer, the buffer is caller supplied and its
* required size can be calculated with the function agent_dump_size.
*
* @param agent A pointer to the agent struct to be serialized
* @param buffer A pointer to the buffer to fill with the serialized agent
*/
static void agent_to_buffer(struct _agent_abm_t *agent, unsigned char* buffer){
	// we use buffer as a mobile pointer so we need to save the original value
	unsigned char *to_send = buffer;
	// copy relevant fields
	memcpy(buffer, agent, sizeof(agent->user_data_size) + sizeof(agent->key));
	buffer += sizeof(agent->user_data_size) + sizeof(agent->key);
	memcpy(buffer, agent->user_data, agent->user_data_size);
	buffer += agent->user_data_size;
	// dumping the arrays
	array_dump(agent->future, buffer);
	if(abm_settings.keep_history)
		array_dump(agent->past, buffer);
	// sanity check
	assert(buffer == to_send +  agent_dump_size(agent));
}


/**
* Checkpoint the region state, saving it into a buffer.
* This is periodically called by the checkpointing module to save the region state.
* The returned buffer needs to be freed.
*
* @param region A pointer to the region struct to be checkpointed
* @return A malloc'ed buffer holding all the region data
*/
unsigned char * abm_do_checkpoint(region_abm_t *region){
	// calculate dump size
	size_t chkp_size_tot = region->chkp_size;
	chkp_size_tot += hash_map_dump_size(region->agents_table);
	struct _agent_abm_t *agent;
	unsigned i = hash_map_count(region->agents_table);
	while(i--){
		agent = &hash_map_items(region->agents_table)[i];
		chkp_size_tot += array_dump_size(agent->future);
		if(abm_settings.keep_history)
			chkp_size_tot += array_dump_size(agent->past);
		chkp_size_tot += agent->user_data_size;
	}
	// allocate and populate the checkpoint
	unsigned char *ret = rsalloc(chkp_size_tot), *chk = ret;
	memcpy(ret, region, region->chkp_size);
	ret += region->chkp_size;
	hash_map_dump(region->agents_table, ret);
	i = hash_map_count(region->agents_table);
	while(i--){
		agent = &hash_map_items(region->agents_table)[i];
		array_dump(agent->future, ret);
		if(abm_settings.keep_history)
			array_dump(agent->past, ret);
		memcpy(ret, agent->user_data, agent->user_data_size);
		ret += agent->user_data_size;
	}
	assert(chk + chkp_size_tot == ret);
	return chk;
}

/**
* Restore a region struct from a previously checkpointed state.
*
* @param data A pointer to the region struct to be checkpointed
* @return A malloc'ed buffer holding all the region data
*/
void abm_restore_checkpoint(unsigned char *data, region_abm_t *region){
	struct _agent_abm_t *agent;

	assert(((region_abm_t *)data)->chkp_size == region->chkp_size);
	// free the region allocations
	unsigned i = hash_map_count(region->agents_table);
	while(i--){
		agent = &(hash_map_items(region->agents_table)[i]);
		array_fini(agent->future);
		if(abm_settings.keep_history)
			array_fini(agent->past);
		rsfree(agent->user_data);
	}
	hash_map_fini(region->agents_table);
	// copy the region back
	memcpy(region, data, region->chkp_size);
	data += region->chkp_size;
	//load the other allocations
	hash_map_load(region->agents_table, data);
	i = hash_map_count(region->agents_table);
	while(i--){
		agent = &(hash_map_items(region->agents_table)[i]);
		array_load(agent->future, data);
		if(abm_settings.keep_history)
			array_load(agent->past, data);
		agent->user_data = rsalloc(agent->user_data_size);
		memcpy(agent->user_data, data, agent->user_data_size);
		data += agent->user_data_size;
	}
}


/**
* Initializes the abm layer internals for all the lps hosted on the machine.
* This needs to be called before starting processing events, after basic
* initialization of the lps.
*/
void abm_layer_init(void) {

	unsigned actual_neighbours, i, region_alloc_size;
	unsigned data_offset;
	region_abm_t *region;

	// settings_abm is a weak symbol, we check for its existence
	if(!&abm_settings){
		return;
	}

	const unsigned directions = DirectionsCount();
	// the ABM API needs an underlying topology for meaningful operations!!!
	if(!directions){
		rootsim_error(true, "Topology has not been initialized!");
		return;
	}

	foreach_lp(lp){
		// we count valid neighbours
		// todo: for constant topologies we can do better than this, we can just select not crossable regions
		actual_neighbours = NeighboursCount(lp->gid.to_int);

		region_alloc_size = (unsigned)(sizeof(region_abm_t) + (sizeof(struct _n_info_t) * directions) + (abm_settings.neighbour_data_size * (actual_neighbours + 1)));
		// instantiates region struct
		region = rsalloc(region_alloc_size);
		memset(region, 0, region_alloc_size);
		// memory layout is as follows:
		// BASE REGION STRUCT | ARRAY OF NEIGHBOUR INFOS | PUBLISHED DATA BY SELF | ARRAY OF PUBLISHED DATA BY OTHER REGIONS
		region->chkp_size = region_alloc_size;
		// helper offset variable for easier assignment of memory regions
		data_offset = (unsigned)(sizeof(region_abm_t) + (sizeof(struct _n_info_t) * directions));
		// the nearest block is used to keep track of published data
		region->published_data_offset = data_offset;
		// here we assign a memory region only to valid neighbours
		for(i = 0; i < directions; ++i){
			if((region->neighbours_info[i].src_lp = GetReceiver(lp->gid.to_int, i, false)) == DIRECTION_INVALID){
				region->neighbours_info[i].data_offset = UINT_MAX;
			}else{
				data_offset += abm_settings.neighbour_data_size;
				region->neighbours_info[i].data_offset = data_offset;
			}
		}
		// default
		region->tracked_data = NULL;

		// initialize the hash_map for agents
		hash_map_init(region->agents_table);
		// initialize mark
		region->next_mark = lp->gid.to_int;
		// Save pointer in LP state
		lp->region = region;
	}
}

static void receive_update(void);
static void update_neighbours(void);

/**
* Handle a visit to a region. This is called by the abm layer when a visit message is received.
* We expect the event to have the serialized visiting agent as payload.
*/
static void on_abm_visit(void){
	struct _visit_abm_t vis;
	// parse the entering agent
	struct _agent_abm_t *agent = agent_from_buffer(current_evt->event_content, current_evt->size);

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

	// seems all ok: user, do whatever you want now (passing current_evt->content works as long as the first
	// field of the agent struct is the agent key
	switch_to_application_mode();
	current->ProcessEvent(current->gid.to_int, current_evt->timestamp, vis.action, current_evt->event_content, sizeof(agent->key), current->current_base_pointer);
	switch_to_platform_mode();
}

/**
* Handle an agent departure. This is called by the abm layer when a leave message is received.
* We expect the event to have the the agent key as payload.
*/
static void on_abm_leave(void){
	struct _visit_abm_t vis;
	unsigned char* to_send;
	unsigned buffer_size;
	// we search for the agent who's leaving
	assert(current_evt->size == sizeof(struct _leave_evt));
	struct _agent_abm_t *agent = hash_map_lookup(current->region->agents_table, ((struct _leave_evt *)current_evt->event_content)->key);
	if(!agent || agent->leave_time > current_evt->timestamp) {
		// the exiting agent has been killed or already left or the agent is trying to leave too early (can happen if user decides so)
		return; // since this is spurious we can directly return
	}

	// seems all ok: user, do whatever you want now
	switch_to_application_mode();
	current->ProcessEvent(current->gid.to_int, current_evt->timestamp, ((struct _leave_evt *)current_evt->event_content)->leave_code, current_evt->event_content, sizeof(agent->key), current->current_base_pointer);
	switch_to_platform_mode();
	// we search again for the agent who's leaving (the user could have possibly killed him)
	agent = hash_map_lookup(current->region->agents_table, ((struct _leave_evt *)current_evt->event_content)->key);
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
		current->ProcessEvent(current->gid.to_int, current_evt->timestamp, vis.action, current_evt->event_content, sizeof(agent->key), current->current_base_pointer);
		switch_to_platform_mode();
	} else {
		buffer_size = agent_dump_size(agent);

		to_send = rsalloc(buffer_size);

		agent_to_buffer(agent, to_send);
		// finally we schedule the agent
		UncheckedScheduleNewEvent(next_hop, current_evt->timestamp, ABM_VISITING, to_send, buffer_size);
		// now we can get rid of it
		KillAgent(agent->key);
		// remember to free that stuff if we mallocated it!
		rsfree(to_send);
	}
}

/**
* Handle an update receive. This updates the corresponding entry in the region struct, which will be used to
* serve fresh neighbour data to the user.
*/
static void receive_update(void){
	//if(abm_settings.neighbour_data_size != current_evt->size)
		//rootsim_error(true, "Misuse of ABM api, regions do not agree on neighbours info size! EXITING!");

	region_abm_t *region = current->region;
	unsigned i = DirectionsCount();
	while(i--){// FIXME linear search, maybe sort them at init for log search or sort by direction for constant indexing
		if(region->neighbours_info[i].src_lp == current_evt->sender.to_int){
			memcpy(((unsigned char *)region) + region->neighbours_info[i].data_offset, current_evt->event_content, abm_settings.neighbour_data_size);
			return;
		}
	}
	rootsim_error(true, "Misuse of ABM api, unable to find neighbours info's memory area! EXITING!");
}

/**
* Keep updated the neighbours of changes in the tracked data. This is called after each event and boradcasts eventual
* changes to neighbours
*/
static void update_neighbours(void){
	region_abm_t *region = current->region;
	unsigned char* published_data = ((unsigned char *)region) + region->published_data_offset;
	// we check whether we need to update our neighbours about some changes in the tracked data
	if(!region->tracked_data || !memcmp(published_data, region->tracked_data, abm_settings.neighbour_data_size))
		return;

	// copy the new data into the tracked one
	memcpy(published_data, region->tracked_data, abm_settings.neighbour_data_size);

	// let's propagate the changes to other regions too
	unsigned i = DirectionsCount();
	while(i--){
		if(region->neighbours_info[i].src_lp != DIRECTION_INVALID){
			UncheckedScheduleNewEvent(region->neighbours_info[i].src_lp, current_evt->timestamp, ABM_UPDATE, published_data, abm_settings.neighbour_data_size);
		}
	}
}

/**
* The event handler which gets called instead of the user supplied ProcessEvent() function when the abm_layer is in use.
* The unrecognized events get passed to ProcessEventTopology() since the abm layer is based on the topology APIs.
*/
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
		default:
			// uninteresting stuff, do as we don't exist
			ProcessEventTopology();
			break;

	}
	update_neighbours();
}

int GetNeighbourInfo(direction_t i, unsigned int *region_id, void **data_p){
	region_abm_t *region = current->region;
	if(unlikely(i >= DirectionsCount()))
		rootsim_error(true, "bad argument in abm call");

	if(region->neighbours_info[i].src_lp == DIRECTION_INVALID)
		return -1;

	*region_id = region->neighbours_info[i].src_lp;
	*data_p = ((unsigned char *)region) + region->neighbours_info[i].data_offset;
	return 0;
}

void TrackNeighbourInfo(void *neighbour_data) {
	current->region->tracked_data = neighbour_data;
}

bool IterAgents(agent_t *agent_p) {
	switch_to_platform_mode();
	// fixme preemption breaks this
	static __thread map_size_t closure = 0;
	if(!agent_p || closure >= hash_map_count(current->region->agents_table)) {
		closure = 0;
		return false;
	}
	*agent_p = hash_map_items(current->region->agents_table)[closure++].key;
	switch_to_application_mode();
	return true;
}

unsigned CountAgents(void) {
	return hash_map_count(current->region->agents_table);
}

agent_t SpawnAgent(unsigned user_data_size) {
	switch_to_platform_mode();

	unsigned long long new_key = get_agent_mark(current->region);
	// new agent
	struct _agent_abm_t *ret = hash_map_reserve_elem(current->region->agents_table, new_key);

	array_init(ret->future);

	ret->user_data_size = user_data_size;
	ret->user_data = rsalloc(user_data_size);
	ret->key = new_key;

	// we register the visit to THIS region
	if(abm_settings.keep_history){
		array_init(ret->past);
		struct _visit_abm_t start_visit = { current->gid.to_int, ACTION_START, current_evt->timestamp };
		array_push(ret->past, start_visit);
	}else{
		memset(&ret->past, 0, sizeof(ret->past));
	}
	switch_to_application_mode();
	return ret->key;
}

void KillAgent(agent_t agent_id) {
	switch_to_platform_mode();
	struct _agent_abm_t *agent = retrieve_agent(agent_id);

	array_fini(agent->future);
	if(abm_settings.keep_history)
		array_fini(agent->past);
	rsfree(agent->user_data);

	hash_map_delete_elem(current->region->agents_table, agent);
	switch_to_application_mode();
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

	struct _leave_evt leave_evt = {agent_id, event_type};

	UncheckedScheduleNewEvent(current->gid.to_int, agent->leave_time, ABM_LEAVING, &leave_evt, sizeof(leave_evt));
	//array_push(current->region->agents_leaving, agent->key);

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
