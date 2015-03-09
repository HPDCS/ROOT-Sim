/**
*
* TRAFFIC is a simulation model for the ROme OpTimistic Simulator (ROOT-Sim)
* which allows to simulate car traffic on generic routes, which can be
* specified from text file.
*
* The software is provided as-is, with no guarantees, and is released under
* the GNU GPL v3 (or higher).
*
* For any information, you can find contact information on my personal webpage:
* http://www.dis.uniroma1.it/~pellegrini
*
* @file application.h
* @brief Global simulation data types and definitions
* @author Alessandro Pellegrini
* @date January 12, 2012
*/

#pragma once
#ifndef _TRAFFIC_APPLICATION_H
#define _TRAFFIC_APPLICATION_H


#include <ROOT-Sim.h>


#define YEAR 	31536000
#define WEEK	604800
#define DAY	86400


// Execution time must be specified in seconds
#ifndef EXECUTION_TIME
	#define EXECUTION_TIME	(1 * WEEK)
#endif


// In Km/h. Il simulatore li converte in Km/s
#ifndef AVERAGE_SPEED
	#define AVERAGE_SPEED	110
#endif

// Lunghezza macchina: 4.20m + 0.80m distanza di sicurezza = 5m
// Unità di lunghezza in km
// Due corsie
#ifndef CARS_PER_UNIT_LENGTH
	#define CARS_PER_UNIT_LENGTH	1500//400
#endif

// A junction has no actual length, yet cars can be queued in it
#ifndef CARS_PER_JUNCTION
	#define	CARS_PER_JUNCTION	5000//1000
#endif

// A junction has no actual length, yet cars take some time to pass in it
#ifndef JUNCTION_TRAVERSE_TIME
	#define	JUNCTION_TRAVERSE_TIME  600	// 10 minutes on average
#endif

#define MIN_SPEED		15//20

// SIGMA is in Km/h
#define SPEED_SIGMA		20.0
#define ENTER_SIGMA		10

// accidents parameters
#define ACCIDENT_DURATION	3600	// one hour on average
#define ACCIDENT_SIGMA		30


// EVENTI
#define ARRIVAL		10
#define LEFT		11
#define KEEP_ALIVE	100


// Traffic Factors
//#define CAR_LAMBDA	1.0	// Defines frequency for a Poisson process. Generation of new cars at junctions
//#define CAR_ENTER_NUM	10	// How many cars (on average) join the route from a junction
//#define CAR_LEAVE_NUM	10	// How many cars (on average) leave the route from a junction

//#define HIGH_LOAD_LEAVE		0.1
//#define NORMAL_LOAD_LEAVE	1

//#define HIGH_LOAD_ENTER		1.5
//#define NORMAL_LOAD_ENTER	1


#define ACCIDENT_PROBABILITY	0.015//0.01


// LP Type
#define JUNCTION	123
#define SEGMENT		124


// Allowed length for an LP's name
#define NAME_LENGTH	32

// <------ Togliere per il rilascio!
#include <float.h>
#define WORD_LENGTH (8 * sizeof(unsigned long))
#define ROR(value, places) ((value << (places)) | (value >> (WORD_LENGTH - (places))));
#define D_EQUAL(a,b) (fabs((a) - (b)) < DBL_EPSILON)
#define D_EQUAL_ZERO(a) (fabs(a) < DBL_EPSILON)
#define D_DIFFER(a,b) (fabs((a) - (b)) >= DBL_EPSILON)
#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)
#define F_EQUAL(a,b) (fabs((a) - (b)) < FLT_EPSILON)
#define F_EQUAL_ZERO(a) (fabs(a) < FLT_EPSILON)
#define F_DIFFER(a,b) (fabs((a) - (b)) >= FLT_EPSILON)
#define F_DIFFER_ZERO(a) (fabs(a) >= FLT_EPSILON)
extern double Gaussian(double m, double s);
// ------>


typedef struct _event_content_type {
	int	from;
	int	injection;
} event_content_type;




typedef struct _topology {
	int num_neighbours;
	unsigned int *neighbours;
} topology_t;



typedef struct _car {
	int		from;
	simtime_t	arrival;
	simtime_t	leave;
	struct _car 	*next;
} car_t;



typedef struct _lp_state_type {
	simtime_t	lvt;			// Elapsed simulation time
	int		accident;
	simtime_t	accident_end;
	unsigned int	total_queue_slots;
	char		name[NAME_LENGTH];	// Name of the cell
	int		lp_type;		// Is it a junction or a segment?
	double		segment_length;		// Length of this road segment. If set to 0, it's a junction
	double		enter_prob;		// in realtà è una frequency!
	double		leave_prob;
	topology_t	*topology;		// Each node can have an arbitrary number of neighbours
	unsigned int	queue_slots;
	car_t		*queue;			// Cars passing through the node are stored here
} lp_state_type;






extern int enqueue_car(lp_state_type *state, simtime_t lvt, int from);
extern void inject_new_cars(lp_state_type *state, int me);
extern int check_car_leaving(lp_state_type *state, int from, int me);
extern void forward_car(lp_state_type *state, int from, int me);
extern void check_accident_end(lp_state_type *state);
extern void cause_accident(lp_state_type *state, int me);



#endif /* _TRAFFIC_APPLICATION_H */

