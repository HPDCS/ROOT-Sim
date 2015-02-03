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
#include <mm/malloc.h>
#include <mm/dymelor.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <mm/modules/ktblmgr/ktblmgr.h>
#include <arch/ult.h>

/// This variable keeps track of per-LP allocated (and assigned) memory regions
static struct _lp_memory *lp_memory_regions;

void (*callback_function)(void);


#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
/// This variable keeps track of information needed by the Linux Kernel Module for activating cross-LP memory accesses
static ioctl_info lp_memory_ioctl_info;


static int ioctl_fd;

/// Per Worker-Thread Memory View
static __thread int pgd_ds;



static void ECS_stub(int ds, unsigned int hitted_object){
	ioctl_info sched_info;
	msg_t control_msg;
	msg_hdr_t msg_hdr;

	printf("ECS synch started on pgd %d - start wait for hitted object num %d by %d\n", ds, hitted_object, current_lp); 
	fflush(stdout);

	// do whatever you want, but you need to reopen access to the objects you cross-depend on before returning

	// Generate a Rendez-Vous Mark
	if(LPS[current_lp]->wait_on_rendezvous == 0) {
		current_evt->rendezvous_mark = generate_mark(current_lp);
		LPS[current_lp]->wait_on_rendezvous = current_evt->rendezvous_mark;
	}

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

	printf("Nello stub lp %d manda a %d START con mark %llu\n", current_lp, hitted_object, control_msg.mark);

	// This message must be stored in the output queue as well, in case this LP rollbacks
	bzero(&msg_hdr, sizeof(msg_hdr_t));
	msg_hdr.sender = control_msg.sender;
	msg_hdr.receiver = control_msg.receiver;
	msg_hdr.timestamp = control_msg.timestamp;
	msg_hdr.send_time = control_msg.send_time;
	msg_hdr.mark = control_msg.mark;
	(void)list_insert(LPS[current_lp]->queue_out, send_time, &msg_hdr);


	// Block the execution of this LP 
	LPS[current_lp]->state = LP_STATE_WAIT_FOR_SYNCH;
	LPS[current_lp]->wait_on_object = LidToGid(hitted_object);

	// Store which LP we are waiting for synchronization. Upon reschedule, it is open immediately
	LPS[current_lp]->ECS_index++;
	LPS[current_lp]->ECS_synch_table[LPS[current_lp]->ECS_index] = hitted_object;
	
	Send(&control_msg);
	
	// TODO: QUESTA RIGA E' COMMENTATA SOLTANTO PER UNO DEI TEST!!
	// Give back control to the simulation kernel's user-level thread
	context_switch(&LPS[current_lp]->context, &kernel_context);
//	lp_alloc_schedule();
	

//	sched_info.ds = ds;
//	sched_info.count = ECS_indexes[ds] + 1; // +1 since index ranges from 0
//	sched_info.objects = ECS_synch_table[ds];
	/* passing into LP mode - here for the ECS table registered objects */
//	ioctl(ioctl_fd,IOCTL_SCHEDULE_ON_PGD, &sched_info);
	
} 


static void rootsim_cross_state_dependency_handler(void) { // for now a simple printf of the cross-state dependency involved LP

	unsigned long __id = -1;
	void *__addr  = NULL;
	unsigned long __pgd_ds = -1;


	__asm__ __volatile__("push %rax");
	__asm__ __volatile__("push %rbx");
	__asm__ __volatile__("push %rcx");
	__asm__ __volatile__("push %rdx");
	__asm__ __volatile__("push %rdi");
	__asm__ __volatile__("push %rsi");
	__asm__ __volatile__("push %r8");
	__asm__ __volatile__("push %r9");
	__asm__ __volatile__("push %r10");
	__asm__ __volatile__("push %r11");
	__asm__ __volatile__("push %r12");
	__asm__ __volatile__("push %r13");
	__asm__ __volatile__("push %r14");
	__asm__ __volatile__("push %r15");
	//__asm__ __volatile__("movq 0x10(%%rbp), %%rax; movq %%rax, %0" : "=m"(addr) : );
	//__asm__ __volatile__("movq 0x8(%%rbp), %%rax; movq %%rax, %0" : "=m"(id) : );

	__asm__ __volatile__("movq 0x18(%%rbp), %%rax; movq %%rax, %0" : "=m"(__addr) : );
	__asm__ __volatile__("movq 0x10(%%rbp), %%rax; movq %%rax, %0" : "=m"(__id) : );
	__asm__ __volatile__("movq 0x8(%%rbp), %%rax; movq %%rax, %0" : "=m"(__pgd_ds) : );
	
	printf("rootsim callback received by the kernel need to sync, pointer passed is %p - hitted object is %u - pdg is %u\n", __addr, __id, __pgd_ds);
	ECS_stub((int)__pgd_ds,(unsigned int)__id);
	__asm__ __volatile__("pop %r15");
	__asm__ __volatile__("pop %r14");
	__asm__ __volatile__("pop %r13");
	__asm__ __volatile__("pop %r12");
	__asm__ __volatile__("pop %r11");
	__asm__ __volatile__("pop %r10");
	__asm__ __volatile__("pop %r9");
	__asm__ __volatile__("pop %r8");
	__asm__ __volatile__("pop %rsi");
	__asm__ __volatile__("pop %rdi");
	__asm__ __volatile__("pop %rdx");
	__asm__ __volatile__("pop %rcx");
	__asm__ __volatile__("pop %rbx");
	__asm__ __volatile__("pop %rax");
//	__asm__ __volatile__("addq $0x18 , %rsp ; popq %rbp ;  addq $0x8, %rsp ; retq");
//	__asm__ __volatile__("addq $0x20 , %rsp ;  popq %rbp; addq $0x10 , %rsp ; retq");
	__asm__ __volatile__("addq $0x28 , %rsp ; popq %rbx ; popq %rbp; addq $0x10 , %rsp ; retq"); // BUONA CON LA PRINTF
//	__asm__ __volatile__("addq $0x50 , %rsp ; popq %rbx ; popq %rbp; addq $0x10 , %rsp ; retq"); // BUONA SENZA PRINTF
}
#endif




void *lp_malloc_unscheduled(unsigned int lid, size_t s) {
	
	void *ret = NULL;

	if((char *)lp_memory_regions[lid].brk + s < (char *)lp_memory_regions[lid].start + PER_LP_PREALLOCATED_MEMORY) {
		ret = lp_memory_regions[lid].brk;
		bzero(ret, s);
		lp_memory_regions[lid].brk = (void *)((char *)lp_memory_regions[lid].brk + s);
	}

	return ret;
}


void lp_free(void *ptr) {
	(void)ptr;

	// TODO: so far, free does not really frees the LP memory :(
	// This is not a great problem, as a lot of stuff must happen before DyMeLoR
	// really calls free, and this is unlikely to happen often...

}


void *lp_realloc(void *ptr, size_t new_s) {
	(void)ptr;
	(void)new_s;

	// TODO: so far, realloc is not implemented. This is a problem with certain
	// models which are very memory-greedy. This will be implemented soon...

	return NULL;
}


void lp_alloc_init(void) {
	
	unsigned int i;
	
	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	int ret;

	ioctl_fd = open("/dev/ktblmgr", O_RDONLY);
	if (ioctl_fd == -1) {
		rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
	}
	
	/* reset of the whole special device file */
	ret = ioctl(ioctl_fd, IOCTL_SET_ANCESTOR_PGD);  //ioctl call
	printf("set ancestor returned %d\n",ret);
	#endif
	

	lp_memory_regions = rsalloc(n_prc * sizeof(struct _lp_memory));

	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	lp_memory_ioctl_info.ds = -1;
	lp_memory_ioctl_info.addr = LP_PREALLOCATION_INITIAL_ADDRESS;
	lp_memory_ioctl_info.mapped_processes = n_prc;
	
	callback_function =  rootsim_cross_state_dependency_handler;
	lp_memory_ioctl_info.callback = callback_function;


	printf("indirizzo originale %p, passato %p\n",  rootsim_cross_state_dependency_handler,  lp_memory_ioctl_info.callback);
	#endif


	// Linux Kernel has an internal limit to mmap requests, but consecutive calls are adjacent!
	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	unsigned int num_mmap = lp_memory_ioctl_info.mapped_processes * 2;
	char *addr = lp_memory_ioctl_info.addr;
	#else
	unsigned int num_mmap = n_prc * 2;
	char *addr = LP_PREALLOCATION_INITIAL_ADDRESS;
	#endif
	size_t size = PER_LP_PREALLOCATED_MEMORY / 2;
	int allocation_counter = 0;

	for(i = 0; i < num_mmap; i++) {
		
		// map memory
		addr = mmap((void*)addr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,0,0);
		// Access the memory in write mode to force the kernel to create the page table entries
		addr[0] = 'x';
		addr[1] = addr[0];
		
		// Keep track of the per-LP allocated memory
		if(allocation_counter % 2 == 0) {
			lp_memory_regions[i/2].start = lp_memory_regions[i/2].brk = addr;
		}
		allocation_counter++;

		addr += size;
	}
	
	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	/* post of the ioctl_table to the special device file - for internal initialization */ 
	ret = ioctl(ioctl_fd, IOCTL_SET_VM_RANGE, &lp_memory_ioctl_info);
	#endif
}


void lp_alloc_fini(void) {
	
	unsigned int i;

	unsigned int num_mmap = n_prc * 2;
	size_t size = PER_LP_PREALLOCATED_MEMORY / 2;
	char *addr = LP_PREALLOCATION_INITIAL_ADDRESS;
	
	for(i = 0; i < num_mmap; i++) {
		munmap(addr, size);
		addr += size;
	}
	
	#ifdef HAVE_LINUX_KERNEL_MAP_MODULE
	close(ioctl_fd); // closing (hence releasing) the special device file
	#endif
}


#ifdef HAVE_LINUX_KERNEL_MAP_MODULE

// inserire qui tutte le api di schedulazione/deschedulazione

void lp_alloc_thread_init(void) {
	/* required to manage the per-thread memory view */
	pgd_ds = ioctl(ioctl_fd, IOCTL_GET_PGD);  //ioctl call 
	printf("rootsim thread: pgd descriptor is %d\n",pgd_ds);
	fflush(stdout);
}




void lp_alloc_schedule(void) {
	
	ioctl_info sched_info;
	
	sched_info.ds = pgd_ds; // this is current
	sched_info.count = LPS[current_lp]->ECS_index + 1; // it's a counter
	sched_info.objects = LPS[current_lp]->ECS_synch_table; // pgd descriptor range from 0 to number threads - a subset of object ids 

	/* passing into LP mode - here for the pgd_ds-th LP */
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
