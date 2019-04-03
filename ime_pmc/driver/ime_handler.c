#include <asm/apic.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_pebs.h"
#include "ime_fops.h"
#include "irq_facility.h"

extern u64 reset_value_pmc[MAX_ID_PMC];
u64 samples_pmc = 0;
u64 index = 0;
u64 pmc = 0;
u64 pebs = 0;
int once = 0;
unsigned long abc = 0;
static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr;
	u64 global; 
	int i;
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	if(global & BIT(62)){ 
		wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(62));
		/*write_buffer();
		if(!once){
			pr_info("\n\n\n\n");
			print_reg();
			pr_info("\n\n\n\n");
			++once;
		}
		for(i = 0; i < MAX_ID_PMC; i++){
			rdmsrl(MSR_IA32_PMC(i), msr);
			if(msr < (~reset_value_pmc[i])){
				wrmsrl(MSR_IA32_PMC(i), ~reset_value_pmc[i]);
			}
		}*/
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, ~(0ULL));
		return 1;
	}
	/*if(global & 0x70000000fULL){
		int k = 0;
		if(global & 0x700000000ULL){
			wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, 0x700000000ULL);
			k++;
		}
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
	++abc;
	pr_info("\n\n\n\n NOT CATCHED\n");
	print_reg();
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	print_reg();
	pr_info("\n\n\n\n");*/
	return 0;
}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ime_nmi
