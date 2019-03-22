#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/printk.h>       // pr_info

#include "core.h"
#include "patcher.h"
#include "thread.h"

static u64 hook_func = 0;
static int system_hooked = 0;

static int switch_post_handler(struct kretprobe_instance *ri, struct pt_regs *regs);

static struct kretprobe krp = {
	.handler = switch_post_handler,
};

/*
 * Pre-execution function hanlder: this executes before the probed function
 */
// static int pre_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
// {
// 	return 0;
// }// pre_handler

/*
 * Post-execution function hanlder: this perfoms the function whose address
 * is contained in the hook_func variable.
 */
static int switch_post_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	h_func *hook;
	unsigned int cpu = get_cpu(); // disable preemption 

	if (!(iso_struct.state && iso_struct.cpus_state & BIT_ULL(cpu) && is_current_enabled))
		goto off;

	if(!(iso_struct.cpus_pmc & BIT_ULL(cpu))){
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0xfULL);
		iso_struct.cpus_pmc |= BIT_ULL(cpu);
	}
	//  else {
	// 	pr_warning("[PID %u] enabled: %lx\n", current->pid, *magic_entry);
	// }
	
	/*if (!hook_func) goto end;

	hook = (h_func*) hook_func;
	// Execute the function
	hook(regs);*/
	goto end;
off:
	if(iso_struct.cpus_pmc & BIT_ULL(cpu)){
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		iso_struct.cpus_pmc &= ~(BIT_ULL(cpu));
	}
end:
	//put_cpu(); /* enable preemption */
	return 0;
}// post_handler



int switch_patcher_init (unsigned long func_addr)
{
	int ret;

	krp.kp.symbol_name = SWITCH_FUNC;
	ret = register_kretprobe(&krp);
	if (ret < 0) {
		pr_info("hook init failed, returned %d\n", ret);
		goto no_probe;
	}
	
	/* assign custom function */
	hook_func = func_addr;

	pr_info("hook module correctly loaded\n");
	system_hooked = 1;
	
	return 0;

no_probe:
	return ret;
}// hook_init

void switch_patcher_exit(void)
{
	if (!system_hooked) {
		pr_info("The system is not hooked\n");
		goto end;
	} 

	unregister_kretprobe(&krp);

	/* nmissed > 0 suggests that maxactive was set too low. */
	if (krp.nmissed) pr_info("Missed %u invocations\n", krp.nmissed);

	pr_info("hook module unloaded\n");
end:
	return;
}// hook_exit

int switch_pacther_state(void)
{
	return system_hooked;
}