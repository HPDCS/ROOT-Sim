#ifndef _MPI_H_
#define _MPI_H_

#ifdef HAS_MPI


#include <stdbool.h>
#include <mpi.h>

#include <core/core.h>
#include <communication/wnd.h>


extern bool mpi_support_multithread;

#define lock_mpi() {if(!mpi_support_multithread) spin_lock(&mpi_lock);}
#define unlock_mpi() {if(!mpi_support_multithread) spin_unlock(&mpi_lock);}

// control access to MPI interface
// used only in the case MPI do not support multithread
extern spinlock_t mpi_lock;

extern MPI_Datatype msg_mpi_t;

void mpi_init(int *argc, char ***argv);
void inter_kernel_comm_init(void);
void inter_kernel_comm_finalize(void);
void mpi_finalize(void);
void syncronize_all(void);
void send_remote_msg(const msg_t* msg);
bool pending_msgs(int tag);
void receive_remote_msgs(void);
bool is_request_completed(MPI_Request* req);
bool all_kernels_terminated(void);
void broadcast_termination(void);
void collect_termination(void);

#endif /* HAS_MPI */
#endif /* _MPI_H_ */
