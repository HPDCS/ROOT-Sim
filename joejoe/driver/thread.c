#include <asm/atomic.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>

#include "thread.h"
#include "patcher.h"
#include "core.h"


// static inline void put_task_struct(struct task_struct *t)
// {
// 	if (atomic_dec_and_test(&t->usage))
// 		__put_task_struct(t);
// }




static int patch_thread_stack(pid_t pid)
{
	/* This will be the value of the last entry stack. It allows for O(1) lookup */
	// u64 magic_entry;
	struct thread_data *t_data;
	struct task_struct *tsk = get_pid_task(find_get_pid(pid), PIDTYPE_PID);

	if (!tsk)
		goto no_pid;

	pr_info("Struct allocated at %lx\n", (unsigned long)tsk);


	t_data = vmalloc(sizeof(struct thread_data));
	if (!t_data)
		goto no_tdata;

	/**
	 * Remember that the stack grows down, so the last entry is pointed by the tsk->stack.
	 * If CONFIG_STACK_GROWSUP is defined, the stack grows up, so the last entry is
	 * pointed by (tsk->stack + THREAD_SIZE) - (sizeof(unsigned long)).
	 * If CONFIG_THREAD_INFO_IN_TASK is defined, end_of_stack macro points to the entry
	 * containing the magic number for corruption detection.
	 */

	/* Points to the last writable entry, however that entry contains a magic number used
	 * to detect stack overflow. We write the 2nd to last entry
	 */
	tsk->flags |= BIT(24);
	return 0;

no_tdata:
	// TODO This must be moved at the remove of the thread, we also need a way to delete struct related to dead thread
	put_task_struct(tsk);
no_pid:
	return -1;

}

int register_thread(pid_t pid)
{
	int cpu;
	// Check if the thread is already signed up
	if (patch_thread_stack(pid))
		goto no_patch;

	if(current->pid == pid){
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0xfULL);
		pr_info("Enable PMC\n");
		cpu = get_cpu();
		iso_struct.cpus_pmc |= BIT_ULL(cpu);
		put_cpu();
	}
	return 0;

no_patch:
	return -1;

	// /* the last kstack position contains the pt buffer addr */
	// kstack = ((unsigned long) new_pt) & PTR_USED_MASK;
	// /* set the crc */
	// kstack |= ((kstack & PTR_CCB_MASK) ^ CRC_MAGIC) << 48;
	// /* clear processing bit */
	// kstack &= ~PROCESS_MASK;
	// /* set enable bit */
	// kstack |= ENABLED_MASK;
	// /* visible to threads_monitor and NMI */
	// *new_pt->ctl = kstack;

}// register_thread

int unregister_thread(pid_t pid)
{
	// if (tsk) {
	// 	pr_info("Before release, Got task_struct of pid %u\n", tsk->pid);
	// 	put_task_struct(tsk);
	// 	pr_info("After release, Got task_struct of pid %u\n", tsk->pid);
	// }
	return 0; 
}