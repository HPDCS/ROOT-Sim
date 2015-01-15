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
* @file gvt.c
* @brief This module switches at compile time between ourimplemetation of the GVT
* 	 protocol and Fujimoto's. It's here just for testing convenience, and
* 	 will be removed soon from the simulator.
* @author Alessandro Pellegrini
*/


#include <gvt/gvt.h>


#define GVT_NOSTRO 1
#define GVT_FUJIMOTO 2


//#define EXEC_GVT GVT_FUJIMOTO
#define EXEC_GVT GVT_NOSTRO





#include <signal.h>
#include <time.h>
unsigned long long gvt_execution_time = 0;
int gvt_timer_id;
timer_t gvt_timerid;

extern void gvt_init_nostro(void);
extern void gvt_init_fujimoto(void);

/**
* Initializer of the GVT subsystem
*
* @author Alessandro Pellegrini
*
*/
void gvt_init(void) {
	#if EXEC_GVT == GVT_FUJIMOTO
	gvt_init_fujimoto();
	#elif EXEC_GVT == GVT_NOSTRO
	gvt_init_nostro();
	#else
	#error Wrong EXEC_GVT value
	#endif

/*	struct sigevent sev;
	sev.
	gvt_timer_id = timer_create(CLOCK_THREAD_CPUTIME_ID, , &gvt_timerid);
*/
}


extern void gvt_fini_nostro(void);
extern void gvt_fini_fujimoto(void);

/**
* Finalizer of the GVT subsystem
*
* @author Alessandro Pellegrini
*/
void gvt_fini(void){
	#if EXEC_GVT == GVT_FUJIMOTO
	gvt_fini_fujimoto();
	#elif EXEC_GVT == GVT_NOSTRO
	#else
	#error Wrong EXEC_GVT value
	gvt_fini_nostro();
	#endif
}




extern simtime_t gvt_operations_nostro(void);
extern simtime_t gvt_operations_fujimoto(void);


/**
* Fujimoto's algorithm for GVT reduction on shared memory machines
*
* @author Richar M. Fujimoto
* @author Alessandro Pellegrini
* 
* @return The newly computed GVT value, or -1.0. Only a Master Thread should return a value
* 	  different from -1.0, to avoid generating too much information. If every thread
* 	  will return a value different from -1.0, nothing will be broken, but all the values
* 	  will be shown associated with the same kernel id (no way to distinguish between
* 	  different threads here).
*/
simtime_t gvt_operations(void) {
	#if EXEC_GVT == GVT_FUJIMOTO
	return gvt_operations_fujimoto();
	#elif EXEC_GVT == GVT_NOSTRO
	return gvt_operations_nostro();
	#else
	#error Wrong EXEC_GVT value
	#endif
}


