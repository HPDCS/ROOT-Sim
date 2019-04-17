#include <asm/apic.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_pebs.h"
#include "ime_fops.h"
#include "irq_facility.h"

extern u64 reset_value_pmc[MAX_ID_PMC];
u64 samples_pmc = 0;

static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr;
	u64 global; 
	int i;
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	if(global & BIT(62)){ 
		wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(62));
		write_buffer();

		for(i = 0; i < MAX_ID_PMC; i++){
			rdmsrl(MSR_IA32_PMC(i), msr);
			if(msr < (~reset_value_pmc[i])){
				wrmsrl(MSR_IA32_PMC(i), ~reset_value_pmc[i]);
			}
		}
		return 1;
	}
	if(global & 0xfULL){
		int k = 0;
		for(i = 0; i < MAX_ID_PMC; i++){
			if(global & BIT(i)){ 
				if(reset_value_pmc[i] != -1) wrmsrl(MSR_IA32_PMC(i), ~reset_value_pmc[i]);
				else wrmsrl(MSR_IA32_PMC(i), 0ULL);
				wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(i));
				samples_pmc++;
				k++;
			}
			else{
				rdmsrl(MSR_IA32_PMC(i), msr);
				if(msr < (~reset_value_pmc[i])){
					wrmsrl(MSR_IA32_PMC(i), ~reset_value_pmc[i]);
				}
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
