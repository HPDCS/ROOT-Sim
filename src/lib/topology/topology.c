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

#define CURRENT_LP_ID 	(gid_to_int(LidToGid(current_lp)))

struct _topology_global_t topology_global;

//used internally (also in abm_layer module) to schedule our reserved events TODO: move in a more system-like module
void UncheckedScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size){

	msg_t *event;
	GID_t receiver;

	// Internally to the platform, the receiver is a GID, while models
	// have no difference across GIDs and LIDs. We convert here the passed
	// id to a GID.
	set_gid(receiver, gid_receiver);

	// In Silent execution, we do not send again already sent messages
	if(LPS(current_lp)->state == LP_STATE_SILENT_EXEC) {
		return;
	}

	// Check whether the destination LP is out of range
	if(receiver.id >= n_prc_tot) { // It's unsigned, so no need to check whether it's < 0
		rootsim_error(true, "Warning: the destination LP %u %lf %u is out of range. The event has been ignored\n", receiver.id, timestamp, event_type);
		return;
	}

	// Check if the associated timestamp is negative
	if(timestamp < lvt(current_lp)) {
		rootsim_error(true, "LP %u is trying to generate an event (type %d) to %u in the past! (Current LVT = %f, generated event's timestamp = %f) Aborting...\n", current_lp, event_type, receiver.id, lvt(current_lp), timestamp);
	}

	// Copy all the information into the event structure
	pack_msg(&event, LidToGid(current_lp), receiver, event_type, timestamp, lvt(current_lp), event_size, event_content);
	event->mark = generate_mark(current_lp);

	insert_outgoing_msg(event);
}

static unsigned neighbours_count(void) {
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


static void load_default_topology(void){
	unsigned edge;

	topology_global.geometry = topology_settings.default_geometry;
	topology_global.neighbours = neighbours_count();

	switch (topology_settings.default_geometry) {
		case TOPOLOGY_SQUARE:
		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_TORUS:
			edge = sqrt(n_prc_tot);
			topology_global.lp_cnt = edge * edge;
			break;
		default:
			edge = 0;
			topology_global.lp_cnt = n_prc_tot;
			break;
	}
	topology_global.edge = edge;
	topology_global.obstacles = NULL;
}

static void load_topology_file(const char *file_name) {
	char *json_base;
	jsmntok_t *root_token;
	c_jsmntok_t *t;

	unsigned lp_cnt, edge;
	enum _topology_type_t t_type;
	enum _topology_geometry_t geometry;

	// load and parse the file with given file_name
	if(load_and_parse_json_file(file_name, &json_base, &root_token) < 0)
		rootsim_error(true, "The specified topology file at \"%s\" is either non accessible, non existing, or is not a properly formed JSON file", file_name);

	if(parse_unsigned_by_key(root_token, json_base, root_token, "regions_count", &lp_cnt) < 0)
		rootsim_error(true, "Invalid or missing json value with key \"regions_count\" (must be an unsigned integer)");
	// sanity checks on the number of instantiated LPs
	if(lp_cnt > n_prc_tot)
		rootsim_error(true, "This topology needs an higher number of available LPs (%lu versus %lu available LPs)", lp_cnt, n_prc_tot);
	// we can continue normal processing but we inform the user about this "anomaly" (could be intended though!)
	if(lp_cnt < n_prc_tot) {
		rootsim_error(false, "Warning: the requested regions are fewer "
				"than the available LPs (%lu versus %lu available LPs)", lp_cnt, n_prc_tot);
		// we have to exit if we are processing a non included LP
		if(CURRENT_LP_ID >= lp_cnt)
			rootsim_error(true, "Instantiating a topology from a not included region!!!");
	}

	topology_global.lp_cnt = lp_cnt;

	// look for the topology type
	const char *type_choices[] = {
			[TOPOLOGY_COSTS] = 	"costs",
			[TOPOLOGY_OBSTACLES] = 	"obstacles",
			[TOPOLOGY_PROBABILITIES] = "probabilities"
	};

	t = get_value_token_by_key(root_token, json_base, root_token, "type");

	if((t_type = parse_string_choice(root_token, json_base, t, sizeof(type_choices)/sizeof(const char *), type_choices)) == UINT_MAX)
		rootsim_error(true, "Invalid or missing json value with key \"type\" (must be a recognizable string)");

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

	t = get_value_token_by_key(root_token, json_base, root_token, "geometry");

	if((geometry = parse_string_choice(root_token, json_base, t, sizeof(geometry_choices)/sizeof(const char *), geometry_choices)) == UINT_MAX)
		rootsim_error(true, "Invalid or missing json value with key \"geometry\" (must be a recognizable string)");

	geometry += TOPOLOGY_GEOMETRY_OFFSET;

	topology_global.geometry = geometry;

	// in case of a pseudo square geometry we precompute the edge of the map
	switch (geometry) {
		case TOPOLOGY_SQUARE:
		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_TORUS:
			edge = sqrt(lp_cnt);
			if(edge * edge != lp_cnt)
				rootsim_error(true, "Invalid number of regions for this topology geometry (must be a square number)");
			break;
		default:
			edge = 0;
			break;
	}

	topology_global.edge = edge;
	topology_global.neighbours = neighbours_count();

	switch (t_type) {
		case TOPOLOGY_PROBABILITIES:
			load_topology_file_probabilities(root_token, json_base);
			break;

		case TOPOLOGY_COSTS:
			load_topology_file_costs(root_token, json_base);
			break;

		case TOPOLOGY_OBSTACLES:
			load_topology_file_obstacles(root_token, json_base);
			break;
	}

	rsfree(root_token);
	rsfree(json_base);
}

void topology_preinit(void) {
	if(!&topology_settings) // if the weak symbol isn't defined we aren't needed
		return;

	if(topology_settings.topology_path)
		load_topology_file(topology_settings.topology_path);
	else
		load_default_topology();
}

void topology_init(void) {
	if(!&topology_settings) // if the weak symbol isn't defined we aren't needed
		return;

	switch(topology_settings.type){
		case TOPOLOGY_COSTS:
			topology_costs_init();
			break;
		case TOPOLOGY_OBSTACLES:
			topology_obstacles_init();
			break;
		case TOPOLOGY_PROBABILITIES:
			topology_probabilities_init();
			break;
	}
}

void SetValueTopology(unsigned from, unsigned to, double value) {
	switch_to_platform_mode();
	const unsigned lp_cnt = topology_global.lp_cnt;

	if(from >= lp_cnt || to >= lp_cnt)
		rootsim_error(true, "SetValueTopology(): from % u, to %u when lp_cnt is %u", from, to, lp_cnt);

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
		case INIT:
			topology_init();
			/* fall through */
		default:
			switch_to_application_mode();
			ProcessEvent[lid_to_int(current_lp)](CURRENT_LP_ID, current_evt->timestamp,
					current_evt->type, current_evt->event_content, current_evt->size, current_state);
			switch_to_platform_mode();
	}
}

unsigned int RegionsCount(void) {
	return topology_global.lp_cnt;
}

unsigned int NeighboursCount(void) {
	// TODO
	return topology_global.neighbours;
}

unsigned int ActualNeighboursCount(void){
	unsigned actual_neighbours = 0;
	unsigned i = topology_global.neighbours;
	while(i--)
		if(GetReceiver(CURRENT_LP_ID, i) != DIRECTION_INVALID)
			++actual_neighbours;

	return actual_neighbours;
}

unsigned int GetReceiver(unsigned int from, direction_t direction) {
	switch_to_platform_mode();

	unsigned int x, y;
	GID_t receiver_gid;
	// const so we don't accidentally modify it
	const unsigned sender = from;
	// the pre-computed edge length for geometries which require it
	const unsigned edge = topology_global.edge;
	// the count of lp involved in the topology
	const unsigned lp_cnt = topology_global.lp_cnt;
	// sanity check
	if(lp_cnt <= sender)
		rootsim_error(true, "GetReceiver(): region argument not included in topology!");

	// without modifications we would fail
	set_gid(receiver_gid, DIRECTION_INVALID);

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

			if(x < edge && y < edge)
				set_gid(receiver_gid, y * edge + x);

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

			if(x < edge && y < edge)
				set_gid(receiver_gid, y * edge + x);

			break;

		case TOPOLOGY_TORUS:

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

			// Check for wrapping around
			if(x >= edge)
				x -= edge;
			if(y >= edge)
				y -= edge;

			// Convert back to linear coordinates
			set_gid(receiver_gid, y * edge + x);

			break;

		case TOPOLOGY_GRAPH:
			if(direction < lp_cnt)
				set_gid(receiver_gid, direction);
			break;

		case TOPOLOGY_BIDRING:

			if(direction == DIRECTION_W) {
				if(sender == 0) {
					set_gid(receiver_gid, lp_cnt - 1);
				} else {
					set_gid(receiver_gid, sender - 1);
				}
			} else if(direction == DIRECTION_E) {
				if(sender + 1 == lp_cnt)
					set_gid(receiver_gid, 0);
				else
					set_gid(receiver_gid, sender + 1);
			}

			break;

		case TOPOLOGY_RING:

			if(direction == DIRECTION_E) {
				set_gid(receiver_gid, sender + 1);

				if(gid_to_int(receiver_gid) == lp_cnt) {
					set_gid(receiver_gid, 0);
				}
			}

			break;

		case TOPOLOGY_STAR:
			if(sender) {
				if(direction)
					goto out;
				set_gid(receiver_gid, 0);
			} else {
				if(direction + 1 >= lp_cnt)
					goto out;
				set_gid(receiver_gid, direction + 1);
			}

			break;

		default:
			rootsim_error(true, "This shouldn't happen. Aborting...");
	}

	out:
	switch_to_application_mode();
	return gid_to_int(receiver_gid);

}

unsigned int FindReceiver(void) {
	switch_to_platform_mode();

	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned receiver = DIRECTION_INVALID;

	if(lp_cnt <= CURRENT_LP_ID)
		rootsim_error(true, "FindReceiver(): source region %u when topology includes %u regions", CURRENT_LP_ID, lp_cnt);

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
	if(to >= lp_cnt || CURRENT_LP_ID >= lp_cnt)
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

	unsigned hops = 1; // we need at least one hop (we disallow ComputeMinTour() from and to the same region)

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

	if(source >= lp_cnt) {
		rootsim_error(true, "Invalid source passed to ComputeMinTour(): %u.\n", source);
		return UINT_MAX;
	}
	if(dest >= lp_cnt) {
		rootsim_error(true, "Invalid destination passed to ComputeMinTour(): %u.\n", dest);
		return UINT_MAX;
	}
	if(source == dest) {
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
