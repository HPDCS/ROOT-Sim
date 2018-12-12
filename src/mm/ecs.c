/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
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
#include <communication/communication.h>
#include <communication/gvt.h>
#include <arch/ult.h>
#include <arch/x86.h>
#include <statistics/statistics.h>

#include <arch/linux/modules/cross_state_manager/cross_state_manager.h>

#define foo(x) printf(x "\n"); fflush(stdout)
#define treshold(x) ((x * alpha) + 100 - 1) / 100
// TODO: trovare un modo elegante per utilizzare soltanto LID dentro questo modulo

#define __NR_OPEN 2

void (*callback_function)(void);

/// This variable keeps track of information needed by the Linux Kernel Module for activating cross-LP memory accesses
static ioctl_info lp_memory_ioctl_info;

static int ioctl_fd;

/// Per Worker-Thread Memory View
static __thread int pgd_ds;

/// Per Worker-Thread Fault Info Structure
static __thread fault_info_t fault_info;

extern void rootsim_cross_state_dependency_handler(void);

ecs_prefetch_t* compute_scattered_data(malloc_state *state, GID_t gid, void * fault_address, int tot_pages, int write_mode, ecs_prefetch_t * pfr)
{
	int i, j = 0, max = 0, min = 0,  temp = 0;
	long long *base_pointer, end_pointer;
	long long page_count, segment_base_pointer;
	long long *pages_to_add[state->num_areas];
	long long counts[state->num_areas], utilizations[state->num_areas];
	malloc_area *current_area;
	//if base_pointer is perfectly aligned with segment, do not perform the subtraction.
	GID_t dummy; dummy.id = 0;
	segment_base_pointer = (state->busy_areas) <= 1? 0 : (long long) get_base_pointer(dummy);
	for(i = 0; i < state->num_areas ; i++){
		current_area = &(state->areas[i]);
		if(current_area->area != NULL){
			base_pointer =  (((long long) current_area->area /*- segment_base_pointer*/) & (~((long long)PAGE_SIZE - 1))); 
			end_pointer = (((long long) current_area->area /*- segment_base_pointer*/) + ((current_area->chunk_size * current_area->alloc_chunks) & 
						(~((long long)PAGE_SIZE - 1))) + PAGE_SIZE);
			
			if(base_pointer == NULL)
				continue;

			if(base_pointer == fault_address){
				base_pointer =(((long long) current_area->area + (long long) PAGE_SIZE) & (~((long long)PAGE_SIZE - 1)));	
			}

			page_count = (((long long) end_pointer) & (~((long long)PAGE_SIZE-1)))/PAGE_SIZE - (long long)base_pointer/PAGE_SIZE + 1;

			temp = (int)((page_count + ((current_area->num_chunks * current_area->chunk_size) / PAGE_SIZE) -1) / 
					((current_area->num_chunks * current_area->chunk_size)/ PAGE_SIZE)); // <-- dafuq??
			
			if(temp > max) max = temp;
			if(temp < min) min = temp;
		

			pages_to_add[j] = base_pointer;
			counts[j] = page_count;
			utilizations[j] = temp;
			j++;
		}

	}

	for(i = 0; i < j && tot_pages > 0; i++){
		if(counts[i] < utilizations[i] * tot_pages){
			pfr = add_prefetch_page(pfr, pages_to_add[i], counts[i], write_mode);
			//printf("inserting %d pages since it is less than %d\n", counts[i], utilizations[i] * tot_pages);
			tot_pages -= counts[i];
		}
		else{
			int weight = (utilizations[i] - min)/(max - min);
			//printf("inserting %d pages from %p with weight %d\n", tot_pages * weight, pages_to_add[i], weight);
			pfr = add_prefetch_page(pfr, pages_to_add[i], tot_pages * weight, write_mode);
			tot_pages = 0;
		}
	}

	return pfr;
}
ecs_prefetch_t* compute_clustered_data(malloc_state *state, GID_t gid, void *fault_address, int tot_pages, int write_mode, ecs_prefetch_t * pfr)
{
	int i;
	malloc_area *current_area;
	double temp;
	long long *base_pointer;
	long long *end_pointer;
	long long page_count;
	long long segment_base_pointer;

	GID_t dummy; dummy.id = 0;
	//if base_pointer is perfectly aligned with segment, do not perform the subtraction.
	segment_base_pointer = (state->busy_areas) <= 1? 0 : (long long) get_base_pointer(dummy);
	//printf("segment base pointer is %p\n", (long long) get_base_pointer(dummy));
	for(i = 0; i < state->num_areas; i++){
		current_area = &(state->areas[i]);
		if(current_area != NULL){
			base_pointer  = (((long long) current_area->area /*- segment_base_pointer*/) & (~((long long)PAGE_SIZE - 1)));
			end_pointer = (((long long) current_area->area /*- (long long)segment_base_pointer*/) + ((current_area->chunk_size * current_area->alloc_chunks) & (~((long long)PAGE_SIZE - 1))) + PAGE_SIZE);
			
			if(fault_address >= base_pointer && fault_address <= end_pointer){		
				base_pointer = (long long)fault_address + (long long) PAGE_SIZE & (~((long long)PAGE_SIZE - 1));
				page_count = (((long long) end_pointer) & (~((long long)PAGE_SIZE-1)))/PAGE_SIZE - (long long)fault_address/PAGE_SIZE + 1;
				//printf("fault addr is %p (new base is %p ) end pointer is %p page count is %d while tot is %d\n", fault_address, base_pointer, end_pointer, page_count, tot_pages*temp);	
				
				int counter;
				if(page_count < tot_pages){
					counter = page_count;
					tot_pages -= page_count;
				}else{
					counter = tot_pages;
					tot_pages = 0;
				}

				pfr = add_prefetch_page(pfr, base_pointer, counter, write_mode);
				return pfr;
			}
		}
	}
	return NULL;
}

ecs_prefetch_t* compute_prefetch_data(malloc_state *state, GID_t gid, int prefetch_mode, void *fault_address, int write_mode, ecs_prefetch_t * pfr)
{
	int N = LPS(GidToLid(gid))->ECS_page_faults == 0? 1 : LPS(GidToLid(gid))->ECS_page_faults;
	ecs_prefetch_t * tmp_pfr = prefetch_init();

	//as first one, add original faulting page
	pfr = add_prefetch_page(pfr, (long long*) fault_address, 1, write_mode);
	
	if(prefetch_mode == SCATTERED)
		tmp_pfr = compute_scattered_data(state, gid, fault_address, N, write_mode, pfr);
	else if(prefetch_mode == CLUSTERED)
		tmp_pfr = compute_clustered_data(state, gid, fault_address, N, write_mode, pfr);
	else
		tmp_pfr = NULL;
	
	if(tmp_pfr != NULL)
		return tmp_pfr;
	else
		return pfr;
}

ecs_page_node_t *add_page_node(long long address, size_t pages, LID_t lid) {
	ecs_page_node_t *page_node, *p;

	page_node = rsalloc(sizeof(ecs_page_node_t) + bitmap_required_size(pages));
	page_node->page_address = address;
	page_node->pages = pages;
	bitmap_initialize(page_node->write_mode, pages);

	list_insert_head(LPS(lid)->ECS_page_list, page_node);

	p = list_head(LPS(lid)->ECS_page_list);

	return page_node;
}

static ecs_page_node_t *find_page(long long address, LID_t lid, size_t *pos) {
	ecs_page_node_t *p;

	p = list_head(LPS(lid)->ECS_page_list);
	while(p != NULL) {
		if(address >= (long long) p->page_address && address < (long long)(p->page_address + p->pages * PAGE_SIZE)) {
			if(pos != NULL)
				*pos = (((address) & (~((long long)PAGE_SIZE-1))) - p->page_address) / PAGE_SIZE;
			break;
		}
		p = list_next(p);
	}

	return p;
}

static void set_write_mode(ecs_page_node_t *node, size_t page) {
	bitmap_set(node->write_mode, page);
}

// This handler is only called in case of a remote ECS
void ecs_secondary(GID_t target_gid) {

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
	
	//printf("ECS OCCURRED AT TIME %llu\n", lvt(current_lp));

	// Compute the starting page address and the page count to get the lease
	page_req.write_mode = IS_MEMWR(&insn_disasm);
	page_req.base_address = (void *)((target_address) & (~((long long)PAGE_SIZE-1)));
	page_req.count = ((target_address + span) & (~((long long)PAGE_SIZE-1)))/PAGE_SIZE - (long long)page_req.base_address/PAGE_SIZE + 1;

	int current_mode = LPS(current_lp)->ECS_current_prefetch_mode;
	//current_mode = SCATTERED;
	//goto out;
	if(fault_info.fault_type == ECS_CHANGE_PAGE_PRIVILEGE){
		//current_mode = NO_PREFETCH;
		goto out;
	}

	if(current_lvt - LPS(current_lp)->ECS_last_prefetch_switch > 10000){ // <--- dafuuq??
		LPS(current_lp)->ECS_page_faults++;
		current_mode = NO_PREFETCH;
		goto out;
	}
	else{
		//if(current_lvt - LPS(current_lp)->ECS_last_prefetch_switch > 50){ //<-- dafuqq??
				
		//	printf("MODE IS %d, cont %d scatt %d, np faults %d, tresh %d at time %llu, lp %d\n", current_mode, LPS(current_lp)->ECS_clustered_faults, LPS(current_lp)->ECS_scattered_faults, LPS(current_lp)->ECS_page_faults, treshold(LPS(current_lp)->ECS_page_faults), current_lvt, LidToGid(current_lp).id);
			//fflush(stdout);
			switch (current_mode){
				case NO_PREFETCH:
					current_mode = CLUSTERED;
					LPS(current_lp)->ECS_clustered_faults++;
					goto out;	
				
					break;
				case CLUSTERED:
					if(current_lvt - LPS(current_lp)->ECS_last_prefetch_switch > 1000 && !(LPS(current_lp)->ECS_clustered_faults <= LPS(current_lp)->ECS_page_faults)){
						current_mode = SCATTERED;
						LPS(current_lp)->ECS_scattered_faults++;
						goto out;
					}

					LPS(current_lp)->ECS_clustered_faults++;
					goto out;

					break;
				case SCATTERED:
					if(current_lvt - LPS(current_lp)->ECS_last_prefetch_switch > 1000 && !(LPS(current_lp)->ECS_scattered_faults <= LPS(current_lp)->ECS_page_faults)){				
						current_mode = NO_PREFETCH;
						LPS(current_lp)->ECS_page_faults++;
						goto out;
					}

					LPS(current_lp)->ECS_scattered_faults++;
					break;
				default:
					rootsim_error(true," Unsupported prefetch mode: %d\n", current_mode);
			}


		//}
	}

out:
	//printf("ECS CHANGING TIME %llu\n",  lvt(current_lp));
	page_req.prefetch_mode = current_mode;
	LPS(current_lp)->ECS_current_prefetch_mode = current_mode;
	LPS(current_lp)->ECS_last_prefetch_switch = current_lvt;
	
	//printf("ECS Page Fault: LP %d accessing %d pages from %p ( %p )on LP %lu in %s mode\n", LidToGid(current_lp).id, page_req.count, (void *)page_req.base_address, (void*) target_address, fault_info.target_gid, (page_req.write_mode ? "write" : "read"));
	fflush(stdout);

	// Send the page lease request control message. This is not incorporated into the input queue at the receiver
	// so we do not place it into the output queue
	pack_msg(&control_msg, LidToGid(current_lp), target_gid, RENDEZVOUS_GET_PAGE, current_lvt, current_lvt, sizeof(page_req), &page_req);
	LPS(current_lp)->state = LP_STATE_WAIT_FOR_DATA;
	control_msg->rendezvous_mark = current_evt->rendezvous_mark;
	Send(control_msg);
	
	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}

void ecs_initiate(void) {
	msg_t *control_msg;
	msg_hdr_t *msg_hdr;
	GID_t target_gid;

	// Generate a unique mark for this ECS
	current_evt->rendezvous_mark = generate_mark(current_lp);
	LPS(current_lp)->wait_on_rendezvous = current_evt->rendezvous_mark;

	// Prepare the control message to synchronize the two LPs
	target_gid.id = fault_info.target_gid;
	pack_msg(&control_msg, LidToGid(current_lp), target_gid, RENDEZVOUS_START, current_lvt, current_lvt, 0, NULL);
	control_msg->rendezvous_mark = current_evt->rendezvous_mark;
	control_msg->mark = generate_mark(current_lp);

	// This message must be stored in the output queue as well, in case this LP rollbacks
	msg_hdr =  get_msg_hdr_from_slab();
	msg_to_hdr(msg_hdr, control_msg);
	list_insert(LPS(current_lp)->queue_out, send_time, msg_hdr);

	// Block the execution of this LP
	LPS(current_lp)->state = LP_STATE_WAIT_FOR_SYNCH;
	LPS(current_lp)->wait_on_object = fault_info.target_gid;

	// Store which LP we are waiting for synchronization.
	LPS(current_lp)->ECS_index++;
	LPS(current_lp)->ECS_synch_table[LPS(current_lp)->ECS_index] = target_gid;
	Send(control_msg);

	// Give back control to the simulation kernel's user-level thread
	long_jmp(&kernel_context, kernel_context.rax);
}

static ecs_writeback_t *writeback_init(void) {
	ecs_writeback_t *wb;
	wb = rsalloc(sizeof(ecs_writeback_t) + INITIAL_WRITEBACK_SLOTS * sizeof(writeback_page_t));
	wb->count = 0;
	wb->total = INITIAL_WRITEBACK_SLOTS;

	return wb;
}

ecs_prefetch_t * prefetch_init(void){
	ecs_prefetch_t *pfr;
	pfr = rsalloc(sizeof(ecs_prefetch_t) + INITIAL_WRITEBACK_SLOTS * sizeof(prefetch_page_t));
	pfr->count = 0;
	pfr->total = INITIAL_WRITEBACK_SLOTS;

	return pfr;
}

ecs_prefetch_t *add_prefetch_page(ecs_prefetch_t *pfr, long long *address, int span, int write_mode){
	int j;
	for(j = 0; j < span; j++){
		//printf("adding page %p\n", ((long long) address + j * PAGE_SIZE & (~((long long)PAGE_SIZE - 1))));
		pfr->pages[pfr->count].address = ((long long) address + j * PAGE_SIZE & (~((long long)PAGE_SIZE - 1)));
		pfr->pages[pfr->count].write_mode = write_mode;
		memcpy(pfr->pages[pfr->count].page, (void *)pfr->pages[pfr->count].address, PAGE_SIZE);
		pfr->count++;

		if(pfr->count == pfr->total) {                                                                                                                      
			pfr = rsrealloc(pfr, sizeof(ecs_prefetch_t) + 2 * pfr->total * sizeof(prefetch_page_t));
			pfr->total *= 2;
		}                                                                                                                                                       
	}

	return pfr; 
}

static ecs_writeback_t *add_writeback_page(ecs_writeback_t *wb, long long address) {
	wb->pages[wb->count].address = address;
	memcpy(wb->pages[wb->count].page, (void *)address, PAGE_SIZE);

	wb->count++;

	if(wb->count == wb->total) {
		wb = rsrealloc(wb, sizeof(ecs_writeback_t) + 2 * wb->total * sizeof(writeback_page_t));
		wb->total *= 2;
	}

	return wb;
}

// This handler is called to initiate an ECS, both in the local and in the distributed case
void ECS(void) __attribute__((__used__));
void ECS(void) {
	ecs_page_node_t *node;
	size_t pos;
	GID_t target_gid;
	// ECS cannot happen in silent execution, as we take a log after the completion
	// of an event which involves one or multiple ecs
	if(LPS(current_lp)->state == LP_STATE_SILENT_EXEC) {
		rootsim_error(true,"----ERROR---- ECS in Silent Execution LP[%d] Hit:%llu Timestamp:%f\n",
		current_lp, fault_info.target_gid, current_lvt);
	}

	// Sanity check: we cannot run an ECS with an old mark after a rollback
	if(LPS(current_lp)->wait_on_rendezvous != 0 && LPS(current_lp)->wait_on_rendezvous != current_evt->rendezvous_mark) {
		printf("muori male\n");
		fflush(stdout);
		abort();
	}

	// Kernel module gives us an unsigned long
	set_gid(target_gid, fault_info.target_gid);

//	printf("Entro nell'ECS handler per un fault di tipo %d\n", fault_info.fault_type);

	switch(fault_info.fault_type) {

		case ECS_MAJOR_FAULT:
			ecs_initiate();
			break;

		case ECS_MINOR_FAULT:
			ecs_secondary(target_gid);
			break;

		case ECS_CHANGE_PAGE_PRIVILEGE:
			//printf("target address : %p target gid %d lid %d ker %d me %d\n", fault_info.target_address, target_gid, GidToLid(target_gid), GidToKernel(target_gid), kid); fflush(stdout);
			node = find_page(fault_info.target_address, current_lp, &pos);
			if(node == NULL){
				node = add_page_node(fault_info.target_address, 1, current_lp);
			}
			set_write_mode(node, pos);
			ecs_secondary(target_gid); 
			lp_alloc_schedule(); // We moved to the original view in the kernel module: we do not unschedule the LP here
			break;

		default:
			rootsim_error(true, "%s:%d: Impossible condition! Aborting...\n", __FILE__,  __LINE__);
			return;
	}
}

void ecs_init(void) {
	printf("Invocation of ECS Init\n");
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
	sched_info.count = LPS(current_lp)->ECS_index + 1; // it's a counter
	sched_info.objects = (unsigned int*) LPS(current_lp)->ECS_synch_table; // pgd descriptor range from 0 to number threads - a subset of object ids

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
	if(GidToKernel(msg->sender) != kid) {
		//printf("Mi sincronizzo con un LP remoto e proteggo la memoria\n");
		bzero(&sched_info, sizeof(ioctl_info));
		sched_info.base_address = get_base_pointer(msg->sender);
		ioctl(ioctl_fd, IOCTL_PROTECT_REMOTE_LP, &sched_info);
	}
}

void lp_alloc_thread_fini(void) {
}

void ecs_send_pages(msg_t *msg) {
	msg_t *prefetch_control_msg;
	ecs_page_request_t *the_request;
	ecs_prefetch_t *pfr;
	ecs_prefetch_t *pfr_final;
	size_t pfr_size;
	void *sender_fault_address;

	the_request = (ecs_page_request_t *)&(msg->event_content);
	

	switch(the_request->prefetch_mode){
		case NO_PREFETCH:
			//printf("NO PREFETCH\n");
			break;
		case CLUSTERED:
			//printf("CLUSTERED\n");
			break;
		case SCATTERED:
			//printf("SCATTERED\n");
			break;
		default:
			rootsim_error(true," Unsupported prefetch mode: %d\n", the_request->prefetch_mode);

	}
	sender_fault_address = the_request->base_address;
	pfr = prefetch_init();
	pfr = compute_prefetch_data(recoverable_state[lid_to_int(GidToLid(msg->receiver))], msg->receiver, the_request->prefetch_mode, sender_fault_address, the_request->write_mode, pfr);
	pfr_final = (pfr->count == 0 )? NULL : pfr;
	pfr_size = (pfr->count == 0 )? 0 : sizeof(ecs_prefetch_t) + pfr->count * sizeof(prefetch_page_t);
	
	pack_msg(&prefetch_control_msg, msg->receiver, msg->sender, RENDEZVOUS_GET_PAGE_ACK, msg->timestamp, msg->timestamp, pfr_size, pfr_final);
	prefetch_control_msg->mark = generate_mark(GidToLid(msg->receiver));
	prefetch_control_msg->rendezvous_mark = msg->rendezvous_mark;
	Send(prefetch_control_msg);
	rsfree(pfr);
}

void reinstall_writeback_pages(msg_t *msg) {
	ecs_writeback_t *wb;
	void *dest;
	void *source;
	unsigned int i;
	if(msg->size == 0)
		return;

	wb = (ecs_writeback_t *)(msg->event_content);

	for(i = 0; i < wb->count; i++) {
		dest = (void *)(wb->pages[i].address);
		source = wb->pages[i].page;
		memcpy(dest, source, PAGE_SIZE);
	}
}

void reinstall_prefetch_pages(msg_t *msg){
	ecs_prefetch_t *pfr;
	void *dest;
	void *source;
	ioctl_info sched_info;
	unsigned int i;
	ecs_page_node_t *node;
	
	if(msg->size == 0) return;
	
	pfr = (ecs_prefetch_t *)(msg->event_content);
	
	for(i = 0; i < pfr->count; i++) {
		dest = (void *)(pfr->pages[i].address);
		source = pfr->pages[i].page;
		memcpy(dest, source, PAGE_SIZE);
		//printf("Completed the installation of the page copying %d bytes from address %p\n", PAGE_SIZE, (void *) (pfr->pages[i]).address);
		node = find_page(pfr->pages[i].address, GidToLid(msg->receiver), NULL);
		if(node == NULL) {
			node = add_page_node(pfr->pages[i].address, 1, GidToLid(msg->receiver));
		}

		//printf("address %p with count %d has mode %d and bit is %d\n", (void *) node->page_address, node->pages, pfr->pages[i].write_mode, bitmap_check(node->write_mode, 0));
		if(pfr->pages[i].write_mode){
			set_write_mode(node, 0);
		}
		else{ 
			bzero(&sched_info, sizeof(ioctl_info));
			sched_info.base_address = (void *)(pfr->pages[i].address);
			sched_info.page_count = 1;
			sched_info.write_mode = (pfr->pages[i].write_mode);
			ioctl(ioctl_fd, IOCTL_SET_PAGE_PRIVILEGE, &sched_info);
		}
	}


}
/*
void ecs_install_pages(msg_t *msg) {
	ecs_page_node_t *node;
	unsigned int i;
	ecs_page_request_t *the_pages = (ecs_page_request_t *)&(msg->event_content);
	ioctl_info sched_info;
	printf("LP %d receiving %d pages from %p from %d\n", msg->receiver, the_pages->count, the_pages->base_address, msg->sender);
	//fflush(stdout);
	
	memcpy(the_pages->base_address, the_pages->buffer, the_pages->count * PAGE_SIZE );
	printf("Completed the installation of the page copying %d bytes from address %p\n", the_pages->count * PAGE_SIZE, (void *) the_pages->base_address);
	fflush(stdout);
	
	node = find_page(the_pages->base_address, GidToLid(msg->receiver), NULL);
	if(node == NULL) {
		node = add_page_node(the_pages->base_address, the_pages->count, GidToLid(msg->receiver));
	}



	if(!the_pages->write_mode) {

		bzero(&sched_info, sizeof(ioctl_info));
		sched_info.base_address = the_pages->base_address;
		sched_info.page_count = the_pages->count;
		sched_info.write_mode = the_pages->write_mode;

		ioctl(ioctl_fd, IOCTL_SET_PAGE_PRIVILEGE, &sched_info);
	}else{
		for(i = 0; i < the_pages->count; i++)  {
			set_write_mode(node, i);
		}
	}
	
	//printf("Completato il setup dei privilegi\n");
	fflush(stdout);
}
*/
void unblock_synchronized_objects(LID_t lid) {
	//printf(" LP %d sending unblock\n", LidToGid(lid));
	fflush(stdout);
	unsigned int i;
	msg_t *control_msg;
	ecs_writeback_t *wb;
	ecs_writeback_t *wb_final;
	size_t wb_size;
	ecs_page_node_t *node;
	int page_faults_tot;
	int page_faults_clustered;
	int page_faults_scattered;

	#define add_page(x) ({ wb = add_writeback_page(wb, node->page_address + (x) * PAGE_SIZE); })

	for(i = 1; i <= LPS(lid)->ECS_index; i++) {
		wb = writeback_init();

		node = list_head(LPS(lid)->ECS_page_list);
		while(node != NULL){
			bitmap_foreach_set(node->write_mode, bitmap_required_size(node->pages), add_page);
			node = list_next(node);
		}

		wb_final = wb->count == 0 ? NULL : wb;
		wb_size = wb->count == 0 ? 0 : sizeof(ecs_writeback_t) + wb->count * sizeof(writeback_page_t);
		
		pack_msg(&control_msg, LidToGid(lid), LPS(lid)->ECS_synch_table[i], RENDEZVOUS_UNBLOCK, lvt(lid), lvt(lid), wb_size, wb_final);
		control_msg->rendezvous_mark = LPS(lid)->wait_on_rendezvous;
		Send(control_msg);

		rsfree(wb);
	}
	#undef add_page
	page_faults_tot = LPS(lid)->ECS_page_faults;
	page_faults_clustered= LPS(lid)->ECS_clustered_faults;
	page_faults_scattered = LPS(lid)->ECS_scattered_faults;
	statistics_post_lp_data(lid, STAT_ECS_FAULT, page_faults_tot == 0? page_faults_tot : page_faults_tot - 1);
	statistics_post_lp_data(lid, STAT_ECS_CLUSTERED, page_faults_clustered == 0? page_faults_clustered: page_faults_clustered- 1);
	statistics_post_lp_data(lid, STAT_ECS_SCATTERED, page_faults_scattered == 0? page_faults_scattered : page_faults_scattered - 1);
	
	LPS(lid)->wait_on_rendezvous = 0;
	LPS(lid)->ECS_index = 0;
}
#endif

