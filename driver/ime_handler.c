#include <asm/apic.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_pebs.h"
#include "ime_fops.h"
#include "irq_facility.h"

extern u64 start_value;

static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr, msr1;
	u64 global; 
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, msr);
	if(global & BIT(62)){ 
		print_reg();
		//prinf_pebs();	
		write_buffer();
		wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(62));
		return 1;
	}
	if(global & 0xfULL){
		int i, k;
		for(i = 0; i < MAX_ID_PMC; i++){
			if(global & BIT(i)){ 
				wrmsrl(MSR_IA32_PMC(i), start_value);
				wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(i));
				k++;
			}
		}
		return k;
	}
	return 0;
}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ime_nmi
