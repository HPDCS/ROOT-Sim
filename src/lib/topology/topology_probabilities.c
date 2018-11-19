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

#define CURRENT_TOPOLOGY  	(LPS(current_lp)->topology)

#define CURRENT_LP_ID		(gid_to_int(LidToGid(current_lp)))

typedef struct _topology_t {
	bool dirty;
	double data[]; 			/// the costs, probabilities or obstacles matrix (depending on the topology type)
} topology_t;

void load_topology_file_probabilities(c_jsmntok_t *root_token, const char *json_base){
	unsigned i;
	c_jsmntok_t *aux_tok;
	const unsigned lp_cnt = topology_global.lp_cnt;
	// number of possible exit regions for this region, we add 1 to take in consideration the self loop
	const unsigned exit_regions = topology_global.neighbours + 1;

	// retrieve the values array
	c_jsmntok_t *values_tok = get_value_token_by_key(root_token, json_base, root_token, "values");
	if(!values_tok|| values_tok->type != JSMN_ARRAY || children_count_token(root_token, values_tok) != lp_cnt)
		rootsim_error(true, "Invalid or missing json value with key \"values\"");

	// instantiates the array
	topology_global.weights = rsalloc(sizeof(double) * exit_regions * lp_cnt);

	// we get the array of tokens we are interested in
	for(i = 0; i < lp_cnt; ++i){
		aux_tok = get_at_token(root_token, values_tok, CURRENT_LP_ID);

		// we parse the array
		if(!aux_tok || parse_double_array(root_token, json_base, aux_tok, exit_regions, &topology_global.weights[i * exit_regions]) < 0)
			rootsim_error(true, "Invalid or missing value in the array of probabilities for this region");
	}

	// sanity check on the values of the probability weights
	for (i = 0; i < exit_regions * lp_cnt; ++i) {
		if(topology_global.weights[i] < 0)
			rootsim_error(true, "Found a negative probability weight in the topology file!");
	}
}


void topology_probabilities_init(void){
	unsigned i;
	// get number of possible exit regions for this region, we add 1 to take in consideration the self loop
	const unsigned exit_regions = topology_global.neighbours + 1;

	// instantiate the topology struct
	topology_t *topology = __wrap_malloc(
			sizeof(topology_t) + // the basic struct size
			sizeof(double) * exit_regions + // the row of exit probabilities weights
			sizeof(double)); // the cache of the sum of probabilities weights

	if(topology_global.weights)
		memcpy(topology->data, topology_global.weights + CURRENT_LP_ID * exit_regions, sizeof(double) * exit_regions);
	else{
		// most models assume that you don't select the region you are from
		topology->data[0] = 0.0;
		for(i = 1; i < exit_regions; ++i) {
			topology->data[i] = 1.0;
		}
	}
	topology->dirty = true;
	CURRENT_TOPOLOGY = topology;
}


static void refresh_cache_probabilities(topology_t *topology){
	if(topology->dirty){
		// +1: remember we hold also the probability of self loop
		const unsigned exit_regions = topology_global.neighbours + 1;
		topology->data[exit_regions] = NeumaierSum(exit_regions, topology->data);
		topology->dirty = false;
	}
}

struct update_topology_t{
	unsigned loc_i;
	double val;
};

void set_value_topology_probabilities(unsigned from, unsigned to, double value) {
	topology_t *topology = CURRENT_TOPOLOGY;
	if(CURRENT_LP_ID == from){
		topology->data[to] = value;
		topology->dirty = true;
	}else {
		struct update_topology_t upd = {to, value};
		UncheckedScheduleNewEvent(from, current_evt->timestamp, TOPOLOGY_UPDATE, &upd, sizeof(upd));
	}
}

void update_topology_probabilities(void){
	topology_t *topology = CURRENT_TOPOLOGY;
	struct update_topology_t *upd_p = (struct update_topology_t *)current_evt->event_content;
	topology->data[upd_p->loc_i] = upd_p->val;
	topology->dirty = true;
}

double get_value_topology_probabilities(unsigned from, unsigned to) {
	if(CURRENT_LP_ID == from){
		return CURRENT_TOPOLOGY->data[to];
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
		if(!direction) {\
			receiver = sender;\
			goto out;\
		}\
		direction--;

unsigned int find_receiver_probabilities(void) {
	topology_t *topology = CURRENT_TOPOLOGY;
	unsigned int direction, receiver = DIRECTION_INVALID;
	const unsigned sender = CURRENT_LP_ID;
	double sum;

	refresh_cache_probabilities(topology);

	// we sum 1 to take into account the possibility to stay where we are
	const unsigned exit_regions = topology_global.neighbours + 1;
	// the sum has been computed during refresh_cache if needed.
	sum = topology->data[exit_regions];

	switch (topology_global.geometry) {

		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_SQUARE:
			// Find a random neighbour
			do {
				select_direction();

				receiver = GetReceiver(sender, direction);
				// we simply repeat this until we either select ourselves or a valid neighbour
			} while (receiver == DIRECTION_INVALID);
			break;

		case TOPOLOGY_TORUS:
		case TOPOLOGY_BIDRING:
		case TOPOLOGY_RING:
			select_direction();

			receiver = GetReceiver(sender, direction);
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

	out:
	return receiver;

}

#undef select_direction

