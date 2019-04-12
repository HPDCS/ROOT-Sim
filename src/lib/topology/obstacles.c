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
#include <datatypes/heap.h>
#include <scheduler/process.h>

/// the customised struct for TOPOLOGY_OBSTACLES representation
typedef struct _topology_t {
	unsigned neighbours_id[6];	/**< these are the cached neighbours IDs */
	unsigned free_neighbours;	/**< this is the number of reachable neighbours */
	unsigned *prev_next_cache;	/**< this is the cache of previous and next hops needed to reach a destination */
	bool dirty;			/**< this tells is the prev_next_cache is invalidated */
	rootsim_bitmap data[];		/**< this is the obstacles bitmap */
} topology_t;

unsigned size_checkpoint_obstacles(void){
	return 	sizeof(topology_t) + 				// the basic struct size
		bitmap_required_size(topology_global.lp_cnt) + 	// the bitmap to hold obstacles
		sizeof(unsigned) * 2 * topology_global.lp_cnt; 	// the cache for next and previous hops
}

// this is called once per machine after the general
void *load_topology_file_obstacles(c_jsmntok_t *root_token, const char *json_base){
	unsigned i;
	double tmp;
	const unsigned lp_cnt = topology_global.lp_cnt;
	// retrieve the values array checking its size against the lp_cnt in the topology
	c_jsmntok_t *values_tok = get_value_token_by_key(root_token, json_base, root_token, "values");
	if(!values_tok|| values_tok->type != JSMN_ARRAY || children_count_token(root_token, values_tok) != lp_cnt)
		rootsim_error(true, "Invalid or missing json value with key \"values\"");
	// we initialize the machine shared initial obstacles status
	rootsim_bitmap *ret_data = rsalloc(bitmap_required_size(lp_cnt));
	bitmap_initialize(ret_data, lp_cnt);
	// now we parse the json array to fill in the actual data
	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;
	for (i = 0; i < lp_cnt; ++i) {
		if(parse_double_token(json_base, get_next_token(root_token, values_tok, &closure), &tmp) < 0)
			rootsim_error(true, "Invalid value found in the \"value\" array");
		// we interpret ones as obstacles, we use the double comparison to avoid warnings
		if(tmp >= 1.0 && tmp <= 1.0) {bitmap_set(ret_data, i);}
	}
	return ret_data;
}


topology_t *topology_obstacles_init(unsigned this_region_id, void *topology_data){
	unsigned i, lp_id;
	const unsigned lp_cnt = topology_global.lp_cnt;

	// allocate the topology struct, we use a single allocation for all the stuff we need
	topology_t* topology = rsalloc(topology_global.chkp_size);

	topology->prev_next_cache = (unsigned *)(((char *)topology->data) + bitmap_required_size(lp_cnt));

	if(topology_data){
		memcpy(topology->data, topology_data, bitmap_required_size(lp_cnt));
	}else{
		bitmap_initialize(topology->data, lp_cnt);
	}

	i = topology_global.directions;
	topology->free_neighbours = i;
	if(topology_global.geometry != TOPOLOGY_GRAPH){
		// we save the neighbours ids for faster accessing
		while(i--){
			lp_id = get_raw_receiver(this_region_id, i);
			topology->neighbours_id[i] = lp_id;
			// we also count reachable neighbours
			if(lp_id == DIRECTION_INVALID || bitmap_check(topology->data, lp_id))
				topology->free_neighbours--;
		}
	}else{
		// in a graph directions are 1 to 1 with regions
		while(i--)
			if(this_region_id == i || bitmap_check(topology->data, i))
				topology->free_neighbours--;
	}

	topology->dirty = true;

	return topology;
}

#define __cmp_dijkstra_h(a, b) (((a).hops > (b).hops) - ((b).hops > (a).hops))
// this is costly: we try as much as possible to cache the results of this function
static void dijkstra_obstacles(const topology_t *topology, unsigned int source_cell, unsigned int previous[RegionsCount()]) {

	// helper structure, we use this as heap elements to keep track of vertexes status during dijkstra execution
	struct _dijkstra_h_t{
		unsigned hops;
		unsigned cell;
	};

	const unsigned directions = topology_global.directions;
	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned i, receiver;
	unsigned min_costs[lp_cnt];
	rootsim_heap(struct _dijkstra_h_t) heap;
	const rootsim_bitmap *obstacles = topology->data;

	struct _dijkstra_h_t current_scan = {0, source_cell}, partial_scan = {0};
	// initialize regions as unreachable
	i = lp_cnt;
	while(i--){
		min_costs[i] = UINT_MAX;
		previous[i] = UINT_MAX;
	}
	// heap init
	heap_init(heap);
	// the source cell has distance 0
	min_costs[source_cell] = current_scan.hops;
	// textbook dijkstra (keep in mind i'm not passing pointers, this stuff gets copied)
	heap_insert(heap, current_scan, __cmp_dijkstra_h);
	// while we have vertexes to process
	while(!heap_empty(heap)) {
		// extract the lowest one
		current_scan = heap_extract(heap, __cmp_dijkstra_h);
		// since we are not supporting decrease key on the heap we have to filter spurious duplicates
		if(current_scan.hops > min_costs[current_scan.cell])
			continue;
		// we compute the sum of the current distance plus one hop to the receiver
		partial_scan.hops = current_scan.hops + 1;
		// we cycle through the neighbours of the current cell
		for(i = 0; i < directions; ++i){
			// we get the receiver cell
			receiver = get_raw_receiver(current_scan.cell, i);
			// obviously we want a valid neighbour
			if(receiver == DIRECTION_INVALID || bitmap_check(obstacles, receiver))
				continue;
			// if lower we have a candidate optimum for the cell
			if(partial_scan.hops < min_costs[receiver]){
				// set the previous cell to retrieve the path later on
				previous[receiver] = current_scan.cell;
				// we set the cell field on our struct
				partial_scan.cell = receiver;
				// refresh lowest cost found for the cell
				min_costs[receiver] = partial_scan.hops;
				// we insert this into the heap
				heap_insert(heap, partial_scan, __cmp_dijkstra_h);
			}
		}
	}
}
#undef __cmp_dijsktra_h

static void refresh_cache_obstacles(topology_t *topology){
	if(topology->dirty){
		const unsigned lp_cnt = topology_global.lp_cnt;
		// calculate the minimum costs spanning tree
		dijkstra_obstacles(topology, current->gid.to_int, topology->prev_next_cache);
		// this sets to an uninitialized value the buffer which holds the next hop for
		// each possible destination (we compute those on demand when asked by the user and we cache those here)
		memset(&topology->prev_next_cache[lp_cnt], UCHAR_MAX, sizeof(unsigned) * lp_cnt);
		topology->dirty = false;
	}
}

unsigned int find_receiver_obstacles(void) {
	const topology_t *topology = current->topology;
	const rootsim_bitmap *obstacles = topology->data;
	const unsigned sender = current->gid.to_int;
	if(unlikely(bitmap_check(obstacles, sender))){
		rootsim_error(false, "FindReceiver(): this is an obstacle region!!!\n");
		return DIRECTION_INVALID;
	}

	if(unlikely(!topology->free_neighbours))
		return sender;

	const unsigned directions = topology_global.directions;
	const unsigned *neighbours;
	unsigned receiver;

	switch(topology_global.geometry){
		case TOPOLOGY_HEXAGON:
		case TOPOLOGY_SQUARE:
		case TOPOLOGY_TORUS:
		case TOPOLOGY_BIDRING:
		case TOPOLOGY_RING:
		neighbours = topology->neighbours_id;
		do{
			receiver = neighbours[(unsigned)(directions * Random())];
		}while(unlikely(receiver == DIRECTION_INVALID || bitmap_check(obstacles, receiver)));
		break;

		case TOPOLOGY_GRAPH:
		do{
			receiver = (directions + 1) * Random();
		}while(unlikely(bitmap_check(obstacles, receiver)));
		break;

		case TOPOLOGY_STAR:
			if(sender != 0) {
				receiver = 0;
			} else {
				receiver = ((topology_global.lp_cnt - 1) * Random()) + 1;
			}
	}
	return receiver;
}

static inline void toggle_bit_and_state(topology_t *topology, rootsim_bitmap *bitmap, unsigned from){

	unsigned refresh_free = 0;
	if(topology_global.geometry != TOPOLOGY_GRAPH){
		unsigned i = topology_global.directions;
		while(i--)
			if(topology->neighbours_id[i] != DIRECTION_INVALID){
				refresh_free = 1;
				break;
			}
	}else{
		refresh_free = (from != current->gid.to_int);
	}


	if(bitmap_check(bitmap, from)){
		bitmap_reset(bitmap, from);
		topology->free_neighbours += refresh_free;
	}else{
		bitmap_set(bitmap, from);
		topology->free_neighbours -= refresh_free;
	}
	topology->dirty = true;
}

void set_value_topology_obstacles(unsigned from, unsigned to, double value){
	(void)to;

	topology_t *topology = current->topology;
	const unsigned this_lp = current->gid.to_int;
	rootsim_bitmap *bitmap = topology->data;

	if(!((value > 0) ^ bitmap_check(bitmap, from)))
		return;

	toggle_bit_and_state(topology, bitmap, from);

	unsigned i = topology_global.lp_cnt;
	while(i--){
		if(i == this_lp)
			continue;
		UncheckedScheduleNewEvent(i, current_evt->timestamp, TOPOLOGY_UPDATE, &from, sizeof(from));
	}
}

void update_topology_obstacles(void){
	topology_t *topology = current->topology;
	toggle_bit_and_state(topology, topology->data, *((unsigned *)current_evt->event_content));
}

double get_value_topology_obstacles(unsigned from, unsigned to){
	(void)to;
	return bitmap_check(current->topology->data, from) ? 1.0 : 0.0;
}

bool is_reachable_obstacles(unsigned to){
	// XXX do we want deep reachability or dumb reachability?
	return find_receiver_toward_obstacles(to) != UINT_MAX;
}

unsigned int find_receiver_toward_obstacles(unsigned int to){
	topology_t *topology = current->topology;
	const unsigned lp_cnt = topology_global.lp_cnt;
	const unsigned this_lp = current->gid.to_int;

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
	topology_t *topology = current->topology;
	const unsigned lp_cnt = topology_global.lp_cnt;
	unsigned int previous[lp_cnt], hops;

	if(source == current->gid.to_int){
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
