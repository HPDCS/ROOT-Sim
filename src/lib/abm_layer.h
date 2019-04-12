/*
 * abm_layer.h
 *
 *  Created on: 22 giu 2018
 *      Author: andrea
 */

#ifndef ABM_LAYER_H_
#define ABM_LAYER_H_

typedef struct _region_abm_t region_abm_t;

void 	abm_layer_init	(void);
void 	ProcessEventABM	(void);
unsigned char * abm_do_checkpoint(region_abm_t *region);
void abm_restore_checkpoint(unsigned char *data, region_abm_t *old_region);

#endif /* ABM_LAYER_H_ */
