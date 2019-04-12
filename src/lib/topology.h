/*
 * topology.h
 *
 *  Created on: 02 lug 2018
 *      Author: andrea
 */

#ifndef __TOPOLOGY_H_
#define __TOPOLOGY_H_

#include <lib/jsmn_helper.h>
#include <datatypes/bitmap.h>

typedef struct _topology_t topology_t;

/// this is used to store the common characteristics of the topology
extern struct _topology_global_t{
	unsigned chkp_size;			/**< the size of the topology struct of each region (used for efficient check-pointing) */
	unsigned directions;			/**< the number of valid directions in the topology */
	unsigned edge; 				/**< the pre-computed edge length (if it makes sense for the current topology geometry) */
	unsigned lp_cnt; 			/**< the number of LPs involved in the topology */
	enum _topology_geometry_t geometry;	/**< the topology geometry (see ROOT-Sim.h) */
} topology_global;

// this initializes the topology environment
void topology_init(void);

//used internally (also in abm_layer module) to schedule our reserved events TODO: move in a more system-like module
void UncheckedScheduleNewEvent(unsigned int gid_receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

// if the model is using a topology this gets called instead of the plain ProcessEvent
void ProcessEventTopology(void);

// STUFF FOR INTERNAL USE (in between topology sources)
unsigned 	size_checkpoint_probabilities	(void);
unsigned 	size_checkpoint_costs		(void);
unsigned 	size_checkpoint_obstacles	(void);

void *		load_topology_file_probabilities(c_jsmntok_t *root_token, const char *json_base);
void *		load_topology_file_costs	(c_jsmntok_t *root_token, const char *json_base);
void *		load_topology_file_obstacles	(c_jsmntok_t *root_token, const char *json_base);

topology_t *	topology_probabilities_init	(unsigned this_region_id, void *topology_data);
topology_t *	topology_costs_init		(unsigned this_region_id, void *topology_data);
topology_t *	topology_obstacles_init		(unsigned this_region_id, void *topology_data);

double 		get_value_topology_probabilities(unsigned from, unsigned to);
double 		get_value_topology_costs	(unsigned from, unsigned to);
double 		get_value_topology_obstacles	(unsigned from, unsigned to);

void 		set_value_topology_probabilities(unsigned from, unsigned to, double value);
void 		set_value_topology_costs	(unsigned from, unsigned to, double value);
void 		set_value_topology_obstacles	(unsigned from, unsigned to, double value);

bool		is_reachable_probabilities	(unsigned to);
bool		is_reachable_costs		(unsigned to);
bool		is_reachable_obstacles		(unsigned to);

void 		update_topology_probabilities	(void);
void 		update_topology_costs		(void);
void 		update_topology_obstacles	(void);

unsigned int	find_receiver_probabilities	(void);
unsigned int 	find_receiver_obstacles		(void);

double 		compute_min_tour_costs		(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]);
double 		compute_min_tour_obstacles	(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]);

unsigned int 	find_receiver_toward_costs	(unsigned int to);
unsigned int 	find_receiver_toward_obstacles	(unsigned int to);


unsigned int 	get_raw_receiver		(unsigned int from, direction_t direction);
// the dijkstra algorithm returns a spanning tree rooted at the source with information about the parent of
// each node: this method is needed to build the complete path of a node given such an array of previous hops
// it's here because it's needed by both COSTS and BINARY
unsigned build_path(unsigned lp_cnt, unsigned result[lp_cnt], const unsigned int previous[lp_cnt], unsigned source, unsigned dest);

#endif /* __TOPOLOGY_H_ */
