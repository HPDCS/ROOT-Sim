#pragma once

#include <linux/types.h>
#include <linux/fs.h>
#include <asm/special_insns.h>

// main.c
extern int rootsim_open(struct inode *inode, struct file *filp) ;
extern int rootsim_release(struct inode *inode, struct file *filp);

// fault.c
extern void rootsim_page_fault(struct pt_regs *regs, unsigned long err);
extern void fault_handler(void);
extern void fault_init(void);
extern void fault_fini(void);

// scheduler.c
extern int scheduler_init(void);
extern int scheduler_fini(void);

// ioctl.c
extern long rootsim_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

// pagetable.c
extern void release_pagetable(void);

// idt.c
extern int setup_idt(void);
extern void restore_idt(void);

// timer.c
extern void timer_init(void);
extern void timer_fini(void);


extern unsigned long cr0;

static inline void protect_memory(void)
{
	write_cr0(cr0);
}

static inline void unprotect_memory(void)
{
	cr0 = read_cr0();
	write_cr0(cr0 & ~0x00010000);
}


