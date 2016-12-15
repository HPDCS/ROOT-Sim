#ifdef HAS_MPI


#include <mpi.h>

#include <communication/mpi.h>
#include <core/core.h>


// true if the underlying MPI implementation support multithreading
bool mpi_support_multithread;


/*
 * Wrapper of MPI_Init call
 */
void mpi_init(int *argc, char ***argv){
	int mpi_thread_lvl_provided;
	MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &mpi_thread_lvl_provided);

	mpi_support_multithread = true;
	if(mpi_thread_lvl_provided < MPI_THREAD_MULTIPLE){
		//MPI do not support thread safe api call
		if(mpi_thread_lvl_provided < MPI_THREAD_SERIALIZED){
			// MPI do not even support serialized threaded call we cannot continue
			rootsim_error(true, "The MPI implementation do not support threads "
			                    "[current thread level support: %d]\n", mpi_thread_lvl_provided);
		}
		mpi_support_multithread = false;
	}
}


void mpi_finalize(void){
	if(master_thread())
		MPI_Finalize();
}


#endif /* HAS_MPI */
