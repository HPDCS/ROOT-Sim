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
 * Copyright (C) 2008-2018 HPDCS Group
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

// Topology library
#define TOPOLOGY_HEXAGON	1000
#define TOPOLOGY_SQUARE		1001
#define TOPOLOGY_MESH		1002
#define TOPOLOGY_STAR		1003
#define TOPOLOGY_RING		1004
#define TOPOLOGY_BIDRING	1005
#define TOPOLOGY_TORUS		1006
unsigned int FindReceiver(int topology);

#define DIRECTION_N	0
#define DIRECTION_S	1
#define DIRECTION_E	2
#define DIRECTION_W	3
#define DIRECTION_NW	4
#define DIRECTION_SW	5
#define DIRECTION_SE	6
#define DIRECTION_NE	7

#define INVALID_DIRECTION UINT_MAX

/// This is a type used to setup obstacles in a grid of cells
typedef struct obstacles_t obstacles_t;

// Returns INVALID_DIRECTION if a movement is not possible according to the given topology
unsigned int GetReceiver(int topology, int direction);

// Setup and discard an obstacle grid. num is the number of integers passed to the variadic function
void SetupObstacles(obstacles_t ** obstacles);
void AddObstacles(obstacles_t * obstacles, int num, ...);
void AddObstacle(obstacles_t * obstacles, int cell);
void DiscardObstacles(obstacles_t * obstacles);

// Function to return a list of LP IDs to be visited in order to reach a given cell.
unsigned int ComputeMinTour(unsigned int **list, obstacles_t * obstacles,
			    int topology, unsigned int source,
			    unsigned int dest);

// Expose to the application level the command line parameter parsers
int GetParameterInt(void *args, char *name);
float GetParameterFloat(void *args, char *name);
double GetParameterDouble(void *args, char *name);
bool GetParameterBool(void *args, char *name);
char *GetParameterString(void *args, char *name);
bool IsParameterPresent(void *args, char *name);

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
extern void (*ScheduleNewEvent)(unsigned int receiver, simtime_t timestamp,
				unsigned int event_type, void *event_content,
				unsigned int event_size);
extern void SetState(void *new_state);
