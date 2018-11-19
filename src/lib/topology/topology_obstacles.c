/*
 * topology_binary.c
 *
 *  Created on: 02 ago 2018
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

#define CURRENT_TOPOLOGY  	(LPS(current_lp)->topology)

#define CURRENT_LP_ID		(gid_to_int(LidToGid(current_lp)))

typedef struct _topology_t {
	bool dirty;
	unsigned *prev_next_cache;
	rootsim_bitmap data[]; 			/// the costs, probabilities or obstacles matrix (depending on the topology type)
} topology_t;

void load_topology_file_obstacles(c_jsmntok_t *root_token, const char *json_base){
	unsigned i;
	double tmp;
	const unsigned lp_cnt = topology_global.lp_cnt;
	// retrieve the values array
	c_jsmntok_t *values_tok = get_value_token_by_key(root_token, json_base, root_token, "values");
	if(!values_tok|| values_tok->type != JSMN_ARRAY || children_count_token(root_token, values_tok) != lp_cnt)
		rootsim_error(true, "Invalid or missing json value with key \"values\"");

	topology_global.obstacles = __wrap_malloc(bitmap_required_size(lp_cnt));
	bitmap_initialize(topology_global.obstacles, lp_cnt);

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;
	for (i = 0; i < lp_cnt; ++i) {
		if(parse_double_token(json_base, get_next_token(root_token, values_tok, &closure), &tmp) < 0)
			rootsim_error(true, "Invalid value found in the \"value\" array");

		if(tmp >= -1.0 && tmp <= -1.0) {bitmap_set(topology_global.obstacles, i);}
	}
}


void topology_obstacles_init(void){
	const unsigned lp_cnt = topology_global.lp_cnt;

	topology_t* topology = __wrap_malloc(
			sizeof(topology_t) + // the basic struct size
			bitmap_required_size(lp_cnt) + // the bitmap to hold obstacles
			sizeof(unsigned) * 2 * lp_cnt); // the cache for next previous hops

	topology->prev_next_cache = (unsigned*)(((char*)topology->data) + bitmap_required_size(lp_cnt));

	if(topology_global.obstacles)
		memcpy(topology->data, topology_global.obstacles, bitmap_required_size(lp_cnt));
	else{
		bitmap_initialize(topology->data, lp_cnt);
	}
	topology->dirty = true;
	CURRENT_TOPOLOGY = topology;
}


// helper structure, we use this as heap elements to keep track of vertexes status during dijkstra execution
struct _dijkstra_h_t{
	unsigned hops;
	unsigned cell;
};

#define __cmp_dijkstra_h(a, b) (((a).hops > (b).hops) - ((b).hops > (a).hops))
// this is costly: we try as much as possible to cache the results of this function
static void dijkstra_obstacles(const topology_t *topology, unsigned int source_cell, unsigned int previous[RegionsCount()]) {
	const unsigned neighbours = topology_global.neighbours;
	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned i, receiver;
	unsigned min_costs[lp_cnt];
	rootsim_heap(struct _dijkstra_h_t) heap;
	const rootsim_bitmap *obstacles = topology->data;

	struct _dijkstra_h_t current = {0, source_cell}, partial = {0};
	// initialize regions as unreachable
	i = lp_cnt;
	while(i--){
		min_costs[i] = UINT_MAX;
		previous[i] = UINT_MAX;
	}
	// heap init
	heap_init(heap);
	// the source cell has distance 0
	min_costs[source_cell] = 0;
	// textbook dijkstra (keep in mind i'm not passing pointers, this stuff gets copied)
	heap_insert(heap, current, __cmp_dijkstra_h);
	// while we have vertexes to process
	while(!heap_empty(heap)) {
		// extract the lowest one
		current = heap_extract(heap, __cmp_dijkstra_h);
		// since we are not supporting decrease key on the heap we have to filter spurious duplicates
		if(current.hops > min_costs[current.cell])
			continue;
		// we compute the sum of the current distance plus one hop to the receiver
		partial.hops = current.hops + 1;
		// we cycle through the neighbours of the current cell
		for(i = 0; i < neighbours; ++i){
			// we get the receiver cell
			receiver = GetReceiver(current.cell, i);
			// obviously we want a valid neighbour
			if(receiver == DIRECTION_INVALID || bitmap_check(obstacles, receiver))
				continue;
			// if lower we have a candidate optimum for the cell
			if(partial.hops < min_costs[receiver]){
				// set the previous cell to retrieve the path later on
				previous[receiver] = current.cell;
				// we set the cell field on our struct
				partial.cell = receiver;
				// refresh lowest cost found for the cell
				min_costs[receiver] = partial.hops;
				// we insert this into the heap
				heap_insert(heap, partial, __cmp_dijkstra_h);
			}
		}
	}
}
#undef __cmp_dijsktra_h

static void refresh_cache_obstacles(topology_t *topology){
	if(topology->dirty){
		const unsigned lp_cnt = topology_global.lp_cnt;

		dijkstra_obstacles(topology, CURRENT_LP_ID, topology->prev_next_cache);
		// this sets to an uninitialized value the buffer which holds the next hop for
		// each possible destination (we compute those on demand when asked by the user and we cache those here)
		memset(&topology->prev_next_cache[lp_cnt], UCHAR_MAX, sizeof(unsigned) * lp_cnt);
		topology->dirty = false;
	}
}

unsigned int find_receiver_obstacles(void) {
	rootsim_bitmap *obstacles = CURRENT_TOPOLOGY->data;
	const unsigned sender = CURRENT_LP_ID;
	if(bitmap_check(obstacles, sender)){
		rootsim_error(false, "FindReceiver(): this is an obstacle region!!!\n");
		return DIRECTION_INVALID;
	}

	const unsigned exit_regions = topology_global.lp_cnt + 1;
	unsigned receiver, direction;

	switch (topology_global.geometry) {

		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_SQUARE:
		case TOPOLOGY_TORUS:
		case TOPOLOGY_BIDRING:
		case TOPOLOGY_RING:
		case TOPOLOGY_STAR:
			do{
				direction = exit_regions * Random();
				if(!direction) {
					receiver = sender;
					goto out;
				}
				receiver = GetReceiver(sender, direction - 1);
			}while(receiver == DIRECTION_INVALID || bitmap_check(obstacles, receiver));
			break;

		case TOPOLOGY_GRAPH:
			do {
				direction = exit_regions * Random();
				//receiver = GetReceiver(topology, sender, direction);
				receiver = direction;
			} while (bitmap_check(obstacles, receiver));

			break;

		default:
			rootsim_error(true, "Wrong topology geometry specified: %d. Aborting...\n", topology_settings.type);
	}

	out: switch_to_application_mode();
	return receiver;
}

struct update_topology_t{
	unsigned lp;
	bool val;
};

void set_value_topology_obstacles(unsigned from, unsigned to, double value){
	(void)to;

	topology_t *topology = CURRENT_TOPOLOGY;

	if(value < 0)
		rootsim_error(true, "SetValueTopology(): Negative costs are not supported", value);

	rootsim_bitmap *bitmap = ((union {double *d_p; rootsim_bitmap *r_p;})(topology->data)).r_p;

	struct update_topology_t upd = {from, (value > 0) ^ bitmap_check(bitmap, from)};

	(value > 0) ? bitmap_set(bitmap, from) : bitmap_reset(bitmap, from);

	unsigned i = topology_global.lp_cnt;
	while(i--){
		if(i == CURRENT_LP_ID)
			continue;
		UncheckedScheduleNewEvent(i, current_evt->timestamp, TOPOLOGY_UPDATE, &upd, sizeof(upd));
	}

	topology->dirty = true;
}

void update_topology_obstacles(void){
	topology_t *topology = CURRENT_TOPOLOGY;

	rootsim_bitmap *bitmap = ((union {double *d_p; rootsim_bitmap *r_p;})(topology->data)).r_p;
	struct update_topology_t *upd_p = (struct update_topology_t *)current_evt->event_content;

	(upd_p->val ^ bitmap_check(bitmap, upd_p->lp)) ? bitmap_set(bitmap, upd_p->lp) : bitmap_reset(bitmap, upd_p->lp);

	topology->dirty = true;
}

double get_value_topology_obstacles(unsigned from, unsigned to){
	(void)to;
	return bitmap_check(CURRENT_TOPOLOGY->data, from) ? 1.0 : 0.0;
}


unsigned int find_receiver_toward_obstacles(unsigned int to){
	topology_t *topology = CURRENT_TOPOLOGY;
	const unsigned lp_cnt = topology_global.lp_cnt;
	const unsigned this_lp = CURRENT_LP_ID;

	refresh_cache_obstacles(topology);

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

	if(*ret_p == this_lp){
		// the requested cell is unreachable
		return UINT_MAX;
	}

	return *ret_p;
}

double compute_min_tour_obstacles(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]) {
	topology_t *topology = CURRENT_TOPOLOGY;
	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned int previous[lp_cnt], hops;

	if(source == CURRENT_LP_ID){
		refresh_cache_obstacles(topology);
		if(!(hops = build_path(lp_cnt, result, topology->prev_next_cache, source, dest))){
			// the requested cell isn't reachable: this is a sentinel value indicating impossibility
			topology->prev_next_cache[lp_cnt + dest] = source;
			return -1.0;
		}
		topology->prev_next_cache[lp_cnt + dest] = result[0]; // we cache the next hop value;
		return hops;
	}

	dijkstra_obstacles(topology, source, previous);

	if(!(hops = build_path(lp_cnt, result, previous, source, dest)))
		return -1.0;

	return hops;
}
