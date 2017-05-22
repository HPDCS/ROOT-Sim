/**		      Copyright (C) 2014-2015 HPDCS Group
*		       http://www.dis.uniroma1.it/~hpdcs
*
* This is free software;
* You can redistribute it and/or modify this file under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* This file is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this file; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file  schedule-hook.c
* @brief This is the main source for the Linux Kernel Module which implements
*	 a schedule-hook module allowing running a custom function each time a thread is cpu rescheduled
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Emanuele De Santis
*
* @date October 15, 2015
*/

#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/preempt.h>
#include <asm/atomic.h>
#include "ld.h"
#include "lend.h"
//#include <asm/page.h>
//#include <asm/cacheflush.h>
//#include <asm/apic.h>

// This gives access to read_cr0() and write_cr0()
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0)
	#include <asm/switch_to.h>
#else
	#include <asm/system.h>
#endif
#ifndef X86_CR0_WP
#define X86_CR0_WP 0x00010000
#endif

// This macro was added in kernel 3.5
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	#define APIC_EOI_ACK 0x0 /* Docs say 0 for future compat. */
#endif

#include "schedule-hook.h"

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,25)
#error Unsupported Kernel Version
#endif



#define DEBUG if(1) // change with 0 for removing module coded debug stuff
#define DEBUG_SCHEDULE_HOOK if(1) // change with 0 for removing schedule_hook coded debug stuff



unsigned long the_hook = 0;
#define PERMISSION_MASK (S_IRUSR | S_IRGRP | S_IROTH)
module_param(the_hook, ulong, PERMISSION_MASK);
unsigned int audit_counter = 0;
#define PERMISSION_MASK (S_IRUSR | S_IRGRP | S_IROTH)
module_param(audit_counter, int, PERMISSION_MASK);

static int bytes_to_patch_in_schedule = 5;

static atomic_t synch_leave;
static atomic_t synch_enter;

atomic_t count ;
atomic_t reference_count ;
ulong phase = 0;//this is used to implement a phase based retry protocol for umounting this module

static void synchronize_all_slaves(void *);
static void schedule_hook_cleanup(void);
static int schedule_hook_init(void);
static void schedule_unpatch(void);
static int schedule_patch(void);
static void print_bytes(char *str, unsigned char *ptr, size_t s);
static int check_patch_compatibility(void);
extern void schedule_hook(void);
extern void schedule_hook_2(void);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Pellegrini <pellegrini@dis.uniroma1.it>, Francesco Quaglia <quaglia@dis.uniroma1.it>");
MODULE_DESCRIPTION("Run time patch of the Linux kernel scheduler for supporting the execution of a generic custom function upon thread reschedule");
module_init(schedule_hook_init);
module_exit(schedule_unpatch);
//module_exit(schedule_hook_cleanup);


/* MODULE VARIABLES */

static unsigned char finish_task_switch_original_bytes[6];
void *finish_task_switch = (void *)FTS_ADDR;
void *finish_task_switch_next = (void *)FTS_ADDR_NEXT;

typedef struct backup{
	char bytes[128];
	void* init_addr;
	unsigned short int len;
} backup_t;

backup_t* b;

unsigned short int backup_count=0;

typedef struct instr {
		void* ptr;
		unsigned char bytecode[16];
		short unsigned int size;
} instr_t;


// These are set in hook.S
extern void schedule_hook_end(void);
extern void schedule_hook_patch_point(void);
extern void schedule_hook_patch_point_2(void);


static void synchronize_all_slaves(void *info) {
	(void)info;

	printk(KERN_DEBUG "%s: cpu %d entering synchronize_all_slaves\n", KBUILD_MODNAME, smp_processor_id());
	atomic_dec(&synch_enter);
	preempt_disable();
	while(atomic_read(&synch_leave) > 0);
	preempt_enable();
	printk(KERN_DEBUG "%s: cpu %d leaving synchronize_all_slaves\n", KBUILD_MODNAME, smp_processor_id());
}

#define synchronize_all() do { \
		printk("cpu %d asking from unpreemptive synchronization\n", smp_processor_id()); \
		atomic_set(&synch_enter, num_online_cpus() - 1); \
		atomic_set(&synch_leave, 1); \
		preempt_disable(); \
		smp_call_function_many(cpu_online_mask, synchronize_all_slaves, NULL, false); /* false because we manually synchronize */ \
		while(atomic_read(&synch_enter) > 0); \
		printk("cpu %d all kernel threads synchronized\n", smp_processor_id()); \
	} while(0)

#define unsynchronize_all() do { \
		printk("cpu %d freeing other kernel threads\n", smp_processor_id()); \
		atomic_set(&synch_leave, 0); \
		preempt_enable(); \
	} while(0)


static void print_bytes(char *str, unsigned char *ptr, size_t s) {
	size_t i;

	printk(KERN_DEBUG "%s: %s: ", KBUILD_MODNAME, str);
	for(i = 0; i < s; i++)
		printk(KERN_CONT "%02x ", ptr[i]);
	printk(KERN_CONT "\n");
}

static int check_patch_compatibility(void) {
	unsigned char *ptr;

	int matched = 0;
	int i;
	unsigned char magic[9] = {0x41, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f};
	int j = 0;

	print_bytes("Scheduler side - I will check the compatibility on these bytes", finish_task_switch, finish_task_switch_next - finish_task_switch);

	printk(KERN_DEBUG "%s: Scanning bytes backwards for magic matching ('-' are don't care bytes): ",KBUILD_MODNAME);
	ptr = (unsigned char *)finish_task_switch_next - 1;
	while((long)ptr >= (long)finish_task_switch) {

		if(*ptr != 0xc3) {
			printk(KERN_CONT "-");
			ptr--;
			continue;
		}

		// Here we should have found the first (from the end of the function) ret
		// Check whether the pattern is one of:
		// pop pop ret
		// pop pop pop ret
		// pop pop pop pop ret
		for(j = -1; j >= -4; j--) {
			matched = 0;
			for(i = 0; i < 9; i++) {
				if(ptr[j] == magic[i]) {
					printk(KERN_CONT "%02x ", ptr[j]);
					matched = 1;
					break;
				}
			}
			if(!matched)
				break;
		}

		if(matched) {

			printk(KERN_CONT "\n");
			if(ptr[j] == 0x41) { // Check for a REX prefix before the pop we stopped at
				printk(KERN_DEBUG "%s: first pop assumed to be one-byte, yet it's two-bytes. Self-correcting the patch.\n", KBUILD_MODNAME);
				bytes_to_patch_in_schedule = 5;
				ptr--;
			}

			// If the pattern is found, ptr must point to the first byte *after* the ret
			ptr++;
			break;
		}

		// We didn't find the pattern. Go on scanning backwards
		ptr--;

	}

	if(ptr == finish_task_switch) {
		printk(KERN_CONT "no valid ret instruction found in finish_task_switch\n");
		goto failed;
	}

	// We've been lucky! Update the position where we are going to patch the schedule function
	finish_task_switch_next = ptr;

	if(bytes_to_patch_in_schedule > 4) {
		printk(KERN_CONT "%02x ", ((unsigned char *)finish_task_switch_next)[-5]);
	}
	printk(KERN_CONT "%02x %02x %02x %02x %02x looks correct.\n",
			((unsigned char *)finish_task_switch_next)[-4],
			((unsigned char *)finish_task_switch_next)[-3],
			((unsigned char *)finish_task_switch_next)[-2],
			((unsigned char *)finish_task_switch_next)[-1],
			((unsigned char *)finish_task_switch_next)[-0]);

	printk(KERN_DEBUG "%s: total number of bytes to be patched is %d\n", KBUILD_MODNAME, bytes_to_patch_in_schedule);

	return 0;

	failed:

	printk(KERN_NOTICE "%s: magic check on bytes ", KBUILD_MODNAME);
	printk(KERN_CONT "%02x %02x %02x %02x %02x failed.\n",
			((unsigned char *)finish_task_switch_next)[-4],
			((unsigned char *)finish_task_switch_next)[-3],
			((unsigned char *)finish_task_switch_next)[-2],
			((unsigned char *)finish_task_switch_next)[-1],
			((unsigned char *)finish_task_switch_next)[-0]);

	return -1;
}



static int schedule_patch(void) {

	int pos = 0;
	long displacement;
	int ret = 0;
	unsigned long cr0;
	unsigned char bytes_to_redirect[6];
	int schedule_hook_length;
	int i=0;
	int j;
	void * temp;

	instr_t* v=(instr_t*) kmalloc(((unsigned char*)(finish_task_switch_next)-(unsigned char*)(finish_task_switch))*sizeof(instr_t), GFP_KERNEL);
	if(!v){
		printk("Errore 1\n");
		return 0;
	}
	memset(v, 0, ((unsigned char*)(finish_task_switch_next)-(unsigned char*)(finish_task_switch))*sizeof(instr_t));


	b=(backup_t*) kmalloc(20*sizeof(backup_t), GFP_KERNEL); //this size is the maximum number of return this module can handle
	if(!b){
		printk("Errore 2\n");
		return 0;
	}
	memset(b, 0, 20*sizeof(backup_t));

	//instr_t* v will contain all the instructions between finish_task_switch and finish_task_switch_next
	temp=(void*)finish_task_switch; // copy of finish_task_switch for ld

	while (temp<finish_task_switch_next){
		int size=length_disasm(temp, MODE_X64);
		v[i].size=size;
		v[i].ptr=(void*)((char*)(temp));
		memcpy(v[i].bytecode, (unsigned char*)(temp), size);
		i++;
		temp=((unsigned char*)(temp))+size;
	}
	//instr_t* v populated successfully
	print_bytes("finish_task_switch before patch", finish_task_switch, finish_task_switch_next-finish_task_switch);

	for (j=0; j<i; j++){
		print_bytes("instruction:", v[j].bytecode, v[j].size);
		//printk("\t address: %p\n", v[j].ptr);
	}

	//disable memory protection
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);

	//HERE FIND 0XC3 PREVIOUS INSTRUCTIONS, BACKUP THEM INTO AN ARRAY AND PATCH SCHEDULE
	print_bytes("finish_task_switch_next before self patching", (unsigned char *)finish_task_switch, finish_task_switch_next-finish_task_switch);
	for (j=0; j<i; j++){
		if (v[j].size==1 && v[j].bytecode[0]==0xc3){ //return found
			printk(KERN_DEBUG "%s: return found at address %p, offset %p\n", KBUILD_MODNAME, (unsigned char*)v[j].ptr, (void*)(v[j].ptr-finish_task_switch));
			int k=0;
			int count=1;
			do{
				k+=v[j-count].size;
				count++;
			}while(k<5);
			count--;
			print_bytes("upper bound instruction: ", v[j-count].bytecode, v[j-count].size);
			void* upper_bound=v[j-count].ptr;
			void* lower_bound=v[j].ptr;
			int size=lower_bound-upper_bound;
			printk(KERN_DEBUG "%s: size=%d\n", KBUILD_MODNAME, size);
			b[backup_count].init_addr=upper_bound;
			b[backup_count].len=size;
			memcpy(b[backup_count].bytes, upper_bound, size);
			print_bytes("Backup bytes: ", b[backup_count].bytes, size);

			//Assemble JMP
			displacement=((long)((long)schedule_hook+(long)backup_count*((long)schedule_hook_2-(long)schedule_hook))-(long)upper_bound)-5;
			//displacement=((long)schedule_hook-(long)finish_task_switch_next);
			pos = 0;
			bytes_to_redirect[pos++] = 0xe9;
			bytes_to_redirect[pos++] = (unsigned char)(displacement & 0xff);
			bytes_to_redirect[pos++] = (unsigned char)(displacement >> 8 & 0xff);
			bytes_to_redirect[pos++] = (unsigned char)(displacement >> 16 & 0xff);
			bytes_to_redirect[pos++] = (unsigned char)(displacement >> 24 & 0xff);

			print_bytes("assembled jump is", bytes_to_redirect, 5);

			backup_count++;

			//Patch schedule()
			//if (backup_count!=-1)
				memcpy(upper_bound, bytes_to_redirect, 5);
		}
	}

	print_bytes("schedule after patch: ", finish_task_switch, finish_task_switch_next-finish_task_switch);

	//here all bytes are backed-up

	int patch_size=schedule_hook_2-schedule_hook_patch_point;
	int patch_offset=schedule_hook_patch_point_2-schedule_hook_patch_point;
	printk(KERN_DEBUG "%s: schedule_hook is at address %p\n", KBUILD_MODNAME, schedule_hook);
	print_bytes("schedule_hook_patch_point: ", schedule_hook_patch_point, patch_size);

	//patch schedule_hook_patch_point

	for (j=0; j<backup_count; j++){
		memcpy(schedule_hook_patch_point+j*patch_offset, b[j].bytes, b[j].len);
		print_bytes("schedule_hook_patch_point after patch: ", schedule_hook+j*(schedule_hook_2-schedule_hook), (schedule_hook_2-schedule_hook));
	}
	write_cr0(cr0);
	kfree(v);
	return 0;
	out:
	return 0;
}


static void schedule_unpatch(void) {

	unsigned long cr0;

	synchronize_all();
	printk(KERN_DEBUG "%s: restoring standard schedule function...\n", KBUILD_MODNAME);


	// To unpatch, simply place back original bytes of finish_task_switch
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);


	// Restore original bytes of finish_task_switch postamble
	int i;
	for (i=0; i<backup_count; i++){
		memcpy(b[i].init_addr, b[i].bytes, b[i].len);
	}

	write_cr0(cr0);
	kfree(b);
	print_bytes("Schedule after restore: ", finish_task_switch, finish_task_switch_next-finish_task_switch);
	printk(KERN_INFO "%s: standard schedule function correctly restored...\n", KBUILD_MODNAME);
	unsynchronize_all();
	return;
}


static int schedule_hook_init(void) {

	int ret = 0;

	atomic_set(&count,0);
	atomic_set(&reference_count,0);

	printk(KERN_INFO "%s: mounting the module\n", KBUILD_MODNAME);

	ret = schedule_patch();

	return 0;

	failed_patch:

	return ret;
}


static void schedule_hook_cleanup(void) {

	int i;


retry_needed:
	switch(phase){

		case 0:
			schedule_unpatch();
			printk(KERN_DEBUG "%s: phase %lu of unmounting schedule_hook done\n",KBUILD_MODNAME,phase);
			phase = 1;
			goto retry_needed;

		case 1:
			printk(KERN_DEBUG "%s: phase %lu of unmounting schedule_hook (this can iterate)\n",KBUILD_MODNAME,phase);
			if(atomic_read(&count) == 0){
				printk(KERN_DEBUG "%s: phase %lu of unmounting schedule_hook done\n",KBUILD_MODNAME,phase);
				phase = 2;
			}
			goto retry_needed;

		case 2:
			printk(KERN_DEBUG "%s: phase %lu of unmounting schedule_hook (this is the last one)\n",KBUILD_MODNAME,phase);
			printk(KERN_INFO "%s: further delaying module unmount for multicore scheduler safety", KBUILD_MODNAME);
			for(i=0; i < DELAY; i++) printk(KERN_CONT ".");
			printk(KERN_CONT " delay is over\n");
			break;
	}


	printk(KERN_DEBUG "%s: module umount run by cpu %d\n",KBUILD_MODNAME,smp_processor_id());
	printk(KERN_INFO "%s: module unmounted successfully\n", KBUILD_MODNAME);

}
