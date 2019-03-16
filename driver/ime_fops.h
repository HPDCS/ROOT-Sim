#ifndef IME_FOPS_H
#define IME_FOPS_H

#define MAX_PMC 4

long ime_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

void set_mitigation(void* arg);

void clear_mitigation(void* arg);

#endif