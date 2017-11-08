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

// Declared in ecsstub.S
extern void rootsim_cross_state_dependency_handler(void);

void ecs_fault_handler_on_segment(int signal, siginfo_t *info, void *context) {
	unsigned long i=0;
	unsigned long j;

	const ucontext_t *con = (ucontext_t *)context;
	long long target_address = (long long)info->si_addr;
	unsigned char *faulting_insn = (unsigned char *)con->uc_mcontext.gregs[REG_RIP];
	void *target_pages;
	unsigned int page_count;
	long long span;
	insn_info_x86 insn_disasm;

	(void)signal;

	printf("Faulting instruction is at %p, accessing %p\n", faulting_insn, target_address);
	fflush(stdout); 

	x86_disassemble_instruction(faulting_insn, &i, &insn_disasm, DATA_64 | ADDR_64);

	printf("\tbytes: ");
	for(j = 0; j < i; j++) {
		printf("%02x ", insn_disasm.insn[j]);
	}
	printf("\n");
	printf("\tis a memory-%s instruction (mnemonic: %s)\n", (IS_MEMRD(&insn_disasm) ? "read" : "write" ),insn_disasm.mnemonic);
	printf("\tit %sa string instruction\n", (IS_STRING(&insn_disasm) ? "" : "not "));

	if(!IS_STRING(&insn_disasm)) {
		span = insn_disasm.span;
	} else {
		span = insn_disasm.span * con->uc_mcontext.gregs[REG_RCX];
	}
	printf("\tspanned memory area: %lu\n\n", span);

	target_pages = (void *)(target_address & (~((long long)PAGE_SIZE-1)));
	page_count = ((target_address + span) & (~((long long)PAGE_SIZE-1)))/PAGE_SIZE - (long long)target_pages/PAGE_SIZE + 1;

	printf("\trequesting %d page%s starting from page at VA %p\n", page_count, (page_count > 1 ? "s" : ""), target_pages);

        mprotect(info->si_addr, 1, PROT_READ | PROT_WRITE);
	*(char *)(info->si_addr) = 'x';
}

void ECS(long long ds, unsigned long long hitted_object, unsigned char *prev_instr) __attribute__((__used__));
void ECS(long long ds, unsigned long long hitted_object, unsigned char *prev_instr) {
	(void)ds;
	msg_t control_msg;
	msg_hdr_t msg_hdr;

	if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC) {
		rootsim_error(true,"----ERROR---- ECS in Silent Execution LP[%d] Hit:%llu Timestamp:%f\n",
		current_lp, hitted_object, current_lvt);
	}

	
	//prepare data structures to get disassembled instruction 
	//insn_info_x86 *instr = (insn_info_x86*) rsalloc(sizeof(insn_info_x86));
	//unsigned long i = 0;
	//unsigned long *p = &i;

	//call disassembler
	//x86_disassemble_instruction(prev_instr,p,instr,0);

	// do whatever you want, but you need to reopen access to the objects you cross-depend on before returning

	// Generate a Rendez-Vous Mark
	// if it is present already another synchronization event we do not need to generate another mark

	if(LPS[current_lp]->wait_on_rendezvous != 0 && LPS[current_lp]->wait_on_rendezvous != current_evt->rendezvous_mark) {
		printf("muori male\n");
		fflush(stdout);
		abort();
	}

	if(LPS[current_lp]->wait_on_rendezvous == 0) {
		current_evt->rendezvous_mark = generate_mark(current_lp);
		LPS[current_lp]->wait_on_rendezvous = current_evt->rendezvous_mark;
		printf("gid %d starting a rendezvous at time %f with mark %d with LP %d on ds %d\n", LidToGid(current_lp), current_lvt, LPS[current_lp]->wait_on_rendezvous, hitted_object, ds); 
	}

	// Diretcly place the control message in the target bottom half queue
	bzero(&control_msg, sizeof(msg_t));
	control_msg.sender = LidToGid(current_lp);
	control_msg.receiver = hitted_object;
	control_msg.type = RENDEZVOUS_START;
	control_msg.timestamp = current_lvt;
	control_msg.send_time = current_lvt;
	control_msg.message_kind = positive;
	control_msg.rendezvous_mark = current_evt->rendezvous_mark;
	control_msg.mark = generate_mark(current_lp);

	// This message must be stored in the output queue as well, in case this LP rollbacks
	bzero(&msg_hdr, sizeof(msg_hdr_t));
	msg_hdr.sender = control_msg.sender;
	msg_hdr.receiver = control_msg.receiver;
	msg_hdr.type = RENDEZVOUS_START;
	msg_hdr.timestamp = control_msg.timestamp;
	msg_hdr.send_time = control_msg.send_time;
	msg_hdr.mark = control_msg.mark;
	msg_hdr.rendezvous_mark = control_msg.rendezvous_mark;
	(void)list_insert(current_lp, LPS[current_lp]->queue_out, send_time, &msg_hdr);

	// Block the execution of this LP
	LPS[current_lp]->state = LP_STATE_WAIT_FOR_SYNCH;
	LPS[current_lp]->wait_on_object = hitted_object;

	// Store which LP we are waiting for synchronization. Upon reschedule, it is open immediately
	LPS[current_lp]->ECS_index++;
	LPS[current_lp]->ECS_synch_table[LPS[current_lp]->ECS_index] = hitted_object;
	Send(&control_msg);
	
	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}


void ecs_init(void) {
	ioctl_fd = manual_open("/dev/ktblmgr", O_RDONLY);
	if (ioctl_fd <= -1) {
		rootsim_error(true, "Error in opening special device file. ROOT-Sim is compiled for using the ktblmgr linux kernel module, which seems to be not loaded.");
	}

	struct sigaction act;
        sigset_t set;

        sigfillset(&set);
        sigdelset(&set,SIGSEGV);
        sigdelset(&set,SIGINT);

        act.sa_sigaction = ecs_fault_handler_on_segment;
        act.sa_mask =  set;
        act.sa_flags = SA_SIGINFO;
        act.sa_restorer = NULL;
        sigaction(SIGSEGV,&act,NULL);
}

// inserire qui tutte le api di schedulazione/deschedulazione
void lp_alloc_thread_init(void) {
	void *ptr;

	int ret;
	ret = manual_ioctl(ioctl_fd, IOCTL_SET_ANCESTOR_PGD,NULL);  //ioctl call
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
	pgd_ds = manual_ioctl(ioctl_fd, IOCTL_GET_PGD,NULL);  //ioctl call
}

/* void lp_alloc_schedule(void) { */

/* 	unsigned int i; */
/* 	ioctl_info sched_info; */

/* 	bzero(&sched_info, sizeof(ioctl_info)); */

/* 	sched_info.ds = pgd_ds; // this is current */
/* 	sched_info.count = LPS[current_lp]->ECS_index + 1; // it's a counter */

/* 	sched_info.objects = LPS[current_lp]->ECS_synch_table; // pgd descriptor range from 0 to number threads - a subset of object ids */
/* 	sched_info.objects_mmap_count = sched_info.count; */

/* 	sched_info.objects_mmap_pointers = rsalloc(sizeof(void *) * sched_info.objects_mmap_count); */
/* 	for(i=0;i<sched_info.count;i++) { */
/*                 sched_info.objects_mmap_pointers[i] = get_base_pointer(sched_info.objects[i]); */
/* //		printf("lp_alloc_schedule - [%d] addr:%p\n",current_lp,sched_info.objects_mmap_pointers[i]); */
/*         } */

/* 	/1* passing into LP mode - here for the pgd_ds-th LP *1/ */
/* 	sched_info.count = current_lp; */
/* 	ioctl(ioctl_fd,IOCTL_SCHEDULE_ON_PGD, &sched_info); */

/* } */


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
	ioctl(ioctl_fd,IOCTL_UNSCHEDULE_ON_PGD, (void *)pgd_ds);
}


void lp_alloc_thread_fini(void) {
}

void setup_ecs_on_segment(msg_t *msg) {
	void *base_ptr;

	if(GidToKernel(msg->sender) == kid)
		return;

	// ECS on a remote LP: mprotect the whole segment
	base_ptr = get_base_pointer(msg->sender);
	printf("Setting up mprotect for LP %d base ptr %p size %d MB\n", msg->sender, base_ptr, PER_LP_PREALLOCATED_MEMORY / 1024 / 1024);
	mprotect(base_ptr + PAGE_SIZE, PER_LP_PREALLOCATED_MEMORY - PAGE_SIZE , PROT_NONE);
}
#endif
