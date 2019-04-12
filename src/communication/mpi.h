/**
* @file communication/mpi.h
*
* @brief MPI Support Module
*
* MPI Support Module
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @author Tommaso Tocci
*/

#pragma once

#ifdef HAVE_MPI

#include <stdbool.h>
#include <mpi.h>

#include <core/core.h>
#include <communication/wnd.h>
#include <statistics/statistics.h>

/// This macro takes a global lock if multithread support is not available from MPI
#define lock_mpi() {if(!mpi_support_multithread) spin_lock(&mpi_lock);}

/// This macro releases a global lock if multithreaded support is not available from MPI
#define unlock_mpi() {if(!mpi_support_multithread) spin_unlock(&mpi_lock);}

extern bool mpi_support_multithread;
extern spinlock_t mpi_lock;

void mpi_init(int *argc, char ***argv);
void inter_kernel_comm_init(void);
void inter_kernel_comm_finalize(void);
void mpi_finalize(void);
void syncronize_all(void);
void send_remote_msg(msg_t * msg);
bool pending_msgs(int tag);
void receive_remote_msgs(void);
bool is_request_completed(MPI_Request *);
bool all_kernels_terminated(void);
void broadcast_termination(void);
void collect_termination(void);
void mpi_reduce_statistics(struct stat_t *, struct stat_t *);

#endif /* HAVE_MPI */
