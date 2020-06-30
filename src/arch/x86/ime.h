#pragma once

#ifdef HAVE_PMU

// #include <stdint.h>

#define PMU_MSR_CODE		0x38F

// extern __thread uint32_t pmu_msr_high;
// extern __thread uint32_t pmu_msr_low;

static inline void __toggle_pmu_trace(unsigned int msr, uint32_t low, uint32_t high)
{
	__asm__ __volatile__("int $0xf0\n" // Linux reserved for future use IRQ vector
		     : : "c" (msr), "a"(low), "d" (high) : "memory");
}

static inline void on_pmu_trace(void) {
	__toggle_pmu_trace(PMU_MSR_CODE, 0xFULL, 0x0);
}

static inline void off_pmu_trace(void) {
	__toggle_pmu_trace(PMU_MSR_CODE, 0x0, 0x0);
}


#endif /* HAVE_PMU */
