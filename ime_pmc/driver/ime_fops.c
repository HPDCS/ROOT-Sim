#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/smp.h>
#include <linux/vmalloc.h>

#include "ime_fops.h"
#include "../main/ime-ioctl.h"
#include "msr_config.h"
#include "intel_pmc_events.h"
#include "ime_pebs.h"
#include "irq_facility.h"

extern struct pebs_user* buffer_sample;
extern int write_index;
extern int read_index;
extern unsigned long write_cycle;
extern unsigned long read_cycle;
extern int nRecords_module;
extern u64 samples_pmc;
extern u64 collected_samples;
u64 reset_value_pmc[MAX_ID_PMC];
u64 size_buffer_samples;

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
	wrmsrl(MSR_IA32_DEBUGCTL, 0ULL);
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
}

void clear_mitigation(void* arg){
	preempt_disable();
	wrmsrl(MSR_IA32_DEBUGCTL, 0ULL);
	preempt_enable();
}

void enablePMC(void* arg){
	preempt_disable();
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0xfULL);
	preempt_enable();
}

void disablePMC(void* arg){
	preempt_disable();
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	empty_pebs_buffer();
	preempt_enable();
}

void debugPMC(void* arg){
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

void resetPMC(void* arg){
	int pmc_id;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->cpu_id[smp_processor_id()] == 0) return;
	pmc_id = args->pmc_id; 
	preempt_disable();
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	preempt_enable();
}

void setupPMC(void* arg){
	int pmc_id;
	u64 msr, pmc_mask;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->cpu_id[smp_processor_id()] == 0) return;
	pmc_id = args->pmc_id;
	pmc_mask = 0ULL;
	if(!args->enable_PEBS[smp_processor_id()]) pmc_mask |= BIT(20);
	if(args->user[smp_processor_id()]) pmc_mask |= BIT(16);
	if(args->kernel[smp_processor_id()]) pmc_mask |= BIT(17); 
	u64 event = user_events[args->event_id];
	preempt_disable();
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	if(args->start_value != -1) wrmsrl(MSR_IA32_PMC(pmc_id), ~(args->start_value));
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), BIT(22) | pmc_mask | event);
	rdmsrl(MSR_IA32_PERFEVTSEL(pmc_id), msr);
	//pr_info("[CPU%d] MSR_IA32_PERFEVTSEL(%d): %llx\n", smp_processor_id(), pmc_id, msr);
	preempt_enable();
	
}

long ime_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    int err = 0;
	if(cmd == IME_PROFILER_ON) on_each_cpu(enablePMC, NULL, 1);
	if(cmd == IME_PROFILER_OFF) on_each_cpu(disablePMC, NULL, 1);
	if(cmd == IME_SIZE_BUFFER){
		unsigned long* args = (unsigned long*)kzalloc (sizeof(unsigned long), GFP_KERNEL);
		if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(unsigned long));
		if(!err) goto out_size;
		err = copy_from_user(args, (void *)arg, sizeof(unsigned long));
		if(err) goto out_size;

		*args = retrieve_buffer_size();
		size_buffer_samples = *args;

		err = access_ok(VERIFY_WRITE, (void *)arg, sizeof(unsigned long));
		if(!err) goto out_size;
		err = copy_to_user((void *)arg, args, sizeof(unsigned long));
		if(err) goto out_size;

		kfree(args);
		return 0;
	out_size:
		kfree(args);
		return -1;
	}
    if(cmd == IME_SETUP_PMC || cmd == IME_RESET_PMC){
        int on = 0;
        struct sampling_spec* args;
        if(cmd == IME_SETUP_PMC) on = 1;

        args = (struct sampling_spec*) kzalloc (sizeof(struct sampling_spec), GFP_KERNEL);
        if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(struct sampling_spec));
		if(!err) goto out_pmc;
		err = copy_from_user(args, (void *)arg, sizeof(struct sampling_spec));
		if(err) goto out_pmc;
        if(on == 0){ 
			if(!test_bit(args->pmc_id, pmc_bitmap)) goto out_pmc;
			on_each_cpu(resetPMC, (void *) args, 1);
			clear_bit(args->pmc_id, pmc_bitmap);
			on_each_cpu(pebs_exit, (void *) args, 1);
		}
		else{
			if(test_bit(args->pmc_id, pmc_bitmap)) goto out_pmc;
			set_bit(args->pmc_id, pmc_bitmap);
			reset_value_pmc[args->pmc_id] = args->reset_value;
			on_each_cpu(pebs_init, (void *)args, 1);
			on_each_cpu(setupPMC, (void *) args, 1);
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

		on_each_cpu(debugPMC, (void *) args, 1);
		
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
		int k = 0, current_read, current_write;
		unsigned long current_wcycle, current_rcycle;
		pr_info("Size buffer: %llx\n", size_buffer_samples);
		struct buffer_struct* args = (struct buffer_struct*) vmalloc (sizeof(struct buffer_struct)*size_buffer_samples);
        if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(struct buffer_struct)*size_buffer_samples);
		if(!err) goto out_read;
		err = copy_from_user(args, (void *)arg, sizeof(struct buffer_struct)*size_buffer_samples);
		if(err) goto out_read;

		/*current_read = read_index;
		current_write = write_index;
		current_rcycle = read_cycle;
		current_wcycle = write_cycle;
		pr_info("samples: %llx\n", collected_samples*327);
		for(; !(current_read == current_write && current_wcycle == current_rcycle) && k < args->last_index;){
			int new_index;
			memcpy(&(args->buffer_sample[k]), &(buffer_sample[current_read]), sizeof(struct pebs_user));
			new_index = (current_read+1)%nRecords_module;
			if(new_index < current_read) current_rcycle++;
			current_read = new_index;
			k++;
		}*/
		read_buffer_samples(args);
		reset_hashtable();

		err = access_ok(VERIFY_WRITE, (void *)arg, sizeof(struct buffer_struct)*size_buffer_samples);
		if(!err) goto out_read;
		err = copy_to_user((void *)arg, args, sizeof(struct buffer_struct)*size_buffer_samples);
		if(err) goto out_read;

		/*preempt_disable();
		write_index = 0;
		read_index = 0;
		write_cycle = 0;
		read_cycle = 0;
		preempt_enable();*/

		vfree(args);
		return 0;
	out_read:
		vfree(args);
		return -1;
	}

	if(cmd == IME_RESET_BUFFER){
		preempt_disable();
		write_index = 0;
		read_index = 0;
		write_cycle = 0;
		read_cycle = 0;
		preempt_enable();
	}
	return err;
}// ime_ctl_ioctl
