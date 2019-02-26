#include <asm/apicdef.h> 	// Intel apic constants
#include <asm/desc.h>
#include <linux/kallsyms.h>
#include <linux/smp.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>		/* current macro */
#include <linux/percpu.h>		/* this_cpu_ptr */
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <asm/apic.h>
#include <asm/nmi.h>

#include "msr_config.h"
#include "irq_facility.h"
#include "intel_pmc_events.h"
#include "ime_fops.h"
#include "ime_handler.h"

#define MSR_IBS_CONTROL					0xc001103a
#define 	IBS_LVT_OFFSET_VAL				(1ULL<<8)	
#define 	IBS_LVT_OFFSET					0xfULL 	

unsigned ime_vector = 0xffU;

static void debugPMU(u64 pmu)
{
	u64 msr;
	rdmsrl(pmu, msr);
	pr_info("PMU %llx: %llx\n", pmu, msr);
}// debugPMU


/**
 * On my machine system_irqs is a global symbol but it is not exported (EXPORT_SYMBOL).
 * This is a problem for the system whne trying to mount the module because it cannot find
 * the symbol and ends up aborting the operation. To bypass this flaw, we use the 
 * kallsyms_lookup_name utility function which does the grunt work in place of us.
 */
static long unsigned int *system_irqs;

static unsigned irq_vector = 0;

static DEFINE_PER_CPU(u64, lvtpc_bkp);
static gate_desc entry_bkp;

extern unsigned long audit_counter;

extern void pebs_entry(void);

static void setup_ime_lvt(void *err)
{
	u64 reg;
	u64 ime_ctl;
	u32 entry;
	u32 new_entry;
	u8 offset;

	/*
	 * The LAPIC entry is set by the BIOS and reserves the 
	 * offset specified by the IBS_CTL register for 
	 * Non Maskable Interrupt (NMI). This entry should be 
	 * masked.
	 */
	/* Get the IBS_LAPIC offset by IBS_CTL */
	/*rdmsrl(MSR_IBS_CONTROL, ime_ctl);
	if (!(ime_ctl & IBS_LVT_OFFSET_VAL)) {
		pr_err("APIC setup failed: invalid offset by MSR_bits: %llu\n", ime_ctl);
		goto no_offset;
	}

	offset = ime_ctl & IBS_LVT_OFFSET;
	pr_info("IBS_CTL Offset: %u\n", offset);

	reg = APIC_LVTPC;
	entry = apic_read(reg);

	/* print the apic register : | mask | msg_type | vector | before setup */
	/*pr_info("[APIC] CPU %u - READ offset %u -> | %lu | %lu | %lu |\n", 
		smp_processor_id(), 
		offset, ((entry >> 16) & 0xFUL), ((entry >> 8) & 0xFUL), (entry & 0xFFUL));

	// if different, clear

	// This is the entry we want to install, ime_irq_line has been got in the step before
	new_entry = (0UL) | (APIC_EILVT_MSG_NMI << 8) | (0);

	// If not masked, remove it
	if (entry != new_entry || !((entry >> 16) & 0xFUL) ) {
		if (!setup_APIC_eilvt(offset, 0, 0, 1)) {
			pr_info("Cleared LVT entry #%i on cpu:%i\n", 
				offset, smp_processor_id());
			reg = APIC_EILVTn(offset);
			entry = apic_read(reg);
		} else {
			goto fail;
		}
	}

	if (!setup_APIC_eilvt(offset, 0, APIC_EILVT_MSG_NMI, 0))
		pr_info("LVT entry #%i setup on cpu:%i\n", 
			offset, smp_processor_id());
	else{
		goto fail;
	}*/

	preempt_disable();
	for (offset = 0; offset < APIC_EILVT_NR_MAX; offset++) {
		if (setup_APIC_eilvt(offset, 0, APIC_EILVT_MSG_NMI, 0)) {
			pr_info("LVT entry #%i setup on cpu:%i\n", 
			offset, smp_processor_id());
			break;
		}
	}
	preempt_enable();
	return;

fail:
	pr_err("APIC setup failed: cannot set up the LVT entry \
		#%u on CPU: %u\n", offset, smp_processor_id());
no_offset:
	*(int*)err = -1;
} // setup_ime_lvt

int setup_ime_nmi(int (*handler) (unsigned int, struct pt_regs*))
{
	int err = 0;
	static struct nmiaction handler_na;

	on_each_cpu(setup_ime_lvt, &err, 1);
	if (err) goto out;

	// register_nmi_handler(NMI_LOCAL, handler, NMI_FLAG_FIRST, NMI_NAME);
	handler_na.handler = handler;
	handler_na.name = NMI_NAME;
	/* this is a Local CPU NMI, thus must be processed before others */
	handler_na.flags = NMI_FLAG_FIRST;
	err = __register_nmi_handler(NMI_LOCAL, &handler_na);

out:
	return err;
}// setup_ime_nmi

void cleanup_ime_nmi(void)
{
	unregister_nmi_handler(NMI_LOCAL, NMI_NAME);
}// cleanup_ime_nmi


int enable_pebs_on_system(void)
{
	pr_info("Enable pebs\n");
	setup_ime_nmi(handle_ime_nmi);
	return 0;
}// enable_pebs_on_systemz



void disable_pebs_on_system(void)
{
	cleanup_ime_nmi();
}// disable_pebs_on_system



void disableAllPMC(void* arg)
{
	int pmc_id; 
	preempt_disable();
	for(pmc_id = 0; pmc_id < MAX_ID_PMC; pmc_id++){
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
		pr_info("[CPU %u] disablePMC%d\n", smp_processor_id(), pmc_id);
	}
	preempt_enable();
}

void cleanup_pmc(void){
	pr_info("audit_counter: %lu\n", audit_counter);
	on_each_cpu(disableAllPMC, NULL, 1);
}

void disablePMC0(void* arg)
{
	preempt_disable();
	//ret = debugPMU(MSR_IA32_PMC(pmc_id));
	if(smp_processor_id() == 3){
		int pmc_id = 0;
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
		pr_info("[CPU %u] disablePMC%d\n", smp_processor_id(), pmc_id);
		preempt_enable();
	}
}

void enablePMC0(void* arg)
{
	if(smp_processor_id() == 3){
		int pmc_id = 0;
		u64 msr;
		preempt_disable();
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, BIT(pmc_id));
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), BIT(22) | BIT(20) | BIT(16) | 0xC0);
		wrmsrl(MSR_IA32_PERF_GLOBAL_OVF_CTRL, 1ULL << 62);
		wrmsrl(MSR_IA32_PMC(pmc_id), ~(0xffULL));
		pr_info("[CPU %u] enabledPMC%d\n", smp_processor_id(), pmc_id);
		rdmsrl(MSR_IA32_PMC(pmc_id), msr);
		pr_info("PMU%llx: %llx\n", MSR_IA32_PMC(pmc_id) - 0xC1, msr);
		preempt_enable();
	}
}