/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file lp-alloc.c
* @brief LP's memory pre-allocator. This layer stands below DyMeLoR, and is the
* 		connection point to the Linux Kernel Module for Memory Management, when
* 		activated.
* @author Alessandro Pellegrini
*/

#ifdef HAVE_CROSS_STATE

#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(OS_LINUX)
#include <stropts.h>
#endif


#include <core/core.h>
//#include <mm/malloc.h>
#include <mm/allocator.h>
#include <mm/dymelor.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <arch/ult.h>

//TODO MN
#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>
#include <mm/allocator_ecs.h>

#ifdef HAVE_GLP_SCH_MODULE
#include <gvt/gvt.h>
#endif


void (*callback_function)(void);

/// This variable keeps track of information needed by the Linux Kernel Module for activating cross-LP memory accesses
static ioctl_info lp_memory_ioctl_info;

static int ioctl_fd;

/// Per Worker-Thread Memory View
static __thread int pgd_ds;

// Declared in ecsstub.S
extern void rootsim_cross_state_dependency_handler(void);

void ECS(long long ds, unsigned long long hitted_object){
	(void)ds;
	msg_t control_msg;
	msg_hdr_t msg_hdr;

	if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC) 
		rootsim_error(true,"----ERROR---- LP[%d] Hitted:%llu Timestamp:%f\n",current_lp,hitted_object,current_lvt);	

	// do whatever you want, but you need to reopen access to the objects you cross-depend on before returning

	// Generate a Rendez-Vous Mark
	// if it is presente already another synchronization event we do not need to generate another mark
	if(LPS[current_lp]->wait_on_rendezvous == 0) {
		current_evt->rendezvous_mark = generate_mark(current_lp);
		LPS[current_lp]->wait_on_rendezvous = current_evt->rendezvous_mark;
	}

#ifdef HAVE_GLP_SCH_MODULE
	PRINT_DEBUG_GLP_DETAIL{
		printf("LP[%d] hits %llu rende_mark:%llu LP_wait_on_rende:%llu\n",current_lp, hitted_object, current_evt->rendezvous_mark,LPS[current_lp]->wait_on_rendezvous);
	}
	
	//Manage counter to cross-state 
	ECS_stat* temp_update_access = LPS[current_lp]->ECS_stat_table[hitted_object];
	if(!D_EQUAL(temp_update_access->last_access,-1.0) && ((current_lvt - temp_update_access->last_access) < THRESHOLD_TIME_ECS) )
		temp_update_access->count_access++;
	else
		temp_update_access->count_access = 1;

	temp_update_access->last_access = current_lvt;
	//data structure that save the number of access and the last timestamp of the access
	
//	printf("LP:%d --> last_access:%f | count_access:%d \n",current_lp,temp_update_access->last_access,temp_update_access->count_access);
	
	#endif

	// Diretcly place the control message in the target bottom half queue
	bzero(&control_msg, sizeof(msg_t));
	control_msg.sender = LidToGid(current_lp);
	control_msg.receiver = LidToGid(hitted_object);
        control_msg.type = RENDEZVOUS_START;
	control_msg.timestamp = current_lvt;
	control_msg.send_time = current_lvt;
	control_msg.message_kind = positive;
	control_msg.rendezvous_mark = current_evt->rendezvous_mark;
	control_msg.mark = generate_mark(current_lp);

//	printf("ECS_stub lp %d sends to  %d START with  mark %llu\n", current_lp, hitted_object, control_msg.mark);

	// This message must be stored in the output queue as well, in case this LP rollbacks
	bzero(&msg_hdr, sizeof(msg_hdr_t));
	msg_hdr.sender = control_msg.sender;
	msg_hdr.receiver = control_msg.receiver;
	msg_hdr.timestamp = control_msg.timestamp;
	msg_hdr.send_time = control_msg.send_time;
	msg_hdr.mark = control_msg.mark;
	(void)list_insert(current_lp, LPS[current_lp]->queue_out, send_time, &msg_hdr);


	// Block the execution of this LP
	LPS[current_lp]->state = LP_STATE_WAIT_FOR_SYNCH;

	#ifdef HAVE_GLP_SCH_MODULE
	GLP_state *current_group;	
	current_group = GLPS[LPS[current_lp]->current_group];
	
	if(check_start_group(current_lp) && verify_time_group(current_lvt)){
		PRINT_DEBUG_GLP_DETAIL{
			printf("GLP[%d] set state WAIT_FOR_SYNCH\n",LPS[current_lp]->current_group);
		}
		current_group->state = GLP_STATE_WAIT_FOR_SYNCH;
	}
	else{
		PRINT_DEBUG_GLP_DETAIL{
			printf("Not update LP:%d G[%d] G-STATE:%d ECS CSG:%d VTG:%d \n",
			current_lp,
			LPS[current_lp]->current_group,
			current_group->state,
			check_start_group(current_lp),
			verify_time_group(current_lvt));
		}
	}

	#endif

	LPS[current_lp]->wait_on_object = LidToGid(hitted_object);

	// Store which LP we are waiting for synchronization. Upon reschedule, it is open immediately
	LPS[current_lp]->ECS_index++;
	LPS[current_lp]->ECS_synch_table[LPS[current_lp]->ECS_index] = hitted_object;
	
	Send(&control_msg);

	// TODO: QUESTA RIGA E' COMMENTATA SOLTANTO PER UNO DEI TEST!!
	// Give back control to the simulation kernel's user-level thread
//	context_switch(&LPS[current_lp]->context, &kernel_context);
	long_jmp(&kernel_context, kernel_context.rax);

}

// inserire qui tutte le api di schedulazione/deschedulazione

void lp_alloc_thread_init(void) {
	
        ioctl_fd = open("/dev/ktblmgr", O_RDONLY);
        if (ioctl_fd == -1) {
                rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
        }

        ioctl(ioctl_fd, IOCTL_SET_ANCESTOR_PGD);  //ioctl call

        lp_memory_ioctl_info.ds = -1;
        lp_memory_ioctl_info.mapped_processes = n_prc;

        callback_function =  rootsim_cross_state_dependency_handler;
        lp_memory_ioctl_info.callback = callback_function;

	ioctl(ioctl_fd, IOCTL_SET_VM_RANGE, &lp_memory_ioctl_info);
	
	/* required to manage the per-thread memory view */
	pgd_ds = ioctl(ioctl_fd, IOCTL_GET_PGD);  //ioctl call
}

bool present_ECS_table(unsigned int lid){
	unsigned int i;
	unsigned int *table = LPS[current_lp]->ECS_synch_table;

	for(i=0;i<(LPS[current_lp]->ECS_index + 1);i++){
		if(table[i] == lid)	return true;
	}

	return false;
}


void lp_alloc_schedule(void) {
	
	unsigned int i;
	ioctl_info sched_info;

	bzero(&sched_info, sizeof(ioctl_info));

	sched_info.ds = pgd_ds; // this is current
	sched_info.count = LPS[current_lp]->ECS_index + 1; // it's a counter
	
	//TODO MN open group memory view only if lvt < GVT+deltaT	
	#ifdef HAVE_GLP_SCH_MODULE
	LP_state **list;	
	if(check_start_group(current_lp) && verify_time_group(lvt(current_lp))){
		list = GLPS[LPS[current_lp]->current_group]->local_LPS;
        	for(i=0; i< GLPS[LPS[current_lp]->current_group]->tot_LP; i++){
                	if(list[i]->lid != current_lp && !present_ECS_table(list[i]->lid)){
        			LPS[current_lp]->ECS_synch_table[sched_info.count] = list[i]->lid;
				sched_info.count++;
			}
		}
	}
	#endif	
	
	sched_info.objects = LPS[current_lp]->ECS_synch_table; // pgd descriptor range from 0 to number threads - a subset of object ids 	
	//TODO MN
	sched_info.objects_mmap_count = 0;
	
	sched_info.objects_mmap_count = sched_info.count;	
	
	sched_info.objects_mmap_pointers = rsalloc(sizeof(void *) * sched_info.objects_mmap_count);	
	for(i=0;i<sched_info.count;i++) {
                sched_info.objects_mmap_pointers[i] = get_base_pointer(sched_info.objects[i]);
//		printf("lp_alloc_schedule - [%d] addr:%p\n",current_lp,sched_info.objects_mmap_pointers[i]);
        }

	/* passing into LP mode - here for the pgd_ds-th LP */
	sched_info.count = current_lp;
	ioctl(ioctl_fd,IOCTL_SCHEDULE_ON_PGD, &sched_info);
	
}


void lp_alloc_deschedule(void) {
	/* stepping back into non-LP mode */
	/* all previously scheduled (memory opened) LPs are also unscheduled by default */
	/* reaccessing their state will give rise to traps (if not scheduled again) */
	ioctl(ioctl_fd,IOCTL_UNSCHEDULE_ON_PGD, pgd_ds);
}




void lp_alloc_thread_fini(void) {
}
#endif
