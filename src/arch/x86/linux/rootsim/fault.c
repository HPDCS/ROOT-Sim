#include <linux/module.h>
#include <asm/traps.h>
#include <asm-generic/errno.h>
#include <asm/desc.h>
#include <linux/ftrace.h>

#include "rootsim.h"
#include "ioctl.h"

typedef void (*do_page_fault_t)(struct pt_regs *, unsigned long);

static gate_desc fault_desc;
static do_page_fault_t orig_pagefault = NULL;

// TODO: remove, this is used only to check if this is working as expected
unsigned long audit_counter = 0;
module_param(audit_counter, ulong, S_IRUSR | S_IRGRP | S_IROTH);

__attribute__ ((used))
void rootsim_page_fault(struct pt_regs *regs, unsigned long err)
{
	// We immediately make a copy of CR2, to avoid calling any kind
	// of machinery before observing CR2.
	// If we fail to do so, there could be some situations in which
	// the original handler might see a clobbered value.
	unsigned long address = read_cr2();

	audit_counter++;

	// Put back the original CR2 and call the original handler.
	write_cr2(address);
	orig_pagefault(regs, err);
}

/*
void ____rootsim_page_fault(struct pt_regs *regs, long error_code, do_page_fault_t kernel_handler)
{
 	void *target_address;
	void **my_pgd;
	void **my_pdp;
	ulong i;
	ulong *auxiliary_stack_pointer;
	ioctl_info info;

	if(current->mm == NULL) {
		// this is a kernel thread - not a rootsim thread
		kernel_handler(regs, error_code);
		return;
	}

	// discriminate whether this is a classical fault or a root-sim proper fault
	for(i = 0; i < SIBLING_PGD; i++) {
		if (root_sim_processes[i] == current->pid) {

			target_address = (void *)read_cr2();

			if(PML4(target_address) < restore_pml4 || PML4(target_address) >= restore_pml4 + restore_pml4_entries) {
				// a fault outside the root-sim object zone - it needs to be handeld by the traditional fault manager
				kernel_handler(regs, error_code);
				return;
			}

			// Compute the address of the PDP entry associated with the faulting address. It's content
			// will discriminate whether this is a first invocation of ECS or not for the running event.
			my_pgd =(void **)pgd_addr[i];
			my_pdp =(void *)my_pgd[PML4(target_address)];
			my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);

			// Post information about the fault. We fill this structure which is used by the
			// userland handler to keep up with the ECS (distributed) protocol. We fill as much
			// information as possible that we can get from kernel space.
			// TODO: COPY TO USER
			root_sim_fault_info[i]->rcx = regs->cx;
			root_sim_fault_info[i]->rip = regs->ip;
			root_sim_fault_info[i]->target_address = (long long)target_address;
			root_sim_fault_info[i]->target_gid = (PML4(target_address) - restore_pml4) * 512 + PDP(target_address);

			if((ulong)my_pdp[PDP(target_address)] == NULL) {
				printk("ECS Major Fault at %p\n", target_address);
				// First activation of ECS targeting a new LP
				root_sim_fault_info[i]->fault_type = ECS_MAJOR_FAULT;
			} else {
				if(get_pte_sticky_bit(target_address) != 0) {
					printk("ECS Minor Fault (1) at %p\n", target_address);
				// First activation of ECS targeting a new LP
				    secondary:
					// ECS secondary fault on a remote page
					root_sim_fault_info[i]->fault_type = ECS_MINOR_FAULT;
					set_presence_bit(target_address);
				} else {
					if(get_presence_bit(target_address) == 0) {
						printk("Materializing page for %p\n", target_address);
						kernel_handler(regs, error_code);
						if(get_pde_sticky_bit(target_address)) {
							printk("ECS Minor Fault (2) at %p\n", target_address);
							set_single_pte_sticky_flag(target_address);
							goto secondary;
						}
						return;
					} else {
						root_sim_fault_info[i]->fault_type = ECS_CHANGE_PAGE_PRIVILEGE;

						info.base_address = (void *)((long long)target_address & (~(PAGE_SIZE-1)));
						info.page_count = 1;
						info.write_mode = 1;

						set_single_page_privilege(&info);

						// we're on the parallel view here
						rootsim_load_cr3(pgd_addr[i]);
					}
				}
			}

			printk("Activating userspace handler\n");

			rs_ktblmgr_ioctl(NULL, IOCTL_UNSCHEDULE_ON_PGD, (int)i);

			// Pass the address of the faulting instruction to userland
			auxiliary_stack_pointer = (ulong *)regs->sp;
			auxiliary_stack_pointer--;
			copy_to_user((void *)auxiliary_stack_pointer, (void *)&regs->ip, 8);	
			regs->sp = (long)auxiliary_stack_pointer;
			regs->ip = callback;

			return;
		}
	}

	kernel_handler(regs, error_code);
}
*/

static atomic_t synch_leave;
static atomic_t synch_enter;
static unsigned long flags;

static void synchronize_all_slaves(void *info)
{
	(void)info;

	printk(KERN_DEBUG "%s: cpu %d entering synchronize_all_slaves\n", KBUILD_MODNAME, smp_processor_id());

	atomic_dec(&synch_enter);
	preempt_disable();

	while(atomic_read(&synch_leave) > 0);

	preempt_enable();
	printk(KERN_DEBUG "%s: cpu %d leaving synchronize_all_slaves\n", KBUILD_MODNAME, smp_processor_id());
}

void synchronize_all(void)
{

	printk("cpu %d asking from unpreemptive synchronization\n", smp_processor_id());
	atomic_set(&synch_enter, num_online_cpus() - 1);
	atomic_set(&synch_leave, 1);

	local_irq_save(flags);
	preempt_disable();
	smp_call_function_many(cpu_online_mask, synchronize_all_slaves, NULL, false);

	while(atomic_read(&synch_enter) > 0);

	printk("cpu %d all kernel threads synchronized\n", smp_processor_id());
}

void unsynchronize_all(void)
{
	printk("cpu %d freeing other kernel threads\n", smp_processor_id());

	atomic_set(&synch_leave, 0);
	preempt_enable();
	local_irq_restore(flags);
}

int setup_idt(void)
{
	struct desc_ptr idtr;
	gate_desc new_fault_desc;
	int i;

	// Get the address of do_page_fault()
	orig_pagefault = (do_page_fault_t)kallsyms_lookup_name("do_page_fault");
	if(!orig_pagefault) {
		pr_info(KBUILD_MODNAME ": Kernel compiled without CONFIG_KALLSYMS, unable to mount\n");
		return -ENOPKG;
	}
	printk("do_page_fault found at %lx\n", orig_pagefault);


	// read the idtr register
	store_idt(&idtr);

	// copy the old entry before overwritting it
	memcpy(&fault_desc, (void*)(idtr.address + X86_TRAP_PF * sizeof(gate_desc)), sizeof(gate_desc));
	
	pack_gate(&new_fault_desc, GATE_INTERRUPT, (unsigned long)fault_handler, 0, 0, 0);
	
	// the IDT is read only
	synchronize_all();
	for(i = 0; i < 1000000; i++);

	unprotect_memory();

	write_idt_entry((gate_desc*)idtr.address, X86_TRAP_PF, &new_fault_desc);
	
	// restore the Write Protection bit
	protect_memory();
	unsynchronize_all();

	return 0;
}


void restore_idt(void)
{
	struct desc_ptr idtr;

	// read the idtr register
	store_idt(&idtr);

	// the IDT is read only
	synchronize_all();
	unprotect_memory();

	write_idt_entry((gate_desc*)idtr.address, X86_TRAP_PF, &fault_desc);
	
	// restore the Write Protection bit
	protect_memory();
	unsynchronize_all();
}


