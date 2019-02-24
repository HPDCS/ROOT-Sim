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
#include <linux/hashtable.h>

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

static u64 debugPMU(u64 pmu)
{
	u64 msr;
	rdmsrl(pmu, msr);
	pr_info("PMU %llx: %llx\n", pmu - 0xC1, msr);
	return msr;
}

u64 disablePMC(int pmc_id)
{
	u64 ret;
	preempt_disable();
	ret = debugPMU(MSR_IA32_PMC(pmc_id));
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	pr_info("[CPU %u] disablePMC%d\n", smp_processor_id(), pmc_id);
	preempt_enable();
	return ret;
}

int enabledPMC(int pmc_id, u64 event)
{
	preempt_disable();
	//debugPMU(MSR_IA32_PERF_GLOBAL_CTRL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, BIT(pmc_id));
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), BIT(22) | BIT(16) | event);
	pr_info("[CPU %u] enabledPMC%d\n", smp_processor_id(), pmc_id);
	preempt_enable();
	return 0;
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
			args->value = disablePMC(args->pmc_id);
			clear_bit(args->pmc_id, pmc_bitmap);
			err = access_ok(VERIFY_WRITE, (void *)arg, sizeof(struct sampling_spec));
			if(!err) goto out_pmc;
			err = copy_to_user((void *)arg, args, sizeof(struct sampling_spec));
			if(err) goto out_pmc;
		}
		else{
			if(test_bit(args->pmc_id, pmc_bitmap)) goto out_pmc;
			set_bit(args->pmc_id, pmc_bitmap);
			enabledPMC(args->pmc_id, user_events[args->event_id]);
		}
		kfree(args);
		return 0;
	out_pmc:
		kfree(args);
		return -1;
    }

    if (cmd == IME_PMC_STATS){
        struct pmc_stats* args = (struct pmc_stats*) kzalloc (sizeof(struct pmc_stats), GFP_KERNEL);
        if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(struct pmc_stats));
		if(!err) goto out_stat;
		err = copy_from_user(args, (void *)arg, sizeof(struct pmc_stats));
		if(err) goto out_stat;

		if(!test_bit(args->pmc_id, pmc_bitmap)) goto out_stat;
		
		args->value = debugPMU(MSR_IA32_PMC(args->pmc_id));
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