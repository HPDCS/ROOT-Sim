#ifndef _ISO_CORE_H
#define _ISO_CORE_H

#include <linux/kernel.h>

struct iso_struct {
	u8 state;
	// TODO Change to BitsToLong
	u64 cpus_state; // it can manage at most 64 cpus
	u64 cpus_pmc;
};

extern struct iso_struct iso_struct;

extern void *pcpu_core_dev; // DEPRECATED

int enable_monitor(void);

void disable_monitor(void);

int core_init(void);

void core_exit(void);

int set_cpu_mask(u64 mask);

int set_cpu_on(unsigned cpu);

int set_cpu_off(unsigned cpu);

#endif /* _ISO_CORE_H */