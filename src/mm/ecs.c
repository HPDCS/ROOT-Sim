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
#include <signal.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#if defined(OS_LINUX)
#include <stropts.h>
#endif


#include <core/core.h>
#include <mm/dymelor.h>
#include <mm/mm.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <arch/ult.h>
#include <arch/x86.h>

#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>

#define __NR_OPEN 2

void (*callback_function)(void);

/// This variable keeps track of information needed by the Linux Kernel Module for activating cross-LP memory accesses
static ioctl_info lp_memory_ioctl_info;

static int ioctl_fd;

/// Per Worker-Thread Memory View
static __thread int pgd_ds;

/// Per Worker-Thread Fault Info Structure
static __thread fault_info_t fault_info;

// Declared in ecsstub.S
extern void rootsim_cross_state_dependency_handler(void);


// This handler is only called in case of a remote ECS
void ecs_secondary(void) {

	// target_address is filled by the ROOT-Sim fault handler at kernel level before triggering the signal
	long long target_address = fault_info.target_address;
	unsigned char *faulting_insn = (unsigned char *)fault_info.rip;
	long long span;
	insn_info_x86 insn_disasm;
	unsigned long i=0;
	msg_t *control_msg;
	ecs_page_request_t page_req;

	// Disassemble the faulting instruction to get necessary information
	x86_disassemble_instruction(faulting_insn, &i, &insn_disasm, DATA_64 | ADDR_64);

	if(!IS_STRING(&insn_disasm)) {
		span = insn_disasm.span;
	} else {
		span = insn_disasm.span * fault_info.rcx;
	}

	// Compute the starting page address and the page count to get the lease
	page_req.write_mode = IS_MEMWR(&insn_disasm);
	page_req.base_address = (void *)(target_address & (~((long long)PAGE_SIZE-1)));
	page_req.count = ((target_address + span) & (~((long long)PAGE_SIZE-1)))/PAGE_SIZE - (long long)page_req.base_address/PAGE_SIZE + 1;

	// Send the page lease request control message. This is not incorporated into the input queue at the receiver
	// so we do not place it into the output queue
	bzero(&control_msg, sizeof(msg_t));
	pack_msg(&control_msg, LidToGid(current_lp), fault_info.target_gid, RENDEZVOUS_GET_PAGE, current_lvt, current_lvt, sizeof(page_req), &page_req);
	control_msg->rendezvous_mark = current_evt->rendezvous_mark;
	Send(control_msg);

	printf("ECS Page Fault: LP %d accessing %d pages from %p on LP %lu in %s mode\n", current_lp, page_req.count, (void *)page_req.base_address, fault_info.target_gid, (page_req.write_mode ? "write" : "read"));
	fflush(stdout);

	// Pre-materialization of the pages on the local node
        mprotect(page_req.base_address, page_req.count * PAGE_SIZE, PROT_WRITE);
	for(i = 0; i < page_req.count; i++) {
		*((char *)page_req.base_address + i * PAGE_SIZE) = 'x';
	}	
        mprotect(page_req.base_address, page_req.count * PAGE_SIZE, PROT_NONE);

	LPS[current_lp]->state = LP_STATE_WAIT_FOR_DATA;

	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}

void ecs_initiate(void) {
	msg_t *control_msg;
	msg_hdr_t msg_hdr;

	// Generate a unique mark for this ECS
	current_evt->rendezvous_mark = generate_mark(current_lp);
	LPS[current_lp]->wait_on_rendezvous = current_evt->rendezvous_mark;

	// Prepare the control message to synchronize the two LPs
	pack_msg(&control_msg, LidToGid(current_lp), fault_info.target_gid, RENDEZVOUS_START, current_lvt, current_lvt, 0, NULL);
	control_msg->rendezvous_mark = current_evt->rendezvous_mark;
	control_msg->mark = generate_mark(current_lp);

	// This message must be stored in the output queue as well, in case this LP rollbacks
	msg_to_hdr(&msg_hdr, control_msg);
	(void)list_insert(current_lp, LPS[current_lp]->queue_out, send_time, &msg_hdr);

	// Block the execution of this LP
	LPS[current_lp]->state = LP_STATE_WAIT_FOR_SYNCH;
	LPS[current_lp]->wait_on_object = fault_info.target_gid;

	// Store which LP we are waiting for synchronization. Upon reschedule, it is open immediately
	LPS[current_lp]->ECS_index++;
	LPS[current_lp]->ECS_synch_table[LPS[current_lp]->ECS_index] = fault_info.target_gid;
	Send(control_msg);
	
	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}

// This handler is called to initiate an ECS, both in the local and in the distributed case
void ECS(void) __attribute__((__used__));
void ECS(void) {
	unsigned int i;

	// ECS cannot happen in silent execution, as we take a log after the completion
	// of an event which involves one or multiple ecs
	if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC) {
		rootsim_error(true,"----ERROR---- ECS in Silent Execution LP[%d] Hit:%llu Timestamp:%f\n",
		current_lp, fault_info.target_gid, current_lvt);
	}

	// Sanity check: we cannot run an ECS with an old mark after a rollback
	if(LPS[current_lp]->wait_on_rendezvous != 0 && LPS[current_lp]->wait_on_rendezvous != current_evt->rendezvous_mark) {
		printf("muori male\n");
		fflush(stdout);
		abort();
	}

	// If we do not have a rendezvous mark, this is the first fault ever associated with the current event
	if(LPS[current_lp]->wait_on_rendezvous == 0) {
		ecs_initiate();
		return;  // This return will never be executed, as we save the context in the assembly trampoline
	}

	// If we are synchronizing with a new LP, we have to initiate the protocol with it.
	// Otherwise, we are in the case of a distributed ECS and we have to get a page lease.
	for(i = 0; i < 	LPS[current_lp]->ECS_index; i++) {
		if(LPS[current_lp]->ECS_synch_table[i] == fault_info.target_gid) {
			ecs_secondary();
			return;  // This return will never be executed, as we save the context in the assembly trampoline
		}
	}

	// Then, we're initiating an ECS with a new LP
	ecs_initiate();	
}

void ecs_init(void) {
	ioctl_fd = manual_open("/dev/ktblmgr", O_RDONLY);
	if (ioctl_fd <= -1) {
		rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
	}
}

// inserire qui tutte le api di schedulazione/deschedulazione
void lp_alloc_thread_init(void) {
	void *ptr;

	// TODO: test ioctl return value
	manual_ioctl(ioctl_fd, IOCTL_SET_ANCESTOR_PGD,NULL);  //ioctl call
	lp_memory_ioctl_info.ds = -1;
	ptr = get_base_pointer(0); // LP 0 is the first allocated one, and it's memory stock starts from the beginning of the PML4
	lp_memory_ioctl_info.addr = ptr;
	lp_memory_ioctl_info.mapped_processes = n_prc;

	callback_function =  rootsim_cross_state_dependency_handler;
	lp_memory_ioctl_info.callback = (ulong) callback_function;

	// TODO: this function is called by each worker thread. Does calling SET_VM_RANGE cause
  // a memory leak into kernel space?
	// TODO: Yes it does! And there could be some issues when unmounting as well!
	manual_ioctl(ioctl_fd, IOCTL_SET_VM_RANGE, &lp_memory_ioctl_info);

	/* required to manage the per-thread memory view */
	pgd_ds = manual_ioctl(ioctl_fd, IOCTL_GET_PGD, &fault_info);  //ioctl call
	fault_info.target_gid = 3;
}

void lp_alloc_schedule(void) {

	ioctl_info sched_info;
	bzero(&sched_info, sizeof(ioctl_info));

	sched_info.ds = pgd_ds;
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

void setup_ecs_on_segment(msg_t *msg) {
	void *base_ptr;

	if(GidToKernel(msg->sender) == kid)
		return;

	// ECS on a remote LP: mprotect the whole segment
	base_ptr = get_base_pointer(msg->sender);
	printf("Setting up mprotect for LP %d base ptr %p size %ld MB\n", msg->sender, base_ptr, PER_LP_PREALLOCATED_MEMORY / 1024 / 1024);
	mprotect((char *)base_ptr + PAGE_SIZE, PER_LP_PREALLOCATED_MEMORY - PAGE_SIZE , PROT_NONE);
}

void ecs_send_pages(msg_t *msg) {
	msg_t *control_msg;
	ecs_page_request_t *the_request;
	ecs_page_request_t *the_pages;

	the_request = (ecs_page_request_t *)&(msg->event_content);
	the_pages = rsalloc(sizeof(ecs_page_request_t) + the_request->count * PAGE_SIZE);
	the_pages->write_mode = the_request->write_mode;
	the_pages->base_address = the_request->base_address;
	the_pages->count = the_request->count;

	printf("LP %d sending %d pages from %p to %d\n", msg->receiver, the_request->count, the_request->base_address, msg->sender);
	fflush(stdout);

	memcpy(the_pages->buffer, the_request->base_address, the_request->count * PAGE_SIZE);

	// Send back a copy of the pages!
	pack_msg(&control_msg, msg->receiver, msg->sender, RENDEZVOUS_GET_PAGE_ACK, msg->timestamp, msg->timestamp, 0, NULL);
	control_msg->rendezvous_mark = msg->rendezvous_mark;
	Send(control_msg);

	rsfree(the_pages);
}

void ecs_install_pages(msg_t *msg) {
	ecs_page_request_t *the_pages = (ecs_page_request_t *)&(msg->event_content);
	char mode = PROT_READ;

	if(the_pages->write_mode)
		mode |= PROT_WRITE;
	
	printf("LP %d receiving %d pages from %p from %d\n", msg->receiver, the_pages->count, the_pages->base_address, msg->sender);
	fflush(stdout);

        mprotect(the_pages->base_address, the_pages->count * PAGE_SIZE, mode);

	memcpy(the_pages->buffer, the_pages->base_address, the_pages->count * PAGE_SIZE);
}

void unblock_synchronized_objects(unsigned int lid) {
	unsigned int i;
	msg_t *control_msg;

	for(i = 1; i <= LPS[lid]->ECS_index; i++) {
		pack_msg(&control_msg, LidToGid(lid), LPS[lid]->ECS_synch_table[i], RENDEZVOUS_UNBLOCK, lvt(lid), lvt(lid), 0, NULL);
		control_msg->rendezvous_mark = LPS[lid]->wait_on_rendezvous;
		Send(control_msg);
	}

	LPS[lid]->wait_on_rendezvous = 0;
	LPS[lid]->ECS_index = 0;
}
#endif

