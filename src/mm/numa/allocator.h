#include <stdlib.h>
#include <sys/mman.h>

typedef struct _mem_map{
	char* base;   //base address of the chain of meta-data tables for the memory map of the sobj	
	int   size;   //maximum number of entries in the current meta-data tables of the memory map of the sobj
	int   active;   //number of valid entries in the meta-data tables of the memory map of the sobj
	char* live_bh; //address of the live bottom half for the sobj
	char* expired_bh; //address of the expired bottom half
} mem_map; 

typedef struct _mdt_entry{ //mdt stands for 'meta data table'
	char* addr;
	int   numpages;
} mdt_entry;


#define PAGE_SIZE (4*1<<10)
#define MDT_ENTRIES ((PAGE_SIZE)/sizeof(mdt_entry))
#define MAX_SEGMENT_SIZE 256 //this is expressed in number of pages
 
#define MAX_SOBJS  	1024

#define SUCCESS 		0
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
