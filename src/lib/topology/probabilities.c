/*
 * topology_probabilities.c
 *
 *  Created on: 02 ago 2018
 *      Author: andrea
 */

/*
 * topology_custom.c
 *
 *  Created on: 24 mag 2018
 *      Author: andrea
 */

#include <ROOT-Sim.h>
#include <lib/topology.h>

#include <scheduler/scheduler.h>
#include <lib/numerical.h>
#include <datatypes/bitmap.h>
#include <scheduler/process.h>

typedef struct _topology_t {
	unsigned neighbours_id[6];
	bool dirty;
	double data[]; 			/// the costs, probabilities or obstacles matrix (depending on the topology type)
} topology_t;

unsigned size_checkpoint_probabilities(void){
	return 	sizeof(topology_t) + 					// the basic struct size
		sizeof(double) * (topology_global.directions + 1) + 	// the row of exit probabilities weights
		sizeof(double); 					// the cache of the sum of probabilities weights
}

void *load_topology_file_probabilities(c_jsmntok_t *root_token, const char *json_base){
	unsigned i;
	c_jsmntok_t *aux_tok;
	const unsigned lp_cnt = topology_global.lp_cnt;

	// number of possible exit regions for this region, we add 1 to take in consideration the self loop
	const unsigned exit_regions = topology_global.directions + 1;

	// retrieve the values array
	c_jsmntok_t *values_tok = get_value_token_by_key(root_token, json_base, root_token, "values");
	if(!values_tok|| values_tok->type != JSMN_ARRAY || children_count_token(root_token, values_tok) != lp_cnt)
		rootsim_error(true, "Invalid or missing json value with key \"values\"");

	// instantiates the array
	double *ret_data = rsalloc(sizeof(double) * exit_regions * lp_cnt);

	// we get the array of tokens we are interested in
	for(i = 0; i < lp_cnt; ++i){
		aux_tok = get_at_token(root_token, values_tok, i);

		// we parse the array
		if(!aux_tok || parse_double_array(root_token, json_base, aux_tok, exit_regions, &ret_data[i * exit_regions]) < 0)
			rootsim_error(true, "Invalid or missing value in the array of probabilities for this region");
	}

	// sanity check on the values of the probability weights
	for (i = 0; i < exit_regions * lp_cnt; ++i) {
		if(ret_data[i] < 0)
			rootsim_error(true, "Found a negative probability weight in the topology file!");
	}
	return ret_data;
}


topology_t *topology_probabilities_init(unsigned this_region_id, void *topology_data){
	// get number of possible exit regions for this region, we add 1 to take in consideration the self loop
	const unsigned exit_regions = topology_global.directions + 1;
	unsigned i;
	// instantiate the topology struct
	topology_t *topology = rsalloc(topology_global.chkp_size);

	if(topology_data)
		memcpy(topology->data, ((char *) topology_data) + this_region_id * exit_regions, sizeof(double) * exit_regions);
	else{
		// most models assume that you don't select the region you are from
		topology->data[0] = 0.0;
		for(i = 1; i < exit_regions; ++i)
			topology->data[i] = 1.0;
	}

	if(topology_global.geometry != TOPOLOGY_GRAPH){
		i = topology_global.directions;
		while(i--)
			topology->neighbours_id[i] = get_raw_receiver(this_region_id, i);
	}

	topology->dirty = true;
	return topology;
}


static void refresh_cache_probabilities(topology_t *topology){
	if(topology->dirty){
		// +1: remember we hold also the probability of self loop
		const unsigned exit_regions = topology_global.directions + 1;
		topology->data[exit_regions] = NeumaierSum(exit_regions, topology->data);
		topology->dirty = false;
	}
}

struct update_topology_t{
	unsigned loc_i;
	double val;
};

void set_value_topology_probabilities(unsigned from, unsigned to, double value) {
	topology_t *topology = current->topology;
	if(current->gid.to_int == from){
		topology->data[to] = value;
		topology->dirty = true;
	}else {
		struct update_topology_t upd = {to, value};
		UncheckedScheduleNewEvent(from, current_evt->timestamp, TOPOLOGY_UPDATE, &upd, sizeof(upd));
	}
}

bool is_reachable_probabilities(unsigned to){
	topology_t *topology = current->topology;
	unsigned i = topology_global.directions;
	switch (topology_global.geometry) {
		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_SQUARE:
		case TOPOLOGY_TORUS:
		case TOPOLOGY_BIDRING:
			while(i--){
				if(topology->neighbours_id[i] == to){
					if(topology->data[i] > 0)
						return true;
					break;
				}
			}
			break;

		case TOPOLOGY_RING:
			rootsim_error(true, "Topology ring still not supported!");
			break;

		case TOPOLOGY_GRAPH:
			return topology->data[i] > 0;

		default:
			rootsim_error(true, "This shouldn't happen, report to maintainer");

	}

	return false;
}

void update_topology_probabilities(void){
	topology_t *topology = current->topology;
	struct update_topology_t *upd_p = (struct update_topology_t *)current_evt->event_content;
	if(topology->data[upd_p->loc_i] > upd_p->val || topology->data[upd_p->loc_i] < upd_p->val){
		topology->data[upd_p->loc_i] = upd_p->val;
		topology->dirty = true;
	}
}

double get_value_topology_probabilities(unsigned from, unsigned to) {
	if(current->gid.to_int == from){
		return current->topology->data[to];
	}else {
		// TODO remote retrieve value!!
		// this could be real tricky and costly to implement!!
		rootsim_error(true, "getting a remote probability value is still not supported... !");
	}

	return 0.0;
}

// this code right here is used multiple times:
// it randomly chooses a direction weighted on the probabilities values
// if 0 is choosen it means we decided to stay where we were and so we set receiver
// and we exit else we continue processing. Notice that the direction value has different
// meanings in different geometries. FIXME with better approach, this one uses too many calls to random()
#define select_direction()	\
		do{\
			direction = exit_regions * Random();\
		}while(Random() * sum >= topology->data[direction]);\
		if(!direction) \
			return sender; \
		direction--;

unsigned int find_receiver_probabilities(void) {
	topology_t *topology = current->topology;
	unsigned int direction, receiver = DIRECTION_INVALID;
	const unsigned sender = current->gid.to_int;
	double sum;

	refresh_cache_probabilities(topology);

	// we sum 1 to take into account the possibility to stay where we are
	const unsigned exit_regions = topology_global.directions + 1;
	// the sum has been computed during refresh_cache if needed.
	sum = topology->data[exit_regions];
	if(sum <= 0)
		return DIRECTION_INVALID;

	switch (topology_global.geometry) {

		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_SQUARE:
			// Find a random neighbour
			do {
				select_direction();

				receiver = topology->neighbours_id[direction];
				// we simply repeat this until we either select ourselves or a valid neighbour
			} while (receiver == DIRECTION_INVALID);
			break;

		case TOPOLOGY_TORUS:
		case TOPOLOGY_BIDRING:
		case TOPOLOGY_RING:
			select_direction();

			receiver = topology->neighbours_id[direction];
			break;

		case TOPOLOGY_GRAPH:
			// here we don't use the select_direction() macro because we don't map direction 0
			// to the region we occupy because this way we simplify the logic.
			do {
				direction = exit_regions * Random();
			} while (Random() * sum >= topology->data[direction]);

			//receiver = GetReceiver(topology, sender, direction);
			receiver = direction;
			break;

		case TOPOLOGY_STAR:
			select_direction();

			if(sender == 0) {
				receiver = direction;
			} else {
				receiver = 0;
			}
			break;

		default:
			rootsim_error(true, "Wrong topology code specified: %d. Aborting...\n", topology);
	}

	return receiver;
}

#undef select_direction

