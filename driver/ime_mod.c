#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kprobes.h>

#include "control.h"
#include "irq_facility.h"
#include "ime_device.h"

unsigned long audit_counter = 0;
module_param(audit_counter, ulong, S_IRUSR | S_IRGRP | S_IROTH);

extern long unsigned int system_vectors[];

static __init int hop_init(void)
{
	u64 msr;
	int err = 0;
	pr_info("Module Init\n");
	check_for_pebs_support();
	if(enable_pebs_on_system()) err = -1;
	//on_each_cpu(enablePMC0, NULL, 1);
	setup_resources();
	//enable_on_apic();
	return 0;
}// hop_init

void __exit hop_exit(void)
{
	disable_pebs_on_system();
	cleanup_resources();
	//cleanup_pmc();
	//on_each_cpu(disablePMC0, NULL, 1);
	//disable_on_apic();
	pr_info("Module Exit -- %lu\n", audit_counter);
}// hop_exit

// Register these functions
module_init(hop_init);
module_exit(hop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
