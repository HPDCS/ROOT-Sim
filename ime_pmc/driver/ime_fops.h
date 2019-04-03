#ifndef IME_FOPS_H
#define IME_FOPS_H

#include "intel_pmc_events.h"

#define MAX_PMC 4

long ime_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

void set_mitigation(void* arg);

void clear_mitigation(void* arg);

void disablePMC(void* arg);

extern u64 user_events[MAX_NUM_EVENT];

#endif