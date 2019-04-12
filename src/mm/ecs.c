/**
* @file mm/ecs.c
*
* @brief Event & Cross State Synchornization
*
* Event & Cross State Synchronization. This module implements the userspace handler
* of the artificially induced memory faults to implement transparent distributed memory.
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @author Alessandro Pellegrini
* @author Francesco Quaglia
* @author Matteo Principe
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
#include <mm/mm.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <communication/communication.h>
#include <arch/ult.h>
#include <arch/x86/disassemble.h>

#include <arch/x86/linux/cross_state_manager/cross_state_manager.h>

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

	GID_t target_gid;
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

//	printf("ECS Page Fault: LP %d accessing %d pages from %p on LP %lu in %s mode\n", current->gid, page_req.count, (void *)page_req.base_address, fault_info.target_gid, (page_req.write_mode ? "write" : "read"));
	fflush(stdout);

	// Send the page lease request control message. This is not incorporated into the input queue at the receiver
	// so we do not place it into the output queue
	target_gid.id = fault_info.target_gid;
	pack_msg(&control_msg, current->gid, target_gid, RENDEZVOUS_GET_PAGE, current_lvt, current_lvt, sizeof(page_req), &page_req);
	control_msg->rendezvous_mark = current_evt->rendezvous_mark;
	Send(control_msg);

	current->state = LP_STATE_WAIT_FOR_DATA;

	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}

void ecs_initiate(void) {
	msg_t *control_msg;
	msg_hdr_t *msg_hdr;

	GID_t target_gid;
	// Generate a unique mark for this ECS
	current_evt->rendezvous_mark = generate_mark(current);
	current->wait_on_rendezvous = current_evt->rendezvous_mark;

	// Prepare the control message to synchronize the two LPs
	target_gid.id = fault_info.target_gid;
	pack_msg(&control_msg, current->gid, target_gid, RENDEZVOUS_START, current_lvt, current_lvt, 0, NULL);
	control_msg->rendezvous_mark = current_evt->rendezvous_mark;
	control_msg->mark = generate_mark(current);

	// This message must be stored in the output queue as well, in case this LP rollbacks
	msg_hdr =  get_msg_hdr_from_slab();
	msg_to_hdr(msg_hdr, control_msg);
	list_insert(current->queue_out, send_time, msg_hdr);

	// Block the execution of this LP
	current->state = LP_STATE_WAIT_FOR_SYNCH;
	current->wait_on_object = fault_info.target_gid;

	// Store which LP we are waiting for synchronization.
	current->ECS_index++;
	current->ECS_synch_table[current->ECS_index] = target_gid;
	Send(control_msg);

	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}

// This handler is called to initiate an ECS, both in the local and in the distributed case
void ECS(void) __attribute__((__used__));
void ECS(void) {
	// ECS cannot happen in silent execution, as we take a log after the completion
	// of an event which involves one or multiple ecs
	if(unlikely(current->state == LP_STATE_SILENT_EXEC)) {
		rootsim_error(true,"----ERROR---- ECS in Silent Execution LP[%d] Hit:%llu Timestamp:%f\n",
		current->lid.to_int, fault_info.target_gid, current_lvt);
	}

	// Sanity check: we cannot run an ECS with an old mark after a rollback
	if(current->wait_on_rendezvous != 0 && current->wait_on_rendezvous != current_evt->rendezvous_mark) {
		printf("muori male\n");
		fflush(stdout);
		abort();
	}

//	printf("Entro nell'ECS handler per un fault di tipo %d\n", fault_info.fault_type);

	switch(fault_info.fault_type) {

		case ECS_MAJOR_FAULT:
			ecs_initiate();
			break;

		case ECS_MINOR_FAULT:
			// TODO: gestire qui la read/write list!!!!
			ecs_secondary();
			break;

		case ECS_CHANGE_PAGE_PRIVILEGE:
			// TODO: gestire qui la write list!!!!
			lp_alloc_schedule(); // We moved to the original view in the kernel module: we do not unschedule the LP here
			break;

		default:
			rootsim_error(true, "Impossible condition! Aborting...\n");
			return;
	}
}

void ecs_init(void) {
	//printf("Invocation of ECS Init\n");
	ioctl_fd = open("/dev/ktblmgr", O_RDONLY);
	if (ioctl_fd <= -1) {
		rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
	}
}

// inserire qui tutte le api di schedulazione/deschedulazione
void lp_alloc_thread_init(void) {
	void *ptr;
	GID_t LP0; //dummy structure just to accomplish get_base_pointer

	LP0.id = 0;
	// TODO: test ioctl return value
	ioctl(ioctl_fd, IOCTL_SET_ANCESTOR_PGD,NULL);  //ioctl call
	lp_memory_ioctl_info.ds = -1;
	ptr = get_base_pointer(LP0); // LP 0 is the first allocated one, and it's memory stock starts from the beginning of the PML4
	lp_memory_ioctl_info.addr = ptr;
	lp_memory_ioctl_info.mapped_processes = n_prc_tot;

	callback_function =  rootsim_cross_state_dependency_handler;
	lp_memory_ioctl_info.callback = (ulong) callback_function;

	// TODO: this function is called by each worker thread. Does calling SET_VM_RANGE cause
  // a memory leak into kernel space?
	// TODO: Yes it does! And there could be some issues when unmounting as well!
	ioctl(ioctl_fd, IOCTL_SET_VM_RANGE, &lp_memory_ioctl_info);

	/* required to manage the per-thread memory view */
	pgd_ds = ioctl(ioctl_fd, IOCTL_GET_PGD, &fault_info);  //ioctl call
}

void lp_alloc_schedule(void) {

	ioctl_info sched_info;
	bzero(&sched_info, sizeof(ioctl_info));

	sched_info.ds = pgd_ds;
	sched_info.count = current->ECS_index + 1; // it's a counter
	sched_info.objects = (unsigned int*) current->ECS_synch_table; // pgd descriptor range from 0 to number threads - a subset of object ids

	/* passing into LP mode - here for the pgd_ds-th LP */
	ioctl(ioctl_fd,IOCTL_SCHEDULE_ON_PGD, &sched_info);
}


void lp_alloc_deschedule(void) {
	/* stepping back into non-LP mode */
	/* all previously scheduled (memory opened) LPs are also unscheduled by default */
	/* reaccessing their state will give rise to traps (if not scheduled again) */
	ioctl(ioctl_fd,IOCTL_UNSCHEDULE_ON_PGD, pgd_ds);
}

void setup_ecs_on_segment(msg_t *msg) {
	ioctl_info sched_info;

//	printf("Eseguo setup_ecs_on_segment per il messaggio:\n");
//	dump_msg_content(msg);

	// In case of a remote ECS, protect the memory
	if(find_kernel_by_gid(msg->sender) != kid) {
		//printf("Mi sincronizzo con un LP remoto e proteggo la memoria\n");
		bzero(&sched_info, sizeof(ioctl_info));
		sched_info.base_address = get_base_pointer(msg->sender);
		ioctl(ioctl_fd, IOCTL_PROTECT_REMOTE_LP, &sched_info);
	}
}

void lp_alloc_thread_fini(void) {
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

	//printf("LP %d sending %d pages from %p to %d\n", msg->receiver, the_request->count, the_request->base_address, msg->sender);
	fflush(stdout);

	memcpy(the_pages->buffer, the_request->base_address, the_request->count * PAGE_SIZE);

	// Send back a copy of the pages!
	pack_msg(&control_msg, msg->receiver, msg->sender, RENDEZVOUS_GET_PAGE_ACK, msg->timestamp, msg->timestamp, sizeof(ecs_page_request_t)+the_pages->count * PAGE_SIZE, the_pages);
	control_msg->mark = generate_mark(GidToLid(msg->receiver));
	control_msg->rendezvous_mark = msg->rendezvous_mark;
	Send(control_msg);

	rsfree(the_pages);
}

void ecs_install_pages(msg_t *msg) {
	ecs_page_request_t *the_pages = (ecs_page_request_t *)&(msg->event_content);
	ioctl_info sched_info;

	//printf("LP %d receiving %d pages from %p from %d\n", msg->receiver, the_pages->count, the_pages->base_address, msg->sender);
	fflush(stdout);

	memcpy(the_pages->base_address, the_pages->buffer, the_pages->count * PAGE_SIZE);

	//printf("Completed the installation of the page copying %d bytes\n", the_pages->count * PAGE_SIZE);
	fflush(stdout);

	bzero(&sched_info, sizeof(ioctl_info));
	sched_info.base_address = the_pages->base_address;
	sched_info.page_count = the_pages->count;
	sched_info.write_mode = the_pages->write_mode;

	// TODO: se accedo in write non devo fare questa chiamata!
//	ioctl(ioctl_fd, IOCTL_SET_PAGE_PRIVILEGE, &sched_info);

	//printf("Completato il setup dei privilegi\n");
	fflush(stdout);
}

void unblock_synchronized_objects(struct lp_struct *lp) {
	unsigned int i;
	msg_t *control_msg;

	for(i = 1; i <= lp->ECS_index; i++) {
		pack_msg(&control_msg, LidToGid(localID), lp->ECS_synch_table[i], RENDEZVOUS_UNBLOCK, lvt(localID), lvt(localID), 0, NULL);
		control_msg->rendezvous_mark = lp->wait_on_rendezvous;
		Send(control_msg);
	}

	lp->wait_on_rendezvous = 0;
	lp->ECS_index = 0;
}

void remote_memory_init(void) {
	foreach_lp(lp) {
		if(find_kernel_by_gid(lp->gid) != kid) {
			(void)get_segment(lp->gid);
		}
	}
}
#endif
