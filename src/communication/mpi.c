#ifdef HAS_MPI


#include <communication/mpi.h>
#include <core/core.h>


// true if the underlying MPI implementation support multithreading
bool mpi_support_multithread;

MPI_Datatype msg_mpi_t;

void mpi_datatype_init(void){
	msg_t* msg = NULL;
	int base;
	unsigned int i;
	MPI_Datatype type[7] = {MPI_UNSIGNED,
                          MPI_UNSIGNED_CHAR,
                          MPI_INT,
                          MPI_DOUBLE,
                          MPI_UNSIGNED_LONG_LONG,
                          MPI_INT,
                          MPI_CHAR};


	int blocklen[7] = {2, 1, 2, 2, 2, 1, MAX_EVENT_SIZE};
	MPI_Aint disp[7];

	MPI_Get_address(msg, disp);
	MPI_Get_address(&msg->colour, disp+1);
	MPI_Get_address(&msg->type, disp+2);
	MPI_Get_address(&msg->timestamp, disp+3);
	MPI_Get_address(&msg->mark, disp+4);
	MPI_Get_address(&msg->size, disp+5);
	MPI_Get_address(&msg->event_content, disp+6);
	base = disp[0];
	for (i=0; i < sizeof(disp)/sizeof(disp[0]); i++){
		disp[i] = MPI_Aint_diff(disp[i], base);
	}
	MPI_Type_create_struct(7, blocklen, disp, type, &msg_mpi_t);
	MPI_Type_commit(&msg_mpi_t);
}


void mpi_datatype_finalize(void){
	MPI_Type_free(&msg_mpi_t);
}


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

	mpi_datatype_init();
}


void mpi_finalize(void){
	if(master_thread())
		MPI_Finalize();
}


#endif /* HAS_MPI */
