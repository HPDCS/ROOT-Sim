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
* @file ROOT-Sim.h
* @brief This header defines all the symbols which are needed to develop a Model
*        to be simulated on top of ROOT-Sim.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/16/2011
*/



#pragma once
#ifndef __ROOT_Sim_H
#define __ROOT_Sim_H

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
__attribute((weak)) extern struct argp model_argp;

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
extern void (*SetState)(void *new_state);

/*********************************/
/********TOPOLOGY*LIBRARY*********/
/*********************************/

enum _topology_geometry_t {
	TOPOLOGY_GEOMETRY_OFFSET = 1000,
	TOPOLOGY_HEXAGON = TOPOLOGY_GEOMETRY_OFFSET,
	TOPOLOGY_SQUARE,
	TOPOLOGY_GRAPH,
	TOPOLOGY_STAR, // this still needs to be properly implemented FIXME
	TOPOLOGY_RING,
	TOPOLOGY_BIDRING,
	TOPOLOGY_TORUS,
};

typedef enum _direction_t {
	DIRECTION_E = 	0,
	DIRECTION_W = 	1,

	DIRECTION_N = 	2,
	DIRECTION_S = 	3,

	DIRECTION_NE = 	2,
	DIRECTION_SW =  3,
	DIRECTION_NW = 	4,
	DIRECTION_SE = 	5,

	DIRECTION_INVALID = UINT_MAX
} direction_t;

enum _topology_type_t{
	TOPOLOGY_COSTS, 	/// decisions on next hops are taken based on the costs undertaken to cross the boundaries
	TOPOLOGY_PROBABILITIES,	/// decisions are taken at random but are weighted on the palatability of neighbours
	TOPOLOGY_OBSTACLES	/// all crossing costs and probabilities are set to 1, but there can be not crossable regions
};

__attribute((weak)) extern struct _topology_settings_t{
	const char * const topology_path;
	const enum _topology_type_t type;
	const enum _topology_geometry_t default_geometry;
}topology_settings;

/**
 * Change the weight assigned to the link between region from to region to.
 * A link is intended as a crossable edge on the topology graph.
 * A weight has different meanings depending on the _topology_type_t of the topology.
 * For TOPOLOGY_COSTS it's the cost to pay to cross the link
 * For TOPOLOGY_PROBABILITIES it's the weight of the probability to choose that link during movement.
 * In other words given a region n0 having self weight w0 having links to regions n1 n2 n3 n4... with weights w1 w2 w3 w4
 * the probability of choosing region nk as next hop is wk/(w0+w1+w2+w3...)
 * @param topology the topology on which to act. If NULL the global topology is used.
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
unsigned int	NeighboursCount	(void);

// returns the actual count of neighbours this region has (if this is called from a region on the edge of the topology
// this count can be less than the one returned by NeighboursCount())
unsigned int ActualNeighboursCount(void);

// Returns DIRECTION_INVALID if a movement is not possible according to the given topology
unsigned int 	GetReceiver	(unsigned int from, direction_t direction);

// this returns the next hop to reach region to following a minimum cost path: it's far less costly than to recompute a new path with ComputeMinTour()
unsigned int FindReceiverToward	(unsigned int to);

// Function to return a list of LP IDs to be visited in order to reach a given cell with minimum cost.
// -1.0 is returned is no path is available else the total cost is returned
double 		ComputeMinTour	(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()]);

/*********************************/
/************ABM*LIBRARY**********/
/*********************************/

typedef struct _agent_abm_t agent_abm_t;

__attribute((weak)) extern struct _abm_settings_t{
	const unsigned neighbour_data_size;
	const unsigned traverse_handler;
	const bool keep_history;
} abm_settings;

// XXX redundant with NeighboursCount() ELIMINATE THIS
unsigned		CountNeighbourInfos	(void);
int			GetNeighbourInfo	(direction_t i, unsigned int *region_id, void **data_p);
void			TrackNeighbourInfo	(void *neighbour_data);

bool 			IterAgents		(agent_abm_t **agent_p);
unsigned		CountAgents		(void);

agent_abm_t* 		SpawnAgent		(unsigned user_data_size);
int			KillAgent		(agent_abm_t* agent);

void*			DataAgent		(agent_abm_t *agent, unsigned *data_size_p);

unsigned long long 	IdAgent			(const agent_abm_t *agent);
agent_abm_t* 		FindAgent		(unsigned long long agent_id);

void			ScheduleNewLeaveEvent	(simtime_t time, unsigned int event_type, agent_abm_t *agent);

unsigned 		CountVisits		(const agent_abm_t *agent);
int 			GetVisit		(const agent_abm_t *agent, unsigned *region_p, unsigned *event_type_p, unsigned i);
int 			SetVisit		(const agent_abm_t *agent, unsigned region, unsigned event_type, unsigned i);
int 			EnqueueVisit		(agent_abm_t *agent, unsigned region, unsigned event_type);
int 			AddVisit		(agent_abm_t *agent, unsigned region, unsigned event_type, unsigned i);
int 			RemoveVisit		(agent_abm_t *agent, unsigned i);

unsigned 		CountPastVisits		(const agent_abm_t *agent);
int 			GetPastVisit		(const agent_abm_t *agent, unsigned *region_p, unsigned *event_type_p, simtime_t *time_p, unsigned i);

#endif /* __ROOT_Sim_H */

