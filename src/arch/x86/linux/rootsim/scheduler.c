#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/printk.h>       // pr_info

#define switch_func "finish_task_switch"

typedef void h_func(void);

/*
 * Post-execution function hanlder: this calls the other subsystems in this
 * module, which require activation upon rescheduling.
 */
static int post_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	int ret = 0;

	// ret = ...

	return ret;
}// post_handler

static struct kretprobe krp = {
	.handler = post_handler,
	// This should be set by default to 2 * NR_CPUS
	// .maxactive
};

int scheduler_init(void)
{
	int ret;

	krp.kp.symbol_name = switch_func;
	ret = register_kretprobe(&krp);
	if (ret < 0) {
		pr_info(KBUILD_MODNAME ": failed to hook on the scheduler, returned %d\n", ret);
		return ret;
	}
	
	return 0;
}// hook_init

void scheduler_fini(void)
{
	unregister_kretprobe(&krp);

	/* nmissed > 0 suggests that maxactive was set too low. */
	if (krp.nmissed)
		pr_info(KBUILD_MODNAME ": Missed %u invocations\n", krp.nmissed);

}// hook_exit
