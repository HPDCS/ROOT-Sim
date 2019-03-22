#include <linux/percpu.h>

#include "core.h"
#include "core-structs.h"

struct iso_struct iso_struct = {0, 0};

void *pcpu_core_dev;

int enable_monitor(void)
{
	iso_struct.state = 1;
	return 0;
}

void disable_monitor(void)
{
	iso_struct.state = 0;
}

int core_init(void)
{
	pcpu_core_dev = alloc_percpu(struct core_dev);
	return 0;
}// core_init

void core_exit(void)
{
	free_percpu(pcpu_core_dev);
}// core_exit

int set_cpu_mask(u64 mask)
{
	iso_struct.cpus_state = mask;
	return 0;
}// set_cpu_mask

int set_cpu_on(unsigned cpu)
{
	iso_struct.cpus_state |= BIT_ULL(cpu);
	return 0;
}// set_cpu_on

int set_cpu_off(unsigned cpu)
{
	iso_struct.cpus_state &= ~BIT_ULL(cpu);
	return 0;
}// set_cpu_off