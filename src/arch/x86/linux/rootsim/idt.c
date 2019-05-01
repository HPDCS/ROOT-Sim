#include <linux/slab.h>
#include <asm/desc.h>

#include "rootsim.h"

static struct desc_ptr bkp_idtr = {0, 0};

static void smp_load_idt(void *addr)
{
	struct desc_ptr *idtr;
	preempt_disable();
	idtr = (struct desc_ptr *) addr;
	load_idt(idtr);
	pr_info("[CPU %u]: loaded IDT at %lx\n", smp_processor_id(), idtr->address);
	preempt_enable();
}// smp_load_idt

gate_desc *clone_current_idt(void)
{
	struct desc_ptr idtr;
	void *idt_table = (void *)__get_free_page(GFP_KERNEL);

	if (!idt_table) return 0;

	store_idt(&idtr);

	memcpy(idt_table, (void *)(idtr.address), PAGE_SIZE);

	return (gate_desc *) idt_table;
}// clone_current_idt


static inline int _patch_system_idt(struct desc_ptr idtr)
{
	if (bkp_idtr.size > 0) {
		pr_warn("IDT already backed up\n");
	} else {
		pr_info("IDT backup created\n");
		store_idt(&bkp_idtr);
	}

	on_each_cpu(smp_load_idt, &idtr, 1);
	return 0;
}// patch_system_idt

int patch_system_idt(gate_desc *idt, unsigned size)
{
	struct desc_ptr idtr = {size, (unsigned long) idt};
	return _patch_system_idt(idtr);
}

int install_hook(gate_desc *idt, unsigned long handler, 
	unsigned vector, unsigned dpl)
{
	gate_desc new_gate;
	if ((vector >> 8) || dpl > 3) {
		pr_info("Invalid vector %x or dpl %x\n", vector, dpl);
	}

	pack_gate(&new_gate, GATE_INTERRUPT, handler, dpl, 0, 0);
	write_idt_entry(idt, vector, &new_gate);

	return 0;
}// install_hook

void restore_system_idt(void)
{
	if (bkp_idtr.size > 0) {
		on_each_cpu(smp_load_idt, &bkp_idtr, 1);
		pr_info("original IDT restored\n");
	} else {
		pr_warn("IDT backup not found\n");
	}
}// restore_system_idt