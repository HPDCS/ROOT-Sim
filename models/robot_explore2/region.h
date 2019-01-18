#ifndef _REGION_H
#define _REGION_H

#include <stdbool.h>

#include "application.h"

#define OBSTACLE_PROB	0.1
#define REGION_KEEP_ALIVE_INTERVAL 50

/**
 * Given a region's id and a direction, it computes the id of the target region
 * towards which the robot is pointing.
 *
 * @param region_id Current region's id
 * @param direction Integer representing the where robot is heading to
 * @return The id of the region the robot is currently targeting
 */
unsigned int get_target_id(unsigned int region_id, unsigned int direction);

/**
 * Tells whether the passed region <em>neighbour</em> can be reached
 * from the region <em>region</em>.
 *
 * @param region Region's id of the "pivot" point
 * @param neighbour Region's id of the neighbour we want to compute reachability
 * @return True if the <em>neighbour</em> can be reached from <em>region</em>, false otherwise
 */
bool is_reachable(cell_state_type * region, unsigned int direction);

unsigned int map_hexagon_to_linear(unsigned int x, unsigned int y);

void map_linear_to_hexagon(unsigned int linear, unsigned int *x, unsigned int *y);

#endif				// _REGION_H
