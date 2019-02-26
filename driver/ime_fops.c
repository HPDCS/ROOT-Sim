#include <linux/percpu.h>	/* Macro per_cpu */
#include <linux/ioctl.h>
// #include <asm/uaccess.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>	/* task_struct */
#include <linux/cpu.h>
#include <asm/cmpxchg.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/threads.h>
#include <asm/smp.h>

#include "ime_fops.h"
#include "../main/ime-ioctl.h"
#include "msr_config.h"
#include "intel_pmc_events.h"

DECLARE_BITMAP(pmc_bitmap, sizeof(MAX_PMC));

u64 user_events[MAX_NUM_EVENT] = {
    EVT_INSTRUCTIONS_RETIRED,			
    EVT_UNHALTED_CORE_CYCLES,		
    EVT_UNHALTED_REFERENCE_CYCLES, 		
    EVT_BR_INST_RETIRED_ALL_BRANCHES,	
    EVT_MEM_INST_RETIRED_ALL_LOADS,		
    EVT_MEM_INST_RETIRED_ALL_STORES,		
    EVT_MEM_LOAD_RETIRED_L3_HIT			
};


void debugPMU(void* arg)
{
	u64 pmu, msr;
	struct pmc_stats* args = (struct pmc_stats*) arg;
	pmu = MSR_IA32_PMC(args->pmc_id);
	preempt_disable();
	int cpu = smp_processor_id();
	rdmsrl(pmu, msr);
	pr_info("PMU%llx on CPU%d: %llx\n", pmu - 0xC1, cpu, msr);
	args->percpu_value[cpu] = msr;
	wrmsrl(pmu, ~(0xffULL));
	preempt_enable();
}

void disablePMC(void* arg)
{
	struct sampling_spec* args = (struct sampling_spec*) arg;
	int pmc_id = args->pmc_id; 
	preempt_disable();
	//ret = debugPMU(MSR_IA32_PMC(pmc_id));
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	pr_info("[CPU %u] disablePMC%d\n", smp_processor_id(), pmc_id);
	preempt_enable();
}

void enabledPMC(void* arg)
{
	int cpu = smp_processor_id();
	u64 msr;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	int pmc_id = args->pmc_id; 
	u64 event = user_events[args->event_id];
	preempt_disable();
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, BIT(pmc_id));
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), BIT(22) | BIT(20) | BIT(16) | event);
	wrmsrl(MSR_IA32_PERF_GLOBAL_OVF_CTRL, 1ULL << 62);
	wrmsrl(MSR_IA32_PMC(pmc_id), ~(0xffULL));
	pr_info("[CPU %u] enabledPMC%d\n", smp_processor_id(), pmc_id);
	preempt_enable();
}

long ime_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int err = 0;
    if(cmd == IME_PROFILER_ON || cmd == IME_PROFILER_OFF){
        int on = 0;
        struct sampling_spec* args;
        if(cmd == IME_PROFILER_ON) on = 1;

        args = (struct sampling_spec*) kzalloc (sizeof(struct sampling_spec), GFP_KERNEL);
        if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(struct sampling_spec));
		if(!err) goto out_pmc;
		err = copy_from_user(args, (void *)arg, sizeof(struct sampling_spec));
		if(err) goto out_pmc;
        if(on == 0){ 
			if(!test_bit(args->pmc_id, pmc_bitmap)) goto out_pmc;
			on_each_cpu(disablePMC, (void *) args, 1);
			clear_bit(args->pmc_id, pmc_bitmap);
		}
		else{
			if(test_bit(args->pmc_id, pmc_bitmap)) goto out_pmc;
			u64 msr;
			set_bit(args->pmc_id, pmc_bitmap);
			on_each_cpu(enabledPMC, (void *) args, 1);
			on_each_cpu(debugPMU, (void *) args, 1);
		}
		kfree(args);
		return 0;
	out_pmc:
		kfree(args);
		return -1;
    }

    if (cmd == IME_PMC_STATS){
		int i;
        struct pmc_stats* args = (struct pmc_stats*) kzalloc (sizeof(struct pmc_stats), GFP_KERNEL);
        if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(struct pmc_stats));
		if(!err) goto out_stat;
		err = copy_from_user(args, (void *)arg, sizeof(struct pmc_stats));
		if(err) goto out_stat;

		if(!test_bit(args->pmc_id, pmc_bitmap)) goto out_stat;

		on_each_cpu(debugPMU, (void *) args, 1);
		
		err = access_ok(VERIFY_WRITE, (void *)arg, sizeof(struct pmc_stats));
		if(!err) goto out_stat;
		err = copy_to_user((void *)arg, args, sizeof(struct pmc_stats));
		if(err) goto out_stat;

		kfree(args);
		return 0;
	out_stat:
		kfree(args);
		return -1;
    }
	return err;
}// ime_ctl_ioctl