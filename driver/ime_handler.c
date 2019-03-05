#include <asm/apic.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_pebs.h"


static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr; 
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, msr);
	if(msr & BIT(62)){ 
		rdmsrl(MSR_IA32_PERFEVTSEL(0), msr);
		wrmsrl(MSR_IA32_PERFEVTSEL(0), 0ULL);
		write_buffer();
		wrmsrl(MSR_IA32_PERFEVTSEL(0), msr);
		wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(0));
		return 1;
	}
	return 0;
}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ime_nmi
