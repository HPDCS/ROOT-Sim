#include <linux/preempt.h>
#include <linux/slab.h>
#include <asm/cpufeature.h>
#include "msr_config.h"
#include "pmu.h"

// static DEFINE_PER_CPU(struct debug_store, pcpu_ds_bkp);

DEFINE_PER_CPU(struct debug_store, pcpu_ds);
DEFINE_PER_CPU(struct pmc_cfg *, pcpu_pmc_cfg);

DEFINE_PER_CPU(u32, pcpu_lvt_bkp);

static unsigned nr_pmcs = 0;

unsigned available_pmcs(void)
{
	return nr_pmcs;
}

int check_pmu_support(void)
{
	unsigned a, b, c, d;
	u64 msr;

	struct cpuinfo_x86 *cpu_info = &boot_cpu_data;

	if (cpu_info->x86_vendor != X86_VENDOR_INTEL) {
		pr_err("Required Intel processor.\n");
		return -EINVAL;
	}

	pr_info("Found Intel processor: %u|%u\n", cpu_info->x86, cpu_info->x86_model);

	/* Architectural performance monitoring version ID */
	cpuid(0xA, &a, &b, &c, &d);

	pr_info("[CPUID_0AH] version: %u, counters: %u\n", a & 0xFF, (a >> 8) & 0XFF);
	if ((a & 0xFF) < 3 || ((a >> 8) & 0XFF) < 2)
		return -1;
	
	/* set the available pmcs*/
	nr_pmcs = ((a >> 8) & 0XFF);

	pr_info("[CPUID_0AH] PMC bit width: %u\n", (a >> 16) & 0XFF);

	/* Debug Store (DS) support */
	cpuid(0x1, &a, &b, &c, &d);

	//  MSR IA32_PERF_CAPABILITIES: BIT(15)
	pr_info("[CPUID_01H] ds: %u, perf capability: %u\n", !!(d & BIT(21)), !!(c & BIT(15)));
	if (!(d & BIT(21)) || !(c & BIT(15)))
		return -1;

	// see Vol. 4 2-23
	rdmsrl(MSR_IA32_PERF_CAPABILITIES, msr);
	pr_info("[MSR_IA32_PERF_CAPABILITIES] PEBS record format: %llu \
		PEBS SaveArchRegs, %u, PEBS Trap %u\n", 
		(msr >> 8) & 0xF, !!(msr & BIT(7)), !!(msr & BIT(6)));


	// cpuid_eax(0x8000001B);

	return 0;
}

struct pmc_cfg *get_pmc_config(unsigned pmc_id, unsigned cpu)
{
	/* Incorrect PMC ID */
	if (pmc_id < 0 || pmc_id >= NR_PMCS || 
		cpu < 0 || cpu >= num_present_cpus())
		return 0;

	return &(per_cpu(pcpu_pmc_cfg, cpu)[pmc_id]); // TODO CHECK
}

/*
 * We have to configure PEBS before initializing the PMCs beacuse
 * we need to use the DS area to correctly configure the PMC config
 */
static void pmc_init_on_cpu(void *dummy)
{
	per_cpu(pcpu_lvt_bkp, smp_processor_id()) = apic_read(APIC_LVTPC); 
	apic_write(APIC_LVTPC, 241);
}

int pmc_init(void)
{
	unsigned cpu;
	/* Init per-cpu pmc config structs */
	for_each_present_cpu(cpu) {
		per_cpu(pcpu_pmc_cfg, cpu) = kzalloc(sizeof(struct pmc_cfg) * NR_PMCS, GFP_KERNEL);
		// TODO check mem error and free
	}

	on_each_cpu(pmc_init_on_cpu, NULL, 1);
	return 0;
}

static void pmc_fini_on_cpu(void *dummy)
{
	/* Restore LVT entry */
	// apic_write(APIC_LVTPC, per_cpu(pcpu_lvt_bkp, smp_processor_id()));
}

void pmc_fini(void)
{
	unsigned cpu;
	disable_all_pmc_system();
	on_each_cpu(pmc_fini_on_cpu, NULL, 1);
	
	/* Free per-cpu pmc config structs */
	for_each_present_cpu(cpu) {
		kfree(per_cpu(pcpu_pmc_cfg, cpu));
	}
}

static void pebs_init_on_cpu(void *dummy)
{

	u64 msr;
	struct debug_store *ds = per_cpu_ptr(&pcpu_ds, get_cpu());

	/* Setup the Debug Store area */
	wrmsrl(MSR_IA32_DS_AREA, (u64) ds);

	rdmsrl(MSR_IA32_DS_AREA, msr);
	pr_info("DS Store at %llx\n", msr);

	// ds = (struct debug_store *) msr;

	// pr_info("DS[%u] pebs_buffer_base %llx\n", smp_processor_id(), ds->pebs_buffer_base);
	// pr_info("DS[%u] pebs_index %llx\n", smp_processor_id(), ds->pebs_index);
	// pr_info("DS[%u] pebs_absolute_maximum %llx\n", smp_processor_id(), ds->pebs_absolute_maximum);
	// pr_info("DS[%u] pebs_counter0_reset %llx\n", smp_processor_id(), ds->pebs_counter0_reset);

	/* Enable for all available PMCs */
	// wrmsrl(MSR_IA32_PEBS_ENABLE, ~(1 << NR_PMCS));
	/* Enable PMI overhead mitigation */
	// wrmsrl(MSR_IA32_DEBUGCTL, BIT(12));

	put_cpu();
}

static void pebs_fini_on_cpu(void *dummy)
{
	/* Must restore the BKP - Not implemented yet */
	u64 msr;
	struct debug_store *ds;

	rdmsrl(MSR_IA32_DS_AREA, msr);
	
	/* Restore Debug Store area */
	wrmsrl(MSR_IA32_DS_AREA, 0ULL);
	/* Disable for all available PMCs */
	wrmsrl(MSR_IA32_PEBS_ENABLE, 0ULL);
	/* Disable PMI overhead mitigation */
	// rdmsrl(MSR_IA32_DEBUGCTL, msr);
	// wrmsrl(MSR_IA32_DEBUGCTL, msr & ~BIT(12));
	ds = (struct debug_store *) msr;
	pr_info("DS[%u] at %llx\n", smp_processor_id(),  msr);

	pr_info("DSf[%u] pebs_buffer_base %llx\n", smp_processor_id(), ds->pebs_buffer_base);
	pr_info("DSf[%u] pebs_index %llx\n", smp_processor_id(), ds->pebs_index);
	pr_info("DSf[%u] pebs_absolute_maximum %llx\n", smp_processor_id(), ds->pebs_absolute_maximum);
	pr_info("DSf[%u] pebs_counter0_reset %llx\n", smp_processor_id(), ds->pebs_counter0_reset);
}

int pebs_init(void)
{
	int err = 0;
	unsigned cpu;
	unsigned offset;
	unsigned threshold;
	struct debug_store *ds;
	size_t pebs_buf_len;
	void *pebs_buf;

	for_each_present_cpu(cpu) {
		ds = per_cpu_ptr(&pcpu_ds, cpu);

		/* 16 Pages */
		pebs_buf_len = PAGE_SIZE * 16;

		/* Maximum number of samples */
		offset = (pebs_buf_len / sizeof(struct pebs_sample)) * sizeof(struct pebs_sample);
		/* Fire interrupt when there is space for only 16 samples */
		threshold = offset - (sizeof(struct pebs_sample) * 16);
		/* Allocate the maximum possible memory */
		pebs_buf = kmalloc(pebs_buf_len, GFP_KERNEL);

		if (!pebs_buf) goto no_mem;

		/* BTS data */
		ds->bts_buffer_base 		= 0; 
		ds->bts_index 			= 0;
		ds->bts_absolute_maximum 	= 0;
		ds->bts_interrupt_threshold 	= 0;
		/* PEBS data */
		ds->pebs_buffer_base 		= (u64) pebs_buf;
		ds->pebs_index 			= ds->pebs_buffer_base;
		ds->pebs_absolute_maximum	= ds->pebs_buffer_base + offset;
		ds->pebs_interrupt_threshold	= ds->pebs_buffer_base + threshold;
		/* PMC reset value */
		ds->pebs_counter0_reset		= 0;
		ds->pebs_counter1_reset		= 0;
		ds->pebs_counter2_reset		= 0;
		ds->pebs_counter3_reset		= 0;
		/* Unused bits */
		ds->reserved			= 0;
	}

	/* Configure each CPU */
	on_each_cpu(pebs_init_on_cpu, NULL, 1);
	
	goto out;

no_mem:
	/* free allocated memory */
	while(--cpu > -1) { 
		kfree((void *)(per_cpu_ptr(&pcpu_ds, cpu)->pebs_buffer_base));
	}
	pr_err("Cannot allocate memory for PEBS buffers. Stop at cpu %x\n", cpu);
	err = -ENOMEM;
out:
	return err;
}

void pebs_fini(void)
{
	unsigned cpu;

	for_each_present_cpu(cpu) {
		kfree((void *)(per_cpu_ptr(&pcpu_ds, cpu)->pebs_buffer_base));
	}

	on_each_cpu(pebs_fini_on_cpu, NULL, 1);
}

static inline void sync_pmu_state(unsigned pmc_id, struct pmc_cfg *cfg)
{
	u64 msr;
	struct debug_store *ds = per_cpu_ptr(&pcpu_ds, get_cpu());
	u64 *c_reset = &(ds->pebs_counter0_reset);

	pr_info("DS[%u] at %llx\n", smp_processor_id(),  ds);

	/* Enable PEBS */
	if (cfg->pebs) {
		rdmsrl(MSR_IA32_PEBS_ENABLE, msr);
		wrmsrl(MSR_IA32_PEBS_ENABLE, msr | BIT(pmc_id));

		/* Setup the reset value for the pmc*/
		c_reset[pmc_id] = cfg->reset;

		/* Setup the Debug Store area */
		wrmsrl(MSR_IA32_DS_AREA, (u64) ds);

		pr_info("DS pebs_buffer_base %llx\n", ds->pebs_buffer_base);
		pr_info("DS pebs_index %llx\n", ds->pebs_index);
		pr_info("DS pebs_absolute_maximum %llx\n", ds->pebs_absolute_maximum);
		pr_info("DS pebs_counter0_reset %llx\n", ds->pebs_counter0_reset);
		// pr_info("DS pebs_counter1_reset %llx\n", ds->pebs_counter1_reset);
		// pr_info("DS pebs_counter2_reset %llx\n", ds->pebs_counter2_reset);
		// pr_info("DS pebs_counter3_reset %llx\n", ds->pebs_counter3_reset);

		rdmsrl(MSR_IA32_DS_AREA, msr);
		ds = (struct debug_store *) msr;

		pr_info("DS[%u] pebs_buffer_base %llx\n", smp_processor_id(), ds->pebs_buffer_base);
		pr_info("DS[%u] pebs_index %llx\n", smp_processor_id(), ds->pebs_index);
		pr_info("DS[%u] pebs_absolute_maximum %llx\n", smp_processor_id(), ds->pebs_absolute_maximum);
		pr_info("DS[%u] pebs_counter0_reset %llx\n", smp_processor_id(), ds->pebs_counter0_reset);
	}

	/* Write PMC event selector (control msr) */
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), cfg->perf_evt_sel);
	/* Write PMC counter (control msr) */
	wrmsrl(MSR_IA32_PMC(pmc_id), cfg->counter);
}

static void smp_sync_pmu_state(void *dummy)
{
	/* Preemption must be disabled */
	unsigned i;
	struct pmc_cfg *cfg = __this_cpu_read(pcpu_pmc_cfg);
	for (i = 0; i <  NR_PMCS; ++i, ++cfg) {
		sync_pmu_state(i, cfg);
	}
}

void sync_system_pmu_state(void)
{
	on_each_cpu(smp_sync_pmu_state, NULL, 1);
}

void enable_all_pmc(void)
{
	preempt_disable();
	smp_sync_pmu_state(NULL);
	preempt_enable();
}

static void smp_enable_core_pmc(void *d)
{
	enable_all_pmc();
}

void enable_all_pmc_system(void)
{
	// Must wait?
	on_each_cpu(smp_enable_core_pmc, NULL, 1);
}


/* Must be called with preemption disabled */
inline void disable_pmc (unsigned pmc_id)
{
	wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
	wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
}

void disable_all_pmc(void)
{
	unsigned pmc_id = 0;

	preempt_disable();
	while(pmc_id < NR_PMCS) disable_pmc(pmc_id++);
	preempt_enable();
}

/* Must be called with preemption disabled */
void reset_core_msr_pmc(void)
{
	u64 msr;
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	wrmsrl(MSR_IA32_PEBS_ENABLE, 0ULL);
	wrmsrl(MSR_IA32_FIXED_CTR_CTRL, 0ULL);
	/* Ack the system of whatever is pending */
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, msr);
	wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, msr);
}

static void smp_disable_core_pmc(void *d)
{
	disable_all_pmc();
	reset_core_msr_pmc();
}

void disable_all_pmc_system(void)
{
	// Must wait?
	on_each_cpu(smp_disable_core_pmc, NULL, 1);
}