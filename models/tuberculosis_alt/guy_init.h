/*
 * guy_init.h
 *
 *  Created on: 04 ott 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_GUY_INIT_H_
#define MODELS_TUBERCOLOSIS_GUY_INIT_H_

#include "application.h"

typedef struct _init_t init_t;

void guy_init(void);
void guy_on_init(init_t *init_data, region_t *region);

#endif /* MODELS_TUBERCOLOSIS_GUY_INIT_H_ */
