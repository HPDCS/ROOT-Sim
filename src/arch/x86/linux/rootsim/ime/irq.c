#include <linux/types.h>
#include <linux/smp.h>
#include <asm/apic.h>
#include <asm/msr.h>
#include "msr_config.h"
#include "irq.h"
#include "pmu.h"
#include "../rootsim.h" /* idt features */

#include <linux/delay.h>

/* Performance Monitor Interrupt handler */
void pmi_handler(void)
{
	u64 msr;
	u64 global; 

	// Does it make sense?
	// preempt_disable();

	rdmsrl(MSR_IA32_PERF_GLOBAL_CTRL, global);
	// pr_info("GLOBAL CTRL: %llx\n", global);

	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	// pr_info("GLOBAL STATUS: %llx\n", global);

	if(global & BIT_ULL(62)){
		flush_pebs_buffer(1);
	}

	// rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	// pr_info("GLOBAL STATUS: %llx\n", global);

	/* Reset PMUs */
	wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, global);

	/* ack apic */
	apic_eoi();
	/* Unmask PMI as, as it got implicitely masked. */
	apic_write(APIC_LVTPC, 241);

	// preempt_enable();
}

/**
 * This is the irq entry point to toggle the PMU support activity
 */
asm("	.globl pmu_toggle_entry\n"
	"pmu_toggle_entry:\n"
	"	wrmsr\n"
	"	iretq");

/**
 * This is the irq entry point to drain PEBS buffer
 */
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

