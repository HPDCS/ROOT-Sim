#include <linux/module.h>
#include <linux/moduleparam.h>

#include "control.h"
#include "irq_facility.h"
#include "ime_device.h"
#include "ime_pebs.h"

static __init int hop_init(void)
{
	//check_for_pebs_support();
	cleanup_pmc();
	enable_nmi();
	setup_resources();
	init_pebs_struct();
	return 0;
}// hop_init

void __exit hop_exit(void)
{
	exit_pebs_struct();
	cleanup_resources();
	disable_nmi();
	cleanup_pmc();
}// hop_exit


module_init(hop_init);
module_exit(hop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci");
