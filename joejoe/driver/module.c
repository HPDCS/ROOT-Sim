#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kprobes.h>

#include <linux/sched.h>
#include <asm/current.h>

#include "core.h"
#include "core-structs.h"
#include "patcher.h"
#include "thread.h"
#include "device.h"

unsigned long long audit = 0;

char* enable = "";
module_param(enable, charp, 0660);

char* disable = "";
module_param(disable, charp, 0660);

unsigned long hook_func_enable;
unsigned long hook_func_disable;
// Call it in a preempt-safe context
void count(void *dummy)
{
	/* We assume we are always with preemption disabled here*/
	// if (global_core_state & BIT(smp_processor_id())) {
	// 	struct core_dev *dev = this_cpu_ptr(pcpu_core_dev);
	// 	dev->counter ++;
	// 	// ++counter;
	// }
	pr_info("[PID %u] last_stack: %lx, struck_ptr: %lx\n", current->pid, (unsigned long)magic_entry, (unsigned long)current_ptr);

	pr_info("[%u] audit %llu\n", smp_processor_id(), ++audit);
}// count

// static int setup_scheduler_probe(void)
// {
// 	int err;

// 	err = patcher_init(count);

// 	return err;
// }// setup_scheduler_probe

// static int uninstall_scheduler_probe(void)
// {
// 	patcher_exit();
// }// setup_scheduler_probe


unsigned pid = 16486;

// static inline unsigned long *end_of_stack(struct task_struct *p)
// {
// #ifdef CONFIG_STACK_GROWSUP
// 	pr_info("CONFIG_STACK_GROWSUP ON\n");
// 	return (unsigned long *)((unsigned long)task_thread_info(p) + THREAD_SIZE) - 1;
// #else
// 	pr_info("CONFIG_STACK_GROWSUP OFF\n");
// 	return (unsigned long *)(task_thread_info(p) + 1);
// #endif
// }

void print_reg(void* arg){
	u64 msr;
	int i;

	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, msr);
	pr_info("[CPU%d] MSR_IA32_PERF_GLOBAL_STATUS_RESET: %llx\n", smp_processor_id(), msr);

	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, msr);
	pr_info("[CPU%d] MSR_IA32_PERF_GLOBAL_STATUS: %llx\n", smp_processor_id(), msr);

	rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, msr);
	pr_info("[CPU%d] MSR_IA32_PERF_GLOBAL_CTRL: %llx\n", smp_processor_id(), msr);

	rdmsrl(MSR_IA32_PEBS_ENABLE, msr);
	pr_info("[CPU%d] MSR_IA32_PEBS_ENABLE: %llx\n", smp_processor_id(), msr);

	rdmsrl(MSR_IA32_DS_AREA, msr);
	pr_info("[CPU%d] MSR_IA32_DS_AREA: %llx\n", smp_processor_id(), msr);

	for(i = 0; i < 4; i++){
		rdmsrl(MSR_IA32_PERFEVTSEL(i), msr);
		pr_info("[CPU%d] MSR_IA32_PERFEVTSEL%d: %llx\n", smp_processor_id(), i, msr);
	}
}


static __init int mod_init(void)
{

	int err = 0;
	
	kstrtoul(enable, 16, &hook_func_enable);
	kstrtoul(disable, 16, &hook_func_disable);

	err = setup_devices();
	if (err) {
		goto no_dev;
	}

	switch_patcher_init((unsigned long)count);

	// register_thread(1425);

	// set_cores(BIT(2) | BIT(0));

	// pr_info("Current pid %u, tgid %u\n", current->pid, current->tgid);

	// pr_info("Current stack at address   0x%llx\n", current->stack);
	// pr_info("Current stack last address 0x%llx\n", current->stack + (THREAD_SIZE - 1));

	// *(char*)(current->stack + (THREAD_SIZE)) = 0xa;
	// pr_info("Current stack at last position 0x%llx\n", *(char*)(current->stack + (THREAD_SIZE - 1)));

	// pr_info("Stack size: %d\n", THREAD_SIZE);

	// register_thread(pid);

	// TODO REMOVE
	// goto out;

// 	err = check_for_ibs_support();
// 	if (err) goto out;

// #ifdef _IRQ
// 	pr_info("IRQ mode\n");
// 	err = setup_ibs_irq(handle_ibs_irq);
// 	if (err) goto out;
// #else
// 	pr_info("NMI mode\n");
// 	err = setup_ibs_nmi(handle_ibs_nmi);
// 	if (err) goto out;
// #endif

// 	err = core_init();
// 	if (err) goto no_core;

// 	err = setup_scheduler_probe();
// 	if (err) goto no_probe;

// 	goto out;

// no_probe:
// 	core_exit();
// no_core:
// #ifdef _IRQ
// 	cleanup_ibs_irq();
// #else
// 	cleanup_ibs_nmi();
// #endif
	return 0;

no_dev:
	return err;
}// hop_init

void __exit mod_exit(void)
{
	cleanup_devices();

	switch_patcher_exit();
	// struct core_dev *dev;
	// int i;

	// unregister_thread(pid);

	// kprobe
	// uninstall_scheduler_probe();

	// for (i = 0; i < 8; ++i) {
	// 	dev = per_cpu_ptr(pcpu_core_dev, i);
	// 	pr_info("Debug cpu[%u]: %llu\n", i, dev->counter);
	// }



// 	cleanup_resources();

// #ifdef _IRQ
// 	cleanup_ibs_irq();
// #else
// 	cleanup_ibs_nmi();
// #endif
	pr_info("Module Exit\n");
}// hop_exit

// Register these functions
module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
