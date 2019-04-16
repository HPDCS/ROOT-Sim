#include <asm/cpufeature.h>
#include <linux/kallsyms.h>

int check_for_pebs_support(void)
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

	/* Debug Store (DS) support */
	cpuid(0x1, &a, &b, &c, &d);

	//  MSR IA32_PERF_CAPABILITIES: BIT(15)
	pr_info("[CPUID_01H] ds: %u, perf capability: %u\n", !!(d & BIT(21)), !!(c & BIT(15)));


	// see Vol. 4 2-23
	rdmsrl(MSR_IA32_PERF_CAPABILITIES, msr);
	pr_info("[MSR_IA32_PERF_CAPABILITIES] PEBS record format: %u",
		"PEBS SaveArchRegs, %u, PEBS Trap %u\n", 
		(msr >> 8) & 0xF, !!(msr & BIT(7)), !!(msr & BIT(6)));


	// cpuid_eax(0x8000001B);

	return 0;
}
