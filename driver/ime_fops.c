#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/smp.h>

#include "ime_fops.h"
#include "../main/ime-ioctl.h"
#include "msr_config.h"
#include "intel_pmc_events.h"
#include "ime_pebs.h"
#include "irq_facility.h"

extern struct pebs_user* buffer_sample;
extern int user_index_written;
u64 pmc_mask = 0;
u64 start_value;

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

void set_mitigation(void* arg){
	wrmsrl(MSR_IA32_IA32_DEBUGCTL, BIT(12));
}

void clear_mitigation(void* arg){
	wrmsrl(MSR_IA32_IA32_DEBUGCTL, 0ULL);
}

void debugPMU(void* arg)
{
	u64 pmu, msr;
	int cpu = smp_processor_id();
	struct pmc_stats* args = (struct pmc_stats*) arg;
	pmu = MSR_IA32_PMC(args->pmc_id);
	preempt_disable();
	rdmsrl(pmu, msr);
	args->percpu_value[cpu] = msr;
	preempt_enable();
	//print_reg();
}

void disablePMC(void* arg)
{
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->cpu_id[smp_processor_id()] == 0) return;
	int pmc_id = args->pmc_id; 
	preempt_disable();
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	preempt_enable();
}

void enabledPMC(void* arg)
{
	u64 msr, msr1;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->cpu_id[smp_processor_id()] == 0) return;
	if(!args->enable_PEBS[smp_processor_id()]) pmc_mask |= BIT(20);
	if(args->user[smp_processor_id()]) pmc_mask |= BIT(16);
	if(args->kernel[smp_processor_id()]) pmc_mask |= BIT(17);
	int pmc_id = args->pmc_id; 
	u64 event = user_events[args->event_id];
	preempt_disable();
	rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, msr);
	wrmsrl(MSR_IA32_PMC(pmc_id), ~(args->start_value));
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, msr | BIT(pmc_id));
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), BIT(22) | pmc_mask | event);
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
			on_each_cpu(pebs_exit, (void *) args, 1);
		}
		else{
			int k;
			int cpu_mask;
			if(test_bit(args->pmc_id, pmc_bitmap)) goto out_pmc;
			set_bit(args->pmc_id, pmc_bitmap);
			start_value = ~(args->start_value);
			on_each_cpu(pebs_init, (void *)args, 1);
			on_each_cpu(enabledPMC, (void *) args, 1);
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

	if(cmd == IME_READ_BUFFER){
		int k;
		struct buffer_struct* args = (struct buffer_struct*) kzalloc (sizeof(struct buffer_struct), GFP_KERNEL);
        if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(struct buffer_struct));
		if(!err) goto out_read;
		err = copy_from_user(args, (void *)arg, sizeof(struct buffer_struct));
		if(err) goto out_read;
		
		for(k = 0; k < MAX_BUFFER_SIZE && k < user_index_written; k++){
			memcpy(&(args->buffer_sample[k]), &(buffer_sample[k]), sizeof(struct pebs_user));
		}

		args->last_index = user_index_written;
		if(user_index_written > MAX_BUFFER_SIZE)args->last_index = MAX_BUFFER_SIZE;

		err = access_ok(VERIFY_WRITE, (void *)arg, sizeof(struct buffer_struct));
		if(!err) goto out_read;
		err = copy_to_user((void *)arg, args, sizeof(struct buffer_struct));
		if(err) goto out_read;

		kfree(args);
		return 0;
	out_read:
		kfree(args);
		return -1;
	}

	if(cmd == IME_RESET_BUFFER){
		preempt_disable();
		user_index_written = 0;
		preempt_enable();
	}
	return err;
}// ime_ctl_ioctl