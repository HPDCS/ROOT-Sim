#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "allocator.h"
#include <numaif.h>
#include <errno.h>


map_move moves[MAX_SOBJS];


void move_sobj(int sobj, unsigned numa_node){

	mem_map* mmap;
        mdt_entry* mdte;
        int i;
                
        if( (sobj < 0)||(sobj>=handled_sobjs) ){
                printf("move request on invalid sobj\n");
                return ; 
        }

        mmap = &maps[sobj]; 

        for(i=0;i<mmap->active;i++){
                mdte = (mdt_entry*)mmap->base + i;
                printf("moving segment in mdt entry %d - content: addr is %p - num pages is %d\n",i,mdte->addr,mdte->numpages);

		move_segment(mdte,numa_node);
        }

        return ;
}


void move_segment(mdt-entry *mdte, unsigned numa_node);

}

