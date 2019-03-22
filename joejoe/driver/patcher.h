#ifndef _ISO_PATCHER_H
#define _ISO_PATCHER_H

#define SWITCH_FUNC "finish_task_switch"
#define MSR_IA32_PERF_GLOBAL_CTRL	    0x38F

typedef void h_func(void *);

int switch_patcher_init (unsigned long func_addr);
void switch_patcher_exit(void);
int switch_patcher_state(void);

#endif /* _ISO_PATCHER_H */