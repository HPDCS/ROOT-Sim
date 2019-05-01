#include <asm/atomic.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>

#include "msr_config.h"
#include "../rootsim.h"

DEFINE_PER_CPU(unsigned, pcpu_state);

void on_context_switch(void)
{
	unsigned *state = this_cpu_ptr(&pcpu_state);
	u64 msr;

	/* enable */
	if (current->flags & BIT(24)) {
		if (*state) {
			wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0xFULL);
			*state = 0;
		}
		
		/* Set active thread bit */
		*state |= BIT(2);
	} else {
		/* Backup the first time an active thread is descheduled */
		if (*state & BIT(2)) {
			/* Save current MSR state */
			rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, msr);
			*state = !!msr;
			/* Cleanup active thread bit */
			*state &= ~BIT(2);
			/* Disable PMUs */
			wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0);
		}

	}
}

static int enable_thread_stack(pid_t pid)
{
	struct task_struct *tsk = get_pid_task(find_get_pid(pid), PIDTYPE_PID);

	if (!tsk) goto no_pid;
	/* Use an unused bit of the thread flags */
	tsk->flags |= BIT(24);
	return 0;
no_pid:
	return -1;
}

static void disable_thread_stack(pid_t pid)
{
	struct task_struct *tsk = get_pid_task(find_get_pid(pid), PIDTYPE_PID);

	if (!tsk) return;
	/* Use an unused bit of the thread flags */
	tsk->flags &= ~BIT(24);
}

int register_thread(pid_t pid)
{
	int cpu;
	// Check if the thread is already signed up
	if (enable_thread_stack(pid))
		goto no_patch;

	// if(current->pid == pid){
	// 	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0xfULL);
	// 	cpu = get_cpu();
	// 	iso_struct.cpus_pmc |= BIT_ULL(cpu);
	// 	put_cpu();
	// }
	return 0;

no_patch:
	return -1;
}// register_thread

int unregister_thread(pid_t pid)
{
	disable_thread_stack(pid);
	// if (tsk) {
	// 	pr_info("Before release, Got task_struct of pid %u\n", tsk->pid);
	// 	put_task_struct(tsk);
	// 	pr_info("After release, Got task_struct of pid %u\n", tsk->pid);
	// }
	return 0; 
}