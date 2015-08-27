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
* @file init.h
* @brief Definition for supporting text-file parsing
* @author Alessandro Pellegrini
* @date January 12, 2012
*/


#pragma once
#ifndef _TRAFFIC_INIT_H
#define _TRAFFIC_INIT_H

// Input definitions
#define FILENAME	"topology.txt"
#define LINE_LENGTH	128

// Constant Strings
#define INTERSECT_STR	"[INTERSECTIONS]"
#define ROUTES_STR	"[ROUTES]"

// Parsing states
#define NORMAL_S	111
#define INTERSECT_S	222
#define ROUTES_S	333

// In a segment line, which one is the length one? (first element is 1!)
#define LENGTH_ARG	4

extern void init_my_state(int me, lp_state_type *state);


#endif /* _TRAFFIC_INIT_H */

