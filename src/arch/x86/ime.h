#pragma once

#ifdef HAVE_PMU

#include <stdint.h>

#define PMU_MSR_HIGH_VALUE	0x0
#define PMU_MSR_LOW_VALUE	0x3

#define PMU_MSR_CODE		0x38f

extern uint32_t pmu_msr_high;
extern uint32_t pmu_msr_low;

static inline void __toggle_pmu_trace(unsigned int msr, uint32_t low, uint32_t high)
{
	__asm__ __volatile__("int $0xf0\n" // Linux reserved for future use IRQ vector
		     : : "c" (msr), "a"(low), "d" (high) : "memory");
}

static inline void toggle_pmu_trace(void) {
	pmu_msr_low ^= PMU_MSR_LOW_VALUE;
	__toggle_pmu_trace(PMU_MSR_CODE, pmu_msr_low, pmu_msr_high);
}


#endif /* HAVE_PMU */
