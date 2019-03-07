/*
 * topology_custom.c
 *
 *  Created on: 24 mag 2018
 *      Author: andrea
 */

#include <ROOT-Sim.h>
#include <lib/topology.h>

#include <math.h>

#include <scheduler/scheduler.h>
#include <lib/jsmn_helper.h>
#include <lib/numerical.h>
#include <datatypes/bitmap.h>
#include <datatypes/array.h>
#include <datatypes/heap.h>
#include <scheduler/process.h>
#include <core/init.h>
#include <serial/serial.h>

struct _topology_global_t topology_global;

//used internally (also in abm_layer module) to schedule our reserved events TODO: move in a more system-like module
void UncheckedScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size){

	msg_t *event;
	GID_t receiver;

	if(unlikely(rootsim_config.serial)){
		SerialScheduleNewEvent(gid_receiver, timestamp, event_type, event_content, event_size);
		return;
	}

	// Internally to the platform, the receiver is a GID, while models
	// have no difference across GIDs and LIDs. We convert here the passed
	// id to a GID.
	set_gid(receiver, gid_receiver);

	// In Silent execution, we do not send again already sent messages
	if(current->state == LP_STATE_SILENT_EXEC) {
		return;
	}

#ifndef NDEBUG
	// Check whether the destination LP is out of range
	if(receiver.to_int >= n_prc_tot) { // It's unsigned, so no need to check whether it's < 0
		rootsim_error(true, "Warning: the destination LP %u %lf %u is out of range. The event has been ignored\n", receiver.to_int, timestamp, event_type);
		return;
	}

	// Check if the associated timestamp is negative
	if(timestamp < lvt(current)) {
		rootsim_error(true, "LP %u is trying to generate an event (type %d) to %u in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n", current, event_type, receiver.to_int, lvt(current), timestamp);
	}
#endif

	// Copy all the information into the event structure
	pack_msg(&event, current->gid, receiver, event_type, timestamp, lvt(current), event_size, event_content);
	event->mark = generate_mark(current);

	insert_outgoing_msg(event);
}

/**
 * Utility function which returns the expected number of neighbours
 * depending on the geometry of the topology.
 */
static unsigned directions_count(void) {
	switch (topology_global.geometry) {
		case TOPOLOGY_GRAPH:
			return topology_global.lp_cnt - 1;
		case TOPOLOGY_HEXAGON:
			return 6;
		case TOPOLOGY_TORUS:
		case TOPOLOGY_SQUARE:
			return 4;
		case TOPOLOGY_STAR:
			return 2;
		case TOPOLOGY_RING:
			return 1;
		case TOPOLOGY_BIDRING:
			return 2;
	}
	return UINT_MAX;
}

/**
 * This loads a topology file:
 * it checks for the correctness of the JSON file,
 * it sets the correct values for the global struct,
 * it calls the right specific topology parser,
 * it makes cleanup (the returned malloc'ed area needs to be freed at simulation shutdown)
 * @param file_name the path of the file containing the topology info
 * @return an opaque malloc'ed area used by the specific topology initiators
 */
static void *load_topology_file(const char *file_name) {
	char *json_base;
	jsmntok_t *root_token;
	c_jsmntok_t *t;

	unsigned lp_cnt;
	enum _topology_type_t t_type;
	enum _topology_geometry_t geometry;

	void *ret = NULL;

	// load and parse the file with given file_name
	if(load_and_parse_json_file(file_name, &json_base, &root_token) < 0)
		rootsim_error(true, "The specified topology file at \"%s\" is either non accessible, non existing, or is not a properly formed JSON file", file_name);
	// parse the regions count and check its validity
	if(parse_unsigned_by_key(root_token, json_base, root_token, "regions_count", &lp_cnt) < 0)
		rootsim_error(true, "Invalid or missing json value with key \"regions_count\" (must be an unsigned integer)");
	// sanity checks on the number of instantiated LPs
	if(lp_cnt + topology_settings.out_of_topology > n_prc_tot)
		rootsim_error(true, "This topology needs an higher number of available LPs (%lu versus %lu available LPs)", lp_cnt, n_prc_tot);
	if(lp_cnt + topology_settings.out_of_topology < n_prc_tot)
		rootsim_error(true, "The requested regions are fewer than the available LPs (%lu versus %lu available LPs)", lp_cnt, n_prc_tot);

	// look for the topology type
	const char *type_choices[] = {
			[TOPOLOGY_COSTS] = 	"costs",
			[TOPOLOGY_OBSTACLES] = 	"obstacles",
			[TOPOLOGY_PROBABILITIES] = "probabilities"
	};
	// look for the type key and retrieve its value
	t = get_value_token_by_key(root_token, json_base, root_token, "type");
	// parse the choice from the expected string value
	if((t_type = parse_string_choice(root_token, json_base, t, sizeof(type_choices)/sizeof(const char *), type_choices)) == UINT_MAX)
		rootsim_error(true, "Invalid or missing json value with key \"type\" (must be a recognizable string)");
	// sanity check between the topology type requested by the model and what we found
	if(t_type != topology_settings.type){
		rootsim_error(true, "The specified topology has a different type from the one requested by the model");
	}

	// look for the topology type
	const char *geometry_choices[] = {
			[TOPOLOGY_HEXAGON - TOPOLOGY_GEOMETRY_OFFSET] = "hexagons",
			[TOPOLOGY_SQUARE - TOPOLOGY_GEOMETRY_OFFSET] = 	"squares",
			[TOPOLOGY_GRAPH - TOPOLOGY_GEOMETRY_OFFSET] = 	"graph",
			[TOPOLOGY_STAR - TOPOLOGY_GEOMETRY_OFFSET] = 	"star",
			[TOPOLOGY_RING - TOPOLOGY_GEOMETRY_OFFSET] = 	"ring",
			[TOPOLOGY_BIDRING - TOPOLOGY_GEOMETRY_OFFSET] = "bidring",
			[TOPOLOGY_TORUS - TOPOLOGY_GEOMETRY_OFFSET] = 	"torus"
	};
	// look for the geometry key and retrieve its value
	t = get_value_token_by_key(root_token, json_base, root_token, "geometry");
	// parse the choice from the expected string value
	if((geometry = parse_string_choice(root_token, json_base, t, sizeof(geometry_choices)/sizeof(const char *), geometry_choices)) == UINT_MAX)
		rootsim_error(true, "Invalid or missing json value with key \"geometry\" (must be a recognizable string)");
	// we sum the offset of the enum to obtain a valid geometry number
	geometry += TOPOLOGY_GEOMETRY_OFFSET;
	// we set the known fields of the global struct
	topology_global.geometry = geometry;
	topology_global.lp_cnt = lp_cnt;
	topology_global.directions = directions_count();
	// we give control to the right specific parser
	switch (t_type) {
		case TOPOLOGY_PROBABILITIES:
			ret = load_topology_file_probabilities(root_token, json_base);
			break;

		case TOPOLOGY_COSTS:
			ret = load_topology_file_costs(root_token, json_base);
			break;

		case TOPOLOGY_OBSTACLES:
			ret = load_topology_file_obstacles(root_token, json_base);
			break;
	}
	// cleanup
	rsfree(root_token);
	rsfree(json_base);
	// return the topology specific data
	return ret;
}

/**
 * This pre-computes the edge of the topology;
 * the <sqrt>"()" is expensive and so we cache its value
 */
static void compute_edge(void){
	unsigned edge;
	const unsigned lp_cnt = topology_global.lp_cnt;
	switch (topology_global.geometry) {
		case TOPOLOGY_SQUARE:
		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_TORUS:
			edge = sqrt(lp_cnt);
			// we make sure there are no "lonely" LPs
			if(edge * edge != lp_cnt)
				rootsim_error(true, "Invalid number of regions for this topology geometry (must be a square number)");
			break;
		default:
			// the edge value is actually unused
			edge = 0;
			break;
	}
	// set the edge value
	topology_global.edge = edge;
}

/**
 * Initialize the topology module for each LP hosted on the machine.
 * This needs to be called right after LP basic initialization before starting to process events.
 */
void topology_init(void) {
	if(!&topology_settings)
		// the weak symbol isn't defined: we aren't needed
		return;

	// this is the data extracted from the topology file
	void *t_data = NULL;
	// basic sanity check
	if(topology_settings.out_of_topology >= n_prc_tot)
		rootsim_error(true, "Not enough LPs to run even a default topology with %u control LPs", topology_settings.out_of_topology);
	// set default values
	topology_global.lp_cnt = n_prc_tot - topology_settings.out_of_topology;
	topology_global.geometry = topology_settings.default_geometry;
	topology_global.directions = directions_count();
	// load settings from file if specified
	if(topology_settings.topology_path)
		t_data = load_topology_file(topology_settings.topology_path);
	// compute the edge value for topologies it makes sense for
	compute_edge();
	// compute the topology struct size (used also for check-pointing)
	switch(topology_settings.type){
		case TOPOLOGY_COSTS:
			topology_global.chkp_size = size_checkpoint_costs();
			break;
		case TOPOLOGY_PROBABILITIES:
			topology_global.chkp_size = size_checkpoint_probabilities();
			break;
		case TOPOLOGY_OBSTACLES:
			topology_global.chkp_size = size_checkpoint_obstacles();
			break;
	}
	// initialize the topology struct
	foreach_lp(lp){
		if(lp->gid.to_int >= topology_global.lp_cnt){
			// this LP isn't part of the underlying topology
			lp->topology = NULL;
		}
		switch(topology_settings.type){
			case TOPOLOGY_COSTS:
				lp->topology = topology_costs_init(lp->gid.to_int, t_data);
				break;
			case TOPOLOGY_OBSTACLES:
				lp->topology = topology_obstacles_init(lp->gid.to_int, t_data);
				break;
			case TOPOLOGY_PROBABILITIES:
				lp->topology = topology_probabilities_init(lp->gid.to_int, t_data);
				break;
		}
	}
	// free the topology data read from file
	rsfree(t_data);
}

void SetValueTopology(unsigned from, unsigned to, double value) {
	switch_to_platform_mode();
	const unsigned lp_cnt = topology_global.lp_cnt;

	if(unlikely(!topology_settings.write_enabled))
		rootsim_error(true, "SetValueTopology(): called with write_enable false");

	if(unlikely(from >= lp_cnt || to >= lp_cnt))
		rootsim_error(true, "SetValueTopology(): from % u, to %u when lp_cnt is %u", from, to, lp_cnt);

	if(unlikely(value < 0))
		rootsim_error(true, "SetValueTopology(): negative values are not supported", value);

	switch (topology_settings.type) {
		case TOPOLOGY_COSTS:
			set_value_topology_costs(from, to, value);
			break;
		case TOPOLOGY_PROBABILITIES:
			set_value_topology_probabilities(from, to, value);
			break;
		case TOPOLOGY_OBSTACLES:
			set_value_topology_obstacles(from, to, value);
			break;
	}
	switch_to_application_mode();
}

double GetValueTopology(unsigned from, unsigned to) {
	switch_to_platform_mode();
	const unsigned lp_cnt = topology_global.lp_cnt;

	double ret = -1;

	if(from >= lp_cnt || to >= lp_cnt)
		rootsim_error(true, "SetValueTopology(): from % u, to %u when lp_cnt is %u", from, to, lp_cnt);

	switch (topology_settings.type) {
		case TOPOLOGY_COSTS:
			ret = get_value_topology_costs(from, to);
			break;
		case TOPOLOGY_PROBABILITIES:
			ret = get_value_topology_probabilities(from, to);
			break;
		case TOPOLOGY_OBSTACLES:
			ret = get_value_topology_obstacles(from, to);
			break;
	}
	switch_to_application_mode();
	return ret;
}

void ProcessEventTopology(void){
	switch(current_evt->type){
		case TOPOLOGY_UPDATE:
			switch (topology_settings.type) {
				case TOPOLOGY_COSTS:
					update_topology_costs();
					break;
				case TOPOLOGY_PROBABILITIES:
					update_topology_probabilities();
					break;
				case TOPOLOGY_OBSTACLES:
					update_topology_obstacles();
					break;
				default:
					rootsim_error(true, "This shouldn't happen. Contact the maintainer");
			}
			break;
		default:
			switch_to_application_mode();
			current->ProcessEvent(current->gid.to_int, current_evt->timestamp, current_evt->type, current_evt->event_content, current_evt->size, current->current_base_pointer);
			switch_to_platform_mode();
	}
}

unsigned int RegionsCount(void) {
	return topology_global.lp_cnt;
}

unsigned int DirectionsCount(void) {
	return topology_global.directions;
}

static bool is_reachable(unsigned int to){
	bool result = false;
	switch (topology_settings.type) {
		case TOPOLOGY_PROBABILITIES:
			result = is_reachable_probabilities(to);
			break;
		case TOPOLOGY_OBSTACLES:
			result = is_reachable_obstacles(to);
			break;
		case TOPOLOGY_COSTS:
			result = is_reachable_costs(to);
			break;
	}
	return result;
}

unsigned int NeighboursCount(unsigned region){
	switch_to_platform_mode();
	unsigned i = topology_global.directions;
	unsigned res = 0;
	unsigned lp_id;
	while(i--){
		if((lp_id = get_raw_receiver(region, i)) != DIRECTION_INVALID)
			res++;
	}
	switch_to_application_mode();
	return res;
}

/**
 * Compute the id of the LP positioned in the specified direction adjacent to the given LP.
 * @param from The id of the starting point LP
 * @param direction The direction needed to get to the desired neighbour
 * @return the id of the adjacent LP in the requested direction or INVALID_DIRECTION if there's no LP satisfying the
 * request
 */
unsigned int get_raw_receiver(unsigned int from, direction_t direction) {
	unsigned int x, y;
	unsigned receiver;
	// const so we don't accidentally modify it
	const unsigned sender = from;
	// the pre-computed edge length for geometries which require it
	const unsigned edge = topology_global.edge;
	// without modifications we would fail
	receiver = DIRECTION_INVALID;

	switch (topology_global.geometry) {

		case TOPOLOGY_HEXAGON:
			// Convert linear coords to square coords
			y = sender / edge;
			x = sender - y * edge;

			switch (direction) {
				case DIRECTION_NW:
					x += (y & 1U) - 1;
					y -= 1;
					break;
				case DIRECTION_NE:
					x += (y & 1U);
					y -= 1;
					break;
				case DIRECTION_SW:
					x += (y & 1U) - 1;
					y += 1;
					break;
				case DIRECTION_SE:
					x += (y & 1U);
					y += 1;
					break;
				case DIRECTION_E:
					x += 1;
					break;
				case DIRECTION_W:
					x -= 1;
					break;
				default:
					goto out;
			}

			if(likely(x < edge && y < edge))
				receiver = y * edge + x;

			break;

		case TOPOLOGY_SQUARE:
			// Convert linear coords to square coords
			y = sender / edge;
			x = sender - y * edge;

			switch (direction) {
				case DIRECTION_N:
					y -= 1;
					break;
				case DIRECTION_S:
					y += 1;
					break;
				case DIRECTION_E:
					x += 1;
					break;
				case DIRECTION_W:
					x -= 1;
					break;
				default:
					goto out;
			}

			if(likely(x < edge && y < edge))
				receiver = y * edge + x;

			break;

		case TOPOLOGY_TORUS:
			// Convert linear coords to square coords
			y = sender / edge;
			x = sender - y * edge;

			switch (direction) {
				case DIRECTION_N:
					y -= 1;
					if(unlikely(y >= edge)){
						y = edge - 1;
					}
					break;
				case DIRECTION_S:
					y += 1;
					if(unlikely(y >= edge)){
						y = 0;
					}
					break;
				case DIRECTION_E:
					x += 1;
					if(unlikely(x >= edge)){
						x = 0;
					}
					break;
				case DIRECTION_W:
					x -= 1;
					if(unlikely(x >= edge)){
						x = edge - 1;
					}
					break;
				default:
					goto out;
			}

			// Convert back to linear coordinates
			receiver = y * edge + x;

			break;

		case TOPOLOGY_GRAPH:
			if(likely(direction < topology_global.lp_cnt))
				receiver = direction;
			break;

		case TOPOLOGY_BIDRING:
			if(direction == DIRECTION_W) {
				if(sender == 0) {
					receiver = topology_global.lp_cnt - 1;
				} else {
					receiver =sender - 1;
				}
			} else if(likely(direction == DIRECTION_E)) {
				if(sender + 1 == topology_global.lp_cnt)
					receiver = 0;
				else
					receiver = sender + 1;
			}

			break;

		case TOPOLOGY_RING:
			if(likely(direction == DIRECTION_E)) {
				receiver = sender + 1;

				if(receiver == topology_global.lp_cnt) {
					receiver = 0;
				}
			}

			break;

		case TOPOLOGY_STAR:
			if(sender) {
				if(direction)
					goto out;
				receiver = 0;
			} else {
				if(direction + 1 >= topology_global.lp_cnt)
					goto out;
				receiver = direction + 1;
			}

			break;

		default:
			rootsim_error(true, "This shouldn't happen. Aborting...");
	}

	out:
	return receiver;
}

unsigned int GetReceiver(unsigned int from, direction_t direction, bool reachable) {
	unsigned receiver;
	switch_to_platform_mode();
	// sanity check
	if(unlikely(topology_global.lp_cnt <= from))
		rootsim_error(true, "GetReceiver(): region argument not included in topology!");

	receiver = get_raw_receiver(from, direction);
	if(reachable && !is_reachable(receiver))
		receiver = UINT_MAX;
	switch_to_application_mode();
	return receiver;
}

bool IsReachable(unsigned int to){
	bool result;
	switch_to_platform_mode();
	result = is_reachable(to);
	switch_to_application_mode();
	return result;
}

unsigned int FindReceiver(void) {
	switch_to_platform_mode();

	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned receiver = DIRECTION_INVALID;

	if(unlikely(lp_cnt <= current->gid.to_int))
		rootsim_error(true, "FindReceiver(): source region %u when topology includes %u regions", current->gid.to_int, lp_cnt);

	switch (topology_settings.type) {
		case TOPOLOGY_PROBABILITIES:
			receiver = find_receiver_probabilities();
			break;
		case TOPOLOGY_OBSTACLES:
			receiver = find_receiver_obstacles();
			break;
		default:
			rootsim_error(true, "FindReceiver(): topology type not supported!");
	}

	switch_to_application_mode();
	return receiver;
}

unsigned int FindReceiverToward(unsigned int to) {
	switch_to_platform_mode();

	const unsigned lp_cnt = topology_global.lp_cnt;
	// fail by default
	unsigned receiver = DIRECTION_INVALID;
	// sanity checks
	if(unlikely(to >= lp_cnt || current->gid.to_int >= lp_cnt))
		rootsim_error(true, "Calling FindReceiverToward() from or toward a region not included in the topology");

	switch (topology_settings.type) {
		case TOPOLOGY_COSTS:
			receiver = find_receiver_toward_costs(to);
			break;
		case TOPOLOGY_OBSTACLES:
			receiver = find_receiver_toward_obstacles(to);
			break;
		default:
			rootsim_error(true, "FindReceiverToward(): unsupported topology type");
	}
	switch_to_application_mode();
	return receiver;
}

/**
 * Compute the path from 2 LPs given a tree rooted in @p source represented as a parent array (the classic output of the
 * Dijkstra algorithm). We expect source and dest to be different LP ids.
 *
 * @param lp_cnt The size of the tree
 * @param result A caller supplied array which will hold the path as sequence of LP ids
 * @param previous The tree represented as a parent array (the i-th entry is the id of the parent of the LP with id i)
 * @param source The id of the source LP
 * @param dest The id of the destination LP which must be different from the source
 * @return 0 if there's no path between the 2 specified LPs in the given tree, else the number of hops in the returned path
 */
unsigned build_path(unsigned lp_cnt, unsigned result[lp_cnt], const unsigned int previous[lp_cnt], unsigned source, unsigned dest) {
	unsigned mid_cell;
	unsigned *res_aux;

	if(previous[dest] == UINT_MAX) {
		// the wished destination is unreachable
		return 0;
	}
	// we point to the latest element (we still don't know how long is the path)
	res_aux = result + lp_cnt - 1;
	// we fill in the obvious last hop
	*res_aux = dest;

	unsigned hops = 1;

	mid_cell = previous[dest];
	// while the previous hop is not the actual source
	while (mid_cell != source) {
		// we go further behind
		res_aux--;
		// we fill in the intermediate step
		*res_aux = mid_cell;
		// go behind you too!
		mid_cell = previous[mid_cell];
		// one more hop
		++hops;
	}
	// we move back the computed path
	memmove(result, res_aux, ((result + lp_cnt) - res_aux) * sizeof(unsigned));
	return hops;
}

double ComputeMinTour(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]) {
	switch_to_platform_mode();

	const unsigned lp_cnt = topology_global.lp_cnt;
	double ret = -1.0;

	if(unlikely(source >= lp_cnt)) {
		rootsim_error(true, "Invalid source passed to ComputeMinTour(): %u.\n", source);
		return UINT_MAX;
	}
	if(unlikely(dest >= lp_cnt)) {
		rootsim_error(true, "Invalid destination passed to ComputeMinTour(): %u.\n", dest);
		return UINT_MAX;
	}
	if(unlikely(source == dest)) {
		rootsim_error(true, "Asking ComputeMinTour() to find a path from a source equal to the destination\n");
		return UINT_MAX;
	}

	switch (topology_settings.type) {
		case TOPOLOGY_COSTS:
			ret = compute_min_tour_costs(source, dest, result);
			break;
		case TOPOLOGY_OBSTACLES:
			ret = compute_min_tour_obstacles(source, dest, result);
			break;
		default:
			rootsim_error(true, "ComputeMinTour(): unsupported topology type %u", topology_settings.type);
	}
	switch_to_application_mode();
	return ret;
}
