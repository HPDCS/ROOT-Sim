#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>

typedef struct _mem_map{
	char* base;   //base address of the chain of meta-data tables for the memory map of the sobj	
	int   size;   //maximum number of entries in the current meta-data tables of the memory map of the sobj
	int   active;   //number of valid entries in the meta-data tables of the memory map of the sobj
	char* live_bh; //address of the live bottom half for the sobj
	char* expired_bh; //address of the expired bottom half
	int   live_msgs; //number of messages currently present inthe the live bottom half
	int   live_offset; // offset of the oldest undelivered msg from the expired pool
	int   live_boundary; //memory occupancy (in bytes) of live messages
	int   expired_msgs; //number of messages currently present in the live bottom half
	int   expired_offset; // offset of the oldest undelivered msg from the expired pool
	int   expired_boundary; //memory occupancy (in bytes) of live messages
} mem_map; 

typedef struct _mdt_entry{ //mdt stands for 'meta data table'
	char* addr;
	int   numpages;
} mdt_entry;

typedef struct _map_move{
	pthread_spinlock_t spinlock;
	unsigned 	   target_node;
	int      	   need_move;
	int    		   in_progress;
} map_move; 


#define PAGE_SIZE (4*1<<10)
#define MDT_ENTRIES ((PAGE_SIZE)/sizeof(mdt_entry))
#define MAX_SEGMENT_SIZE 256 //this is expressed in number of pages
 
#define MAX_SOBJS  	1024

#define SUCCESS 		0
#define FAILURE			-1
#define INVALID_SOBJS_COUNT 	-99
#define INIT_ERROR		-98
#define INVALID_SOBJ_ID 	-97
#define MDT_RELEASE_FAILURE	-96

char* allocate_mdt(void);
char* allocate_page(void);
mdt_entry* get_new_mdt_entry(int );
int init_allocator(int);
void* allocate_segment(int , int );
void audit(void);
int release_mdt_entry(int);
void audit_map(int);
void move_sobj(int , unsigned );
void move_segment(mdt_entry *, unsigned );
void set_daemon_maps(mem_map *, map_move* );
int init_move(int);
int lock(int);
int unlock(int);
int move_request(int , int );
void set_BH_map(mem_map* );
int init_BH(void);

int insert_BH(int , void*, int );
void* get_BH(int);
