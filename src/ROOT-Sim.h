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

/// This macro can be used to convert command line parameters to integers
#define parseInt(s) ({\
			int __value;\
			char *__endptr;\
			__value = (int)strtol(s, &__endptr, 10);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		     })

/// This macro can be used to convert command line parameters to doubles
#define parseDouble(s) ({\
			double __value;\
			char *__endptr;\
			__value = strtod(s, &__endptr);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		       })

/// This macro can be used to convert command line parameters to floats
#define parseFloat(s) ({\
			float __value;\
			char *__endptr;\
			__value = strtof(s, &__endptr);\
			if(!(*s != '\0' && *__endptr == '\0')) {\
				fprintf(stderr, "%s:%d: Invalid conversion value: %s\n", __FILE__, __LINE__, s);\
			}\
			__value;\
		       })

/// This macro can be used to convert command line parameters to booleans
#define parseBoolean(s) ({\
			bool __value;\
			if(strcmp((s), "true") == 0) {\
				__value = true;\
			} else {\
				__value = false;\
			}\
			__value;\
		       })

/// This defines the type with whom timestamps are represented
typedef double simtime_t;

/// Infinite timestamp: this is the highest timestamp in a simulation run
#define INFTY DBL_MAX

/// This is the definition of the number of LPs running in the current simulation
extern unsigned int n_prc_tot;

// Topology library

typedef enum _topology_t {
	TOPOLOGY_HEXAGON = 	1000,
	TOPOLOGY_SQUARE = 	1001,
	TOPOLOGY_MESH = 	1002,
	TOPOLOGY_STAR = 	1003,
	TOPOLOGY_RING = 	1004,
	TOPOLOGY_BIDRING = 	1005,
	TOPOLOGY_TORUS = 	1006,
	TOPOLOGY_INVALID = 	UINT_MAX
} topology_t;

typedef enum _direction_t {
	DIRECTION_N = 	0,
	DIRECTION_S = 	1,
	DIRECTION_E = 	2,
	DIRECTION_W = 	3,
	DIRECTION_NW = 	4,
	DIRECTION_SW = 	5,
	DIRECTION_SE = 	6,
	DIRECTION_NE = 	7,
	DIRECTION_INVALID = UINT_MAX
} direction_t;

/// This is a type used to setup obstacles in a grid of cells
typedef struct obstacles_t obstacles_t;

unsigned int FindReceiver(topology_t topology);
// Returns INVALID_DIRECTION if a movement is not possible according to the given topology
unsigned int GetReceiver(topology_t topology, unsigned int from, direction_t direction);

// Setup and discard an obstacle grid. num is the number of integers passed to the variadic function
obstacles_t* NewObstacles(void);
int AddObstacles(obstacles_t *obstacles, unsigned int num, ...);
int AddObstacle(obstacles_t *obstacles, unsigned int cell);
void FreeObstacles(obstacles_t *obstacles);
bool IsObstacle(obstacles_t *obstacles, unsigned int cell);

// Function to return a list of LP IDs to be visited in order to reach a given cell.
// @param[out] list Pointer to a memory region with at least sizeof(unsigned)*n_prc_tot bytes available which will hold the result
unsigned int ComputeMinTour(unsigned int result[n_prc_tot], obstacles_t *obstacles, topology_t topology, unsigned int source,
		unsigned int dest);

// This can be implemented by the model: it is a pointer to a struct argp
__attribute((weak)) extern struct argp model_argp;
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

// Generate a system-wide unique identifier
unsigned long long GenerateUniqueId(void);

// ROOT-Sim core API
extern void (*ScheduleNewEvent)(unsigned int receiver, simtime_t timestamp, unsigned int event_type,
		void *event_content, unsigned int event_size);
extern void (*SetState)(void *new_state);

#endif /* __ROOT_Sim_H */

