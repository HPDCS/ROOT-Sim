#ifndef _MPI_H_
#define _MPI_H_

#ifdef HAS_MPI


#include <stdbool.h>
#include <mpi.h>

extern bool mpi_support_multithread;
extern MPI_Datatype msg_mpi_t;

void mpi_init(int *argc, char ***argv);
void mpi_finalize(void);


#endif /* HAS_MPI */
#endif /* _MPI_H_ */
