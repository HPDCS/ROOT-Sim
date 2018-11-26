/*
 * topology_costs.c
 *
 *  Created on: 02 ago 2018
 *      Author: andrea
 */

#include <ROOT-Sim.h>
#include <lib/topology.h>

#include <math.h>
#include <stdint.h>

#include <scheduler/scheduler.h>
#include <lib/jsmn_helper.h>
#include <lib/numerical.h>
#include <datatypes/bitmap.h>
#include <datatypes/array.h>
#include <datatypes/heap.h>
#include <scheduler/process.h>

#define CURRENT_TOPOLOGY  	(LPS(current_lp)->topology)

#define CURRENT_LP_ID		(gid_to_int(LidToGid(current_lp)))

typedef struct _topology_t {
	bool dirty;
	unsigned *prev_next_cache; 	/// a pointer to the cache used to speedup queries on paths (unused in probabilities topology type)
	double data[]; 			/// the costs, probabilities or obstacles matrix (depending on the topology type)
} topology_t;

unsigned size_checkpoint_costs(void){
	return	sizeof(topology_t) + 							// the basic struct size
		sizeof(double) * topology_global.neighbours * topology_global.lp_cnt + 	// the whole cost matrix
		sizeof(double) * topology_global.lp_cnt + 				// the cache of total path costs to speed up queries
		sizeof(unsigned) * 2 * topology_global.lp_cnt; 				// the cache of previous and next hops to speed up queries
}

void load_topology_file_costs(c_jsmntok_t *root_token, const char *json_base){
	unsigned i;
	c_jsmntok_t *aux_tok;
	const unsigned lp_cnt = topology_global.lp_cnt;
	const unsigned neighbours_cnt = topology_global.neighbours;

	// retrieve the values array
	c_jsmntok_t *values_tok = get_value_token_by_key(root_token, json_base, root_token, "values");
	if(!values_tok|| values_tok->type != JSMN_ARRAY || children_count_token(root_token, values_tok) != lp_cnt)
		rootsim_error(false, "Invalid or missing json value with key \"values\"");

	// instantiates the array
	topology_global.costs = rsalloc(sizeof(double) * neighbours_cnt * lp_cnt);

	// we iterate and store the costs of going into a neighbour
	for (i = 0; i < lp_cnt; ++i) {
		// get the token of the lp we are scanning
		aux_tok = get_at_token(root_token, values_tok, i);

		// we parse the array
		if(!aux_tok || parse_double_array(root_token, json_base, aux_tok, neighbours_cnt, &topology_global.weights[i * neighbours_cnt]) < 0)
			rootsim_error(false, "Invalid or missing value in the current array of costs");
	}

	// sanity check on the values of the costs
	// XXX could negative costs be useful to someone?
	// they would require non-minimal work to implement
	// Bellman-Ford would be required
	for (i = 0; i < neighbours_cnt * lp_cnt; ++i) {
		if(topology_global.weights[i] < 0) {
			// xxx if negative costs need to be implemented we can't do this
			if(topology_global.weights[i] > -1.0 || topology_global.weights[i] < -1.0)
				rootsim_error(true, "Negative costs are still not supported");

			topology_global.weights[i] = INFINITY; // this way we can mark an edge as non crossable
		}
	}
}


void topology_costs_init(void){
	unsigned i;
	const unsigned neighbours = topology_global.neighbours;
	const unsigned lp_cnt = topology_global.lp_cnt;

	// instantiate the topology struct
	topology_t *topology = __wrap_malloc(topology_global.chkp_size);

	// from now on we expect a topology based on cost to reside in a unique memory block and to have this layout in memory:
	// BASE_STRUCT | COST MATRIX | CACHE MINIMUM COSTS | CACHE PREVIOUS HOP | CACHE NEXT HOP
	topology->prev_next_cache = ((union {
		double *d_p;
		unsigned *u_p;
	}) &topology->data[(neighbours + 1) * lp_cnt]).u_p;

	if(topology_global.weights)
		memcpy(topology->data, topology_global.weights, sizeof(double) * neighbours * lp_cnt);
	else{
		for(i = 0; i < neighbours * lp_cnt; ++i)
			topology->data[i] = 1.0;
	}
	topology->dirty = true;
	CURRENT_TOPOLOGY = topology;
}


// helper structure, we use this as heap elements to keep track of vertexes status during dijkstra execution
struct _dijkstra_h_t{
	struct _sum_helper_t cost;
	unsigned cell;
};
// this is used in order to sort the elements of the heap
#define __cmp_dijkstra_h(a, b) CmpSumHelpers(a.cost, b.cost)
// this is costly: we try as much as possible to cache the results of this function
static void dijkstra_costs(const topology_t *topology, unsigned int source_cell, double min_costs[RegionsCount()], unsigned int previous[RegionsCount()]) {
	const unsigned neighbours = topology_global.neighbours;
	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned i, receiver;
	struct _sum_helper_t min_costs_h[lp_cnt];
	rootsim_heap(struct _dijkstra_h_t) heap;
	const double *costs = topology->data;
	struct _dijkstra_h_t partial = {{0, 0}, source_cell}, current;

	i = lp_cnt;
	while(i--){
		min_costs_h[i].crt = 0;
		min_costs_h[i].sum = INFINITY;
		previous[i] = UINT_MAX;
	}

	heap_init(heap);

	min_costs_h[source_cell] = partial.cost;
	// textbook dijkstra (keep in mind i'm not passing pointers, this stuff gets copied)
	heap_insert(heap, partial, __cmp_dijkstra_h);
	// while we have vertexes to process
	while(!heap_empty(heap)) {
		// extract the lowest one
		current = heap_extract(heap, __cmp_dijkstra_h);
		// since we are not supporting decrease key on the heap we have to filter spurious duplicates
		if(CmpSumHelpers(current.cost, min_costs_h[current.cell]) > 0)
			continue;
		// we cycle through the neighbours of the current cell
		for(i = 0; i < neighbours; ++i){
			// we get the receiver cell
			receiver = get_raw_receiver(current.cell, i);
			if(receiver == DIRECTION_INVALID || isinf(costs[current.cell * neighbours + i]))
				continue;
			// we compute the sum of the current distance plus one hop to the receiver
			partial.cost = PartialNeumaierSum(current.cost, costs[current.cell * neighbours + i]);
			// if lower we have a candidate optimum for the cell
			if(CmpSumHelpers(partial.cost, min_costs_h[receiver]) < 0){
				// set the previous cell to retrieve the path later on
				previous[receiver] = current.cell;
				// we set the cell field on our struct
				partial.cell = receiver;
				// refresh lowest cost found for the cell
				min_costs_h[receiver] = partial.cost;
				// we insert this into the heap
				heap_insert(heap, partial, __cmp_dijkstra_h);
			}
		}
	}
	// we transform the sum helpers into single double value
	i = lp_cnt;
	while(i--){
		min_costs[i] = ValueSumHelper(min_costs_h[i]);
	}
}
#undef __cmp_dijsktra_h

static void refresh_cache_costs(topology_t *topology){
	if(topology->dirty){
		const unsigned lp_cnt = topology_global.lp_cnt;
		// computes minimum cost spanning tree rooted in the current region
		dijkstra_costs(topology, CURRENT_LP_ID, &topology->data[topology_global.neighbours*lp_cnt], topology->prev_next_cache);
		// this sets to an uninitialized value the buffer which holds the next hop for
		// each possible destination (we compute those on demand when asked by the user and we cache those here)
		memset(&topology->prev_next_cache[lp_cnt], UCHAR_MAX, sizeof(unsigned) * lp_cnt);
		topology->dirty = false;
	}
}

union _double_bits_trick{
	uint64_t val_bits; 	/// needed by the bitwise XOR
	double val;		/// the new cost value
};

struct update_topology_t{
	long unsigned loc_i;		/// where to put the new value
	union _double_bits_trick upd;
};

void set_value_topology_costs(unsigned from, unsigned to, double value){
	topology_t *topology = CURRENT_TOPOLOGY;
	struct update_topology_t to_send = {from * topology_global.neighbours + to, .upd = {value}};
	// this makes sure our bitwise tricks work properly
	static_assert(sizeof(double) == sizeof(uint64_t), "the bit operation trick on TOPOLOGY_COST updates is wrong");
	union _double_bits_trick old = {topology->data[to_send.loc_i]};
	// the XOR is needed in order to have a consistent state after contemporaneous update events
	to_send.upd.val_bits ^= old.val_bits;

	if(unlikely(to_send.upd.val_bits == 0))
		// the update is unnecessary
		return;
	// save the value locally
	topology->data[to_send.loc_i] = value;
	// send the update message to the other LPs
	unsigned i = topology_global.lp_cnt;
	while(i--){
		if(i == CURRENT_LP_ID)
			continue;
		UncheckedScheduleNewEvent(i, current_evt->timestamp, TOPOLOGY_UPDATE, &to_send, sizeof(to_send));
	}
	// mark the topology as dirty
	topology->dirty = true;
}

void update_topology_costs(void){
	topology_t *topology = CURRENT_TOPOLOGY;
	struct update_topology_t *upd_p = (struct update_topology_t *)current_evt->event_content;
	// same ol' trick as set_value_topology_costs()
	union _double_bits_trick *old_p = (union _double_bits_trick *)&topology->data[upd_p->loc_i];
	// update our value through the XOR
	old_p->val_bits ^= upd_p->upd.val_bits;
	// mark the topology as dirty
	topology->dirty = true;
}

double get_value_topology_costs(unsigned from, unsigned to){
	return CURRENT_TOPOLOGY->data[from * topology_global.neighbours + to];
}

bool is_reachable_costs(unsigned to){
	// XXX do we want deep reachability or dumb reachability?
	return find_receiver_toward_costs(to) != UINT_MAX;
}

unsigned int find_receiver_toward_costs(unsigned int to){
	topology_t *topology = CURRENT_TOPOLOGY;
	const unsigned lp_cnt = topology_global.lp_cnt;
	const unsigned this_lp = CURRENT_LP_ID;
	// refresh the cache
	refresh_cache_costs(topology);
	// this is the location where we expect to find the cached next hop
	unsigned *ret_p = &topology->prev_next_cache[lp_cnt + to];

	if(*ret_p == UINT_MAX){
		// this value hasn't been requested yet
		unsigned path[lp_cnt];
		// we compute the complete path
		if(!build_path(lp_cnt, path, topology->prev_next_cache, this_lp, to)){
			// there's no path so we put in a sentinel value
			*ret_p = this_lp;
		}else{
			// we got a feasible path, we save the first hop
			*ret_p = path[0];
		}
	}
	// we mark the unreachable regions with next hop = current LP id
	if(*ret_p == this_lp){
		// the requested cell is unreachable
		return UINT_MAX;
	}

	return *ret_p;
}

double compute_min_tour_costs(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]) {
	topology_t *topology = CURRENT_TOPOLOGY;
	const unsigned lp_cnt = topology_global.lp_cnt;

	// I suppose (I HOPE!!!) the requests will be more frequent for paths starting from the region where we are staying
	if(source == CURRENT_LP_ID){
		refresh_cache_costs(topology); // so we cache that stuff
		// the path is built on the fly (but this is almost as costly as copying it directly)
		if(!build_path(lp_cnt, result, topology->prev_next_cache, source, dest)){
			// the destination is unreachable
			// we mark the cache a sentinel value indicating impossibility
			topology->prev_next_cache[lp_cnt + dest] = source;
			return -1.0;
		}
		topology->prev_next_cache[lp_cnt + dest] = result[0]; // we cache the next hop value;
		// this is the cached value for the minimum cost incurred in the path
		return topology->data[topology_global.neighbours * lp_cnt + dest];
	}

	unsigned int previous[lp_cnt];
	double min_costs[lp_cnt];
	// this is not cahced, we need to execute the whole algorithm, again only for this request
	dijkstra_costs(topology, source, min_costs, previous);
	// we build the path
	if(!build_path(lp_cnt, result, previous, source, dest))
		return -1.0;

	return min_costs[dest];
}

