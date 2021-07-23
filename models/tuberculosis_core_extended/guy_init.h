/*
 * guy_init.h
 *
 *  Created on: 04 ott 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_GUY_INIT_H_
#define MODELS_TUBERCOLOSIS_GUY_INIT_H_

#include "application.h"
#include "guy.h"

typedef struct _init_t init_t;

struct _init_t {
	unsigned agents_count[END_STATES];
};

void guy_init(double randomNumber);
void guy_on_init(init_t *init_data, region_t *region);

void init_infected(struct guy_t *guy, region_t *region);
void init_sick(struct guy_t *guy, region_t *region);
void init_treatment(struct guy_t *guy, region_t *region);
void init_treated(struct guy_t *guy, region_t *region);
int RandomRangeCustom(region_t *region, int min, int max);

#endif /* MODELS_TUBERCOLOSIS_GUY_INIT_H_ */
