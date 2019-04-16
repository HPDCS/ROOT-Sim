#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <asm/desc.h>
#include <asm/cpu_entry_area.h>
#include <asm/traps.h>

#include "rootsim.h"

extern void __my_page_fault(void);
typedef void (*do_page_fault_t)(struct pt_regs *, unsigned long);
static do_page_fault_t orig_pagefault = NULL;

static atomic_t synch_leave;
static atomic_t synch_enter;

static gate_desc *orig_idt;

static void all_cpu_patch_idt_slaves(void *n_idt) {
	struct desc_ptr idtr;
	gate_desc *new_idt = (gate_desc *)n_idt;

	preempt_disable();
	atomic_dec(&synch_enter);

	store_idt(&idtr);
	idtr.address = new_idt;
	load_idt(&idtr);

	while(atomic_read(&synch_leave) > 0);

	store_idt(&idtr);
	idtr.address = CPU_ENTRY_AREA_RO_IDT_VADDR;
	load_idt(&idtr);

	preempt_enable();
}

static void all_cpu_patch_idt(void *new_idt) {
	preempt_disable();
	atomic_set(&synch_enter, num_online_cpus() - 1);
	atomic_set(&synch_leave, 1);
	smp_call_function_many(cpu_online_mask, all_cpu_patch_idt_slaves, new_idt, false);
	while(atomic_read(&synch_enter) > 0);
}


static void switch_back_to_regular_idt(void) {
	atomic_set(&synch_leave, 0);
	preempt_enable();
}

static gate_desc *idt_clone(void)
{
	struct desc_ptr idtr;
	gate_desc *idt_table = __get_free_page(GFP_KERNEL);

	store_idt(&idtr);

	memcpy(idt_table, (void *)(idtr.address), idtr.size + 1);

	return idt_table;
}


static void install_hooks(gate_desc *idt)
{
	gate_desc new_gate;

	orig_pagefault = (do_page_fault_t)kallsyms_lookup_name("do_page_fault");

	pack_gate(&new_gate, GATE_INTERRUPT, (unsigned long)__my_page_fault, 0, 0, 0);
	write_idt_entry(idt, X86_TRAP_PF, &new_gate);
}


static void patch_idt(void)
{
	struct desc_ptr idtr;
	gate_desc *new_idt;

	orig_idt = idt_clone();
	new_idt = idt_clone();

	// Set local IDTR
	store_idt(&idtr);
	idtr.address = new_idt;
	load_idt(&idtr);

	all_cpu_patch_idt(new_idt);

	install_hooks(new_idt);

	// Sovrascrivi la IDT nella cpu_entry_area
	unprotect_memory();
	memcpy(CPU_ENTRY_AREA_RO_IDT_VADDR, new_idt, idtr.size+1);
	protect_memory();

	store_idt(&idtr);
	idtr.address = CPU_ENTRY_AREA_RO_IDT_VADDR;
	load_idt(&idtr);

	// Set IDTR locale
	switch_back_to_regular_idt();

	free_page(new_idt);
}

int setup_idt(void)
{
	// FIXME: we need a set of callbacks here
	//patch_idt();

	return 0;
}

void restore_idt(void)
{
	free_page(orig_idt);
}
