#include <asm/apic.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_pebs.h"
#include "ime_fops.h"
#include "irq_facility.h"

extern u64 start_value[MAX_ID_PMC];
u64 samples_pmc = 0;

static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr;
	u64 global; 
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, msr);
	if(global & BIT(62)){ 
		//tasklet_schedule(); //schedule di un nuovo tasklet
		// set lo stato a TASKLET_STATE_SCHED
		//count = 0 altrimenti il tasklet è disabilitato
		//DECLARE_TASKLET(name, func, data);
		//data: indirizzo del buffer
		write_buffer();
		wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(62));
		return 1;
	}
	if(global & 0xfULL){
		int i, k = 0;
		for(i = 0; i < MAX_ID_PMC; i++){
			if(global & BIT(i)){ 
				wrmsrl(MSR_IA32_PMC(i), start_value[i]);
				wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(i));
				samples_pmc++;
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
