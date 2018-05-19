/*
 * application.h
 *
 *  Created on: 10 mag 2018
 *      Author: andrea
 */

#ifndef ABM_APPLICATION_H_
#define ABM_APPLICATION_H_

#include "agent.h"
#include <ROOT-Sim.h>

void ProcessEvent(unsigned int me, simtime_t now, int event_type, void *event_content, int event_size, region_t *state);
bool OnGVT(unsigned int me, region_t *snapshot);

#endif /* ABM_APPLICATION_H_ */
