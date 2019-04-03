#include <linux/module.h>
#include <linux/moduleparam.h>

#include "control.h"
#include "irq_facility.h"
#include "ime_device.h"
#include "ime_pebs.h"
#include "ime_fops.h"

extern unsigned long abc;

static __init int hop_init(void)
{
	//check_for_pebs_support();
	cleanup_pmc();
	enable_nmi();
	setup_resources();
	init_pebs_struct();
	on_each_cpu(set_mitigation, NULL, 1);
	return 0;
}// hop_init

void __exit hop_exit(void)
{
	on_each_cpu(clear_mitigation, NULL, 1);
	exit_pebs_struct();
	cleanup_resources();
	disable_nmi();
	cleanup_pmc();
	pr_info("Not catched events: %llx\n", abc);
}// hop_exit


module_init(hop_init);
module_exit(hop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci");
