#include <asm/apicdef.h> 	// Intel apic constants
#include <asm/desc.h>
#include <asm/apic.h>
#include <asm/nmi.h>
#include <linux/kprobes.h>

#include "msr_config.h"
#include "irq_facility.h"
#include "ime_handler.h"

unsigned long perf_events = 0;

//#define MAX_ID_CPU
static int switch_post_handler(struct kretprobe_instance *ri, struct pt_regs *regs);

static struct kretprobe krp = {
	.handler = switch_post_handler,
};

static void setup_ime_lvt(void *err)
{
	apic_write(APIC_LVTPC, BIT(10));
	return;
} // setup_ime_lvt

int setup_ime_nmi(int (*handler) (unsigned int, struct pt_regs*))
{
	int err = 0;
	static struct nmiaction handler_na;

	on_each_cpu(setup_ime_lvt, &err, 1);
	if (err) goto out;

	handler_na.handler = handler;
	handler_na.name = NMI_NAME;
	handler_na.flags = NMI_FLAG_FIRST;
	err = __register_nmi_handler(NMI_LOCAL, &handler_na);
out:
	return err;
}// setup_ime_nmi

static int switch_post_handler(struct kretprobe_instance *ri, struct pt_regs *regs){
	pr_info("In patch\n");
	++perf_events;
}

int switch_patcher_init (void)
{
	int ret;

	krp.kp.symbol_name = "intel_pmu_handle_irq";
	ret = register_kretprobe(&krp);
	if (ret < 0) {
		pr_info("hook init failed, returned %d\n", ret);
		goto no_probe;
	}
	return 0;

no_probe:
	return ret;
}// hook_init

void switch_patcher_exit(void)
{
	unregister_kretprobe(&krp);

	/* nmissed > 0 suggests that maxactive was set too low. */
	if (krp.nmissed) pr_info("Missed %u invocations\n", krp.nmissed);

	pr_info("hook module unloaded -- %llx\n", perf_events);
end:
	return;
}// hook_exit

void disable_nmi(void)
{
	unregister_nmi_handler(NMI_LOCAL, NMI_NAME);
	switch_patcher_exit();
}// cleanup_ime_nmi


int enable_nmi(void)
{
	if(switch_patcher_init()){
		pr_info("Patch failed\n");
	}
	setup_ime_nmi(handle_ime_nmi);
	unregister_nmi_handler(NMI_LOCAL, "perf_event_nmi_handler");
	return 0;
}// enable_pebs_on_system



void disableAllPMC(void* arg)
{
	int pmc_id; 
	preempt_disable();
	for(pmc_id = 0; pmc_id < MAX_ID_PMC; pmc_id++){
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	}
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	wrmsrl(MSR_IA32_PEBS_ENABLE, 0ULL);
	wrmsrl(MSR_IA32_FIXED_CTR_CTRL, 0ULL);
	preempt_enable();
}

void cleanup_pmc(void){
	on_each_cpu(disableAllPMC, NULL, 1);
}

void print_reg(void){
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

	rdmsrl(MSR_IA32_FIXED_CTR_CTRL, msr);
	pr_info("[CPU%d] MSR_IA32_FIXED_CTR_CTRL: %llx\n", smp_processor_id(), msr);

	/*for(i = 0; i < MAX_ID_PMC; i++){
		rdmsrl(MSR_IA32_PERFEVTSEL(i), msr);
		pr_info("[CPU%d] MSR_IA32_PERFEVTSEL%d: %llx\n", smp_processor_id(), i, msr);
	}*/
}