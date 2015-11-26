
#include <core/core.h>

typedef struct _lp_mem_region{
	char* base_pointer;
	char* brk;
}lp_mem_region;

#define PER_LP_PREALLOCATED_MEMORY	512*512*4096 // Allow 1 GB of virtual space per LP

#define SUCCESS_AECS                  0
#define FAILURE_AECS                 -1
#define INVALID_SOBJS_COUNT_AECS     -99
#define INIT_ERROR_AECS              -98
#define INVALID_SOBJ_ID_AECS         -97
#define MDT_RELEASE_FAILURE_AECS     -96

int allocator_ecs_init(unsigned int);
void allocator_ecs_fini(unsigned int);
char* get_memory_ecs(unsigned int, size_t);
void* get_base_pointer(unsigned int);
