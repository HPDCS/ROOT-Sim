#ifndef _MPI_H_
#define _MPI_H_

#ifdef HAS_MPI


#include <stdbool.h>

extern bool mpi_support_multithread;

void mpi_init(int *argc, char ***argv);
void mpi_finalize(void);


#endif /* HAS_MPI */
#endif /* _MPI_H_ */
