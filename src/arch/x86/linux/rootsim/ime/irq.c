#include <linux/types.h>
#include <linux/smp.h>
#include <asm/apic.h>
#include <asm/msr.h>
#include "msr_config.h"
#include "irq.h"
#include "pmu.h"
#include "../rootsim.h" /* idt features */

static inline void read_samples(struct debug_store *ds)
{
	unsigned offset;
	struct mem_data *md;
	unsigned i = offsetof(struct pebs_sample, add) / sizeof(u64);
	// unsigned i;
	// unsigned max = (ds->pebs_index - ds->pebs_buffer_base) / sizeof(struct pebs_sample);
	/* Smaples Base */
	u64 *base = (u64 *) ds->pebs_buffer_base;
	// struct pebs_sample *ps = (struct pebs_sample *) ds->pebs_buffer_base;
	
	offset = (ds->pebs_index - ds->pebs_buffer_base) / sizeof(u64);
	// pr_info("Offset %u\n", offset);

	md = this_cpu_ptr(&pcpu_mem_data);

	// pr_info("Start at: ii %u - oo %u\n", i, offset);

	for (; i < offset; i += (sizeof(struct pebs_sample) / sizeof(u64))) {
	// for (i = 0; i < max; ++i) {
		if (md->pos >= md->nr_buf) {
			pr_info("No available buffer: %u\n", md->pos);
			return;
		}
		md->buf_poll[md->pos][md->index] = base[i];
		if (++(md->index) >= md->buf_size) {
			md->pos++;
			md->index = 0;
		}
	}
	pr_info("Samples copied: p %u - i %u - s%u\n", md->pos, md->index, md->buf_size);
}

/* Performance Monitor Interrupt handler */
void pmi_handler(void)
{
	u64 msr;
	u64 global; 
	struct debug_store *ds;

	// Does it make sense?
	preempt_disable();

	rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, global);
	pr_info("GLOBAL CTRL: %llx\n", global);

	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	pr_info("GLOBAL STATUS: %llx\n", global);

	if(global & BIT_ULL(62)){ 

		pr_info("PEBS\n");
		// for(i = 0; i < 4; i++){
		// 	rdmsrl(MSR_IA32_PMC(i), msr);
		// 	if(msr < (~0x10000)){
		// 		wrmsrl(MSR_IA32_PMC(i), ~0x10000);
		// 	}
		// }
		// ++k;

		rdmsrl(MSR_IA32_DS_AREA, msr);
		ds = (struct debug_store *) msr;

		// pr_info("DS[%u] pebs_buffer_base %llx\n", smp_processor_id(), ds->pebs_buffer_base);
		// pr_info("DS[%u] pebs_index %llx\n", smp_processor_id(), ds->pebs_index);
		// pr_info("DS[%u] pebs_absolute_maximum %llx\n", smp_processor_id(), ds->pebs_absolute_maximum);
		// pr_info("DS[%u] pebs_counter0_reset %llx\n", smp_processor_id(), ds->pebs_counter0_reset);
		
		read_samples(ds);

		ds->pebs_index = ds->pebs_buffer_base;

		// rdmsrl(MSR_IA32_DS_AREA, msr);
		// ds = (struct debug_store *) msr;

		// pr_info("DS pebs_buffer_base %llx\n", ds->pebs_buffer_base);
		// pr_info("DS pebs_index %llx\n", ds->pebs_index);
		// pr_info("DS pebs_absolute_maximum %llx\n", ds->pebs_absolute_maximum);
		// pr_info("DS pebs_counter0_reset %llx\n", ds->pebs_counter0_reset);
	}
	// if(global & 0xFULL){
	// 	pr_info("PMC \n");
	// 	for(i = 0; i < 4; i++){
	// 		if(global & BIT(i)){ 
	// 			if(0x10000 != -1) wrmsrl(MSR_IA32_PMC(i), ~0x10000);
	// 			else wrmsrl(MSR_IA32_PMC(i), 0ULL);
	// 			// wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(i));
	// 			++k;
	// 		}
	// 		else{
	// 			rdmsrl(MSR_IA32_PMC(i), msr);
	// 			if(msr < (~0x10000)){
	// 				// wrmsrl(MSR_IA32_PMC(i), ~reset_value_pmc[i]);
	// 			}
	// 		}
	// 	}
		
	// }

	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	pr_info("GLOBAL STATUS: %llx\n", global);

	wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, global);

	/* ack apic */
	apic_eoi();
	/* Unmask PMI as, as it got implicitely masked. */
	apic_write(APIC_LVTPC, 241);

	pr_info("PMI done on cpu %x\n", get_cpu());
	put_cpu();
	preempt_enable();
}

// extern void pmu_on_handler(void);
asm("	.globl pmu_toggle_entry\n"
	"pmu_toggle_entry:\n"
	"	wrmsr\n"
	"	iretq");


// extern void pmi_handler(void);
asm("	.globl pmi_entry\n"
	"pmi_entry:\n"
	"	cld\n"
	"	testq $3,8(%rsp)\n"
	"	jz 1f\n"
	"	swapgs\n"
	"1:\n"
	"	pushq $0\n" /* error code */
	"	pushq %rdi\n"
	"	pushq %rsi\n"
	"	pushq %rdx\n"
	"	pushq %rcx\n"
	"	pushq %rax\n"
	"	pushq %r8\n"
	"	pushq %r9\n"
	"	pushq %r10\n"
	"	pushq %r11\n"
	"	pushq %rbx\n"
	"	pushq %rbp\n"
	"	pushq %r12\n"
	"	pushq %r13\n"
	"	pushq %r14\n"
	"	pushq %r15\n"
	"1:  call pmi_handler\n"
	"	popq %r15\n"
	"	popq %r14\n"
	"	popq %r13\n"
	"	popq %r12\n"
	"	popq %rbp\n"
	"	popq %rbx\n"
	"	popq %r11\n"
	"	popq %r10\n"
	"	popq %r9\n"
	"	popq %r8\n"
	"	popq %rax\n"
	"	popq %rcx\n"
	"	popq %rdx\n"
	"	popq %rsi\n"
	"	popq %rdi\n"
	"	addq $8,%rsp\n" /* error code */
	"	testq $3,8(%rsp)\n"
	"	jz 2f\n"
	"	swapgs\n"
	"2:\n"
	"	iretq");

