/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file models.c
* @brief This module keeps all the models that are re-evaluated during the GVT
*        reduction phase. In particular, a sigle entry point for this module
*        is called by the GVT reduction system. It then calls, in the proper
*        order, all the modules to self-tune the execution of the platform
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date November 11, 2015
*/

#include <core/core.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>

#ifdef HAVE_REVERSE
static int recompute_reverse_events(unsigned int lid) {

	double value;
	double chi = rootsim_config.ckpt_period;	

	// Collecting data
	double Fr = statistics_get_data(STAT_GET_ROLLBACK_FREQ, lid);
	double de = statistics_get_data(STAT_GET_EVENT_TIME_LP, lid);
	double db = statistics_get_data(STAT_GET_UNDO_EVENT_COST, lid);
	double dl = statistics_get_data(STAT_GET_FULL_CKPT_TIME, lid);
	double dr = statistics_get_data(STAT_GET_FULL_RECOVERY_TIME, lid);
	
	// Model
	double a2 = Fr * (dr - de) / chi;
	if(a2 == 0 || isnan(a2)) {
		value = chi;
		goto out;
	}
	
	double b = (Fr * de + 2*db) / 2*chi;
	double c = db/chi - dr - dl;
	double delta = sqrt(b*b - 2*a2*c);

	value = (-b + delta) / a2;
	value = chi - 1 - (-(Fr*de / 2*chi) - (db/chi) + sqrt((2*db*(2*db + 1) + Fr*de*(Fr*de + 4) / 4*chi) - 4*(Fr*db - de)/2*chi)*(db/chi - dr - dl)) / (Fr * (db - de) / chi);
			
    out:
#ifndef NDEBUG
	//printf("LP %d: Fr = %f de = %f db = %f dl = %f dr = %f chi = %f val = %f\n", lid, Fr, de, db, dl, dr, chi, value);
#endif
	return (int)value;
}
#endif


// This is called by all threads, during the adopt_new_gvt() phase.
// Keep this in mind when implementing the models, as they are
// executed in parallel!
void gvt_recompute_models(void) {
	unsigned int i;

#ifdef HAVE_REVERSE
	if(!rootsim_config.disable_reverse) {
		for(i = 0; i < n_prc_per_thread; i++) {
			LPS_bound[i]->events_in_coasting_forward = recompute_reverse_events(i);
		}
	}
#endif
}

