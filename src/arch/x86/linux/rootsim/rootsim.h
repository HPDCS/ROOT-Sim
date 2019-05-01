#pragma once

#include <linux/types.h>
#include <linux/fs.h>
#include <asm/special_insns.h>
#include <asm/desc.h>

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
extern void scheduler_fini(void);

// ioctl.c
extern long rootsim_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

// pagetable.c
extern void release_pagetable(void);

// idt.c
extern gate_desc *clone_current_idt(void);
extern int patch_system_idt(gate_desc *idt, unsigned size);
extern void restore_system_idt(void);
extern int install_hook(gate_desc *idt, unsigned long handler, 
	unsigned vector, unsigned dpl);

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

// ime/core.c
extern int ime_init(void);
extern void ime_fini(void);

extern __percpu struct mem_data pcpu_mem_data;

#define NR_BUFFERS	1
#define BUFFER_SIZE	(PAGE_SIZE * 1024)

struct mem_data {
	u64 **buf_poll;
	unsigned nr_buf;
	unsigned buf_size;

	unsigned pos;
	unsigned index;
	unsigned read;
};

// ime/thread.c
int register_thread(pid_t pid);
int unregister_thread(pid_t pid);
void on_context_switch(void);


