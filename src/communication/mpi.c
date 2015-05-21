/**
*                       Copyright (C) 2008-2014 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file mpi.h
* @brief This module wraps all MPI primitives to allow for easier usage on
*	 multicore systems
* @author Alessandro Pellegrini
* @date 21/05/2015
*/
#include <mpi.h>
#include <stdio.h>

void mpi_init(void) {
	int provided, claimed;
	char s = 'M', r = '?';
	MPI_Status Stat;

	// MPI_THREAD_FUNNELED
	MPI_Init_thread(0, 0, MPI_THREAD_SINGLE, &provided);

	MPI_Query_thread( &claimed );
        printf( "Query thread level= %d  Init_thread level= %d\n", claimed, provided );

	MPI_Send(&s, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
	MPI_Recv(&r, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &Stat);

	printf("sent = %c, recv = %c\n", s, r);
 
}

void mpi_fini(void) {
	MPI_Finalize();
}

