/**
 * @file ROOT-Sim.h
 *
 * @brief ROOT-Sim header for model development.
 *
 * This header defines all the symbols which are needed to develop a model
 * to be simulated on top of ROOT-Sim.
 *
 * This header is the only file which should be included when developing
 * a simulation model. All function prototypes exposed to the application
 * developer are exposed and defined here.
 *
 * @copyright
 * Copyright (C) 2008-2019 HPDCS Group
 * https://hpdcs.github.io
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
 * @author Francesco Quaglia
 * @author Alessandro Pellegrini
 * @author Roberto Vitali
 *
 * @date 3/16/2011
 */

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <float.h>
#include <limits.h>
#include <argp.h>

#ifdef INIT
#undef INIT
#endif
/// This is the message code which is sent by the simulation kernel upon startup
#define INIT	0

/// This defines the type with whom timestamps are represented
typedef double simtime_t;

/// Infinite timestamp: this is the highest timestamp in a simulation run
#define INFTY DBL_MAX

/// This is the definition of the number of LPs running in the current simulation
extern unsigned int n_prc_tot;

/// This can be implemented by the model for smart argument handling
__attribute((weak))
extern struct argp model_argp;

// Expose to the application level the rollbackable numerical library
double Random(void);
int RandomRange(int min, int max);
int RandomRangeNonUniform(int x, int min, int max);
double Expent(double mean);
double Normal(void);
double Gamma(int ia);
double Poisson(void);
int Zipf(double skew, int limit);

// ROOT-Sim core API
extern void (*ScheduleNewEvent)(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);
extern void SetState(void *new_state);

/*********************************/
/********TOPOLOGY*LIBRARY*********/
/*********************************/

/**
 * This are the supported topology geometries
 */
enum _topology_geometry_t {
	TOPOLOGY_GEOMETRY_OFFSET = 1000,            //!< arbitrary offset used to distinguish during debug different enums
	TOPOLOGY_HEXAGON = TOPOLOGY_GEOMETRY_OFFSET,//!< hexagonal grid topology
	TOPOLOGY_SQUARE,                            //!< square grid topology
	TOPOLOGY_RING,                              //!< a ring shaped topology walkable in a single direction
	TOPOLOGY_BIDRING,                           //!< a ring shaped topology direction
	TOPOLOGY_TORUS,                             //!< a torus shaped grid topology (a wrapping around square topology)
	TOPOLOGY_STAR, // this still needs to be properly implemented FIXME
	TOPOLOGY_GRAPH,                             //!< an arbitrary shaped topology
};

/**
 * These are the supported basic directions:
 * TOPOLOGY_HEXAGON recognizes E, W, NE, SW, NW, SE
 * TOPOLOGY_SQUARE and TOPOLOGY_TORUS recognize E, W, N, S
 * TOPOLOGY_RING recognizes E
 * TOPOLOGY_BIDRING recognizes E, W
 * TOPOLOGY_STAR xxx to implement
 * TOPOLOGY_GRAPH directions are actually directly mapped to the LPs IDs
 * The enum layout is intentional:
 * do not modify it if you don't know what you're doing!
 */
typedef enum _direction_t {
	DIRECTION_E = 	0,	//!< DIRECTION_E
	DIRECTION_W = 	1,	//!< DIRECTION_W

	DIRECTION_N = 	2,	//!< DIRECTION_N
	DIRECTION_S = 	3,	//!< DIRECTION_S

	DIRECTION_NE = 	2,	//!< DIRECTION_NE
	DIRECTION_SW =  3,	//!< DIRECTION_SW
	DIRECTION_NW = 	4,	//!< DIRECTION_NW
	DIRECTION_SE = 	5,	//!< DIRECTION_SE

	DIRECTION_INVALID = UINT_MAX	//!< A generic invalid direction
} direction_t;

enum _topology_type_t{
	TOPOLOGY_OBSTACLES,	//!< all crossing costs and probabilities are set to 1, but there can be not crossable regions
	TOPOLOGY_COSTS, 	//!< decisions on next hops are taken based on the costs undertaken to cross the boundaries
	TOPOLOGY_PROBABILITIES 	//!< decisions are taken at random but are weighted on the palatability of neighbours
};

/**
 * Defining this struct in the model source code activates the topology subsystem.
 */
__attribute((weak)) extern struct _topology_settings_t{
	const char * const topology_path;
	const enum _topology_type_t type;			/// The topology type the model wants to use
	const enum _topology_geometry_t default_geometry;	/// The default geometry to use when nothing else is specified
	const unsigned out_of_topology;				/// The minimum number of LPs needed out of the topology
	const bool write_enabled;					/// This is set if the model needs to use SetTopology()
}topology_settings;

/**
 * TODO Documentation!
 * @param from
 * @param to
 * @return
 */
double 		GetValueTopology(unsigned from, unsigned to);

/**
 * Change the weight assigned to the link between region from to region to.
 * A link is intended as a crossable edge on the topology graph.
 * A weight has different meanings depending on the _topology_type_t of the topology.
 * For TOPOLOGY_COSTS it's the cost to pay to cross the link
 * For TOPOLOGY_PROBABILITIES it's the weight of the probability to choose that link during movement.
 * In other words given a region n0 having self weight w0 having links to regions n1 n2 n3 n4... with weights w1 w2 w3 w4
 * the probability of choosing region nk as next hop is wk/(w0+w1+w2+w3...)
 * @param from the LP id of the source region of the link
 * @param to the LP id of the sink region of the link
 * @param value the new weight to be assigned to the link
 * @return 0 on success, -1 otherwise.
 * Failure can happen if non existent links are specified or if value is an illegal argument
 * (For TOPOLOGY_PROBABILITIES can't be negative for example)
 */
void 		SetValueTopology(unsigned from, unsigned to, double value);

// finds a receiver with probabilities weighted on neighbours link (works only for topology type TOPOLOGY_PROBABILITIES)
unsigned int 	FindReceiver	(void);

// returns the count of regions involved in the topology (can be less than n_prc_tot)
unsigned int	RegionsCount	(void);

// returns the maximum count of neighbours this region has, this is made to simplify direction handling (need to explain better xxx)
unsigned int	DirectionsCount	(void);

// returns the actual count of neighbours this region has (if this is called from a region on the edge of the topology
unsigned int 	NeighboursCount	(unsigned int region);

// Returns DIRECTION_INVALID if a movement is not possible according to the given topology
unsigned int 	GetReceiver	(unsigned int from, direction_t direction, bool reachable);

// this returns the next hop to reach region to following a minimum cost path: it's far less costly than to recompute a new path with ComputeMinTour()
unsigned int FindReceiverToward	(unsigned int to);

// Function to return a list of LP IDs to be visited in order to reach a given cell with minimum cost.
// -1.0 is returned is no path is available else the total cost is returned
double 		ComputeMinTour	(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]);

/*********************************/
/************ABM*LIBRARY**********/
/*********************************/

typedef unsigned long long agent_t;

__attribute((weak)) extern struct _abm_settings_t{
	const unsigned neighbour_data_size;
	const unsigned traverse_handler;
	const bool keep_history;
} abm_settings;

int			GetNeighbourInfo	(direction_t i, unsigned int *region_id, void **data_p);
void			TrackNeighbourInfo	(void *neighbour_data);

bool 			IterAgents		(agent_t *agent_p);
unsigned		CountAgents		(void);

agent_t 		SpawnAgent		(unsigned user_data_size);
void			KillAgent		(agent_t agent);

void*			DataAgent		(agent_t agent, unsigned *data_size_p);

void			ScheduleNewLeaveEvent	(simtime_t time, unsigned int event_type, agent_t agent);

unsigned 		CountVisits		(const agent_t agent);
void 			GetVisit		(const agent_t agent, unsigned *region_p, unsigned *event_type_p, unsigned i);
void 			SetVisit		(const agent_t agent, unsigned region, unsigned event_type, unsigned i);
void 			EnqueueVisit		(agent_t agent, unsigned region, unsigned event_type);
void 			AddVisit		(agent_t agent, unsigned region, unsigned event_type, unsigned i);
void 			RemoveVisit		(agent_t agent, unsigned i);

unsigned 		CountPastVisits		(const agent_t agent);
void 			GetPastVisit		(const agent_t agent, unsigned *region_p, unsigned *event_type_p, simtime_t *time_p, unsigned i);
