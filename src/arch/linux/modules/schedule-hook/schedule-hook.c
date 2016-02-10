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

//#define AUXILIARY_FRAMES 256

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,25)
#error Unsupported Kernel Version
#endif

#define DEBUG if(1) // change with 0 for removing module coded debug stuff 
#define DEBUG_SCHEDULE_HOOK if(1) // change with 0 for removing schedule_hook coded debug stuff



void (*the_hook)(void) = 0x0;
#define PERMISSION_MASK (S_IRUSR | S_IRGRP | S_IROTH)
module_param(the_hook, ulong, PERMISSION_MASK);

int safe_guard = 1;

static int bytes_to_patch_in_schedule = 4;

static atomic_t synch_leave;
static atomic_t synch_enter;

atomic_t count ;
atomic_t reference_count ;
ulong phase = 0;//this is used to implement a phase based retry protocol for umounting this module
//module_param(phase, ulong, PERMISSION_MASK);


/** FUNCTION PROTOTYPES
 * All the functions in this module must be listed here
 * and explicitly handled in prepare_self_patch(), so as to allow a correct self-patching
 * independently of the order of symbols emitted by the linker, which changes
 * even when small changes to the code are made.
 * Failing to do so, could prevent self-patching and therefore module usability.
 * This approach does not work only if schedule_hook is the last function of the module,
 * or if the padding becomes smaller than 4 bytes...
 * In both cases, only a small change in the code (even placing a nop) could help doing
 * the job!
 */

static smp_call_func_t synchronize_patching(void *);
static void *prepare_self_patch(void);
static void schedule_hook_cleanup(void);
static int schedule_hook_init(void);
static void schedule_unpatch(void);
static int schedule_patch(void);
static void print_bytes(char *str, unsigned char *ptr, size_t s);
static int check_patch_compatibility(void);
static void schedule_hook(void);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Pellegrini <pellegrini@dis.uniroma1.it>, Francesco Quaglia <quaglia@dis.uniroma1.it>");
MODULE_DESCRIPTION("Run time patch of the Linux kernel scheduler for supporting the execution of a generic custom function upon thread reschedule");
module_init(schedule_hook_init);
module_exit(schedule_hook_cleanup);


/* MODULE VARIABLES */

static ulong watch_dog=0;

static unsigned char finish_task_switch_original_bytes[6];
void *finish_task_switch = (void *)FTS_ADDR;
void *finish_task_switch_next = (void *)FTS_ADDR_NEXT;


 

static smp_call_func_t synchronize_patching(void *ptr) {
	(void)ptr;

	printk("cpu %d entering synchronize_patching\n", smp_processor_id());
	atomic_dec(&synch_enter);
	preempt_disable();
	while(atomic_read(&synch_leave) > 0);
	preempt_enable();
	printk("cpu %d leaving synchronize_patching\n", smp_processor_id());
	return NULL;
}

int dummycount = 0;

#define synchronize_all() do { \
		printk("cpu %d asking from unpreemptive synchronization\n", smp_processor_id()); \
		atomic_set(&synch_enter, num_online_cpus() - 1); \
		atomic_set(&synch_leave, 1); \
		preempt_disable(); \
		smp_call_function_many(cpu_online_mask, synchronize_patching, NULL, false); /* 0 because we manually synchronize */ \
		while(atomic_read(&synch_enter) > 0); \
		printk("cpu %d all kernel threads synchronized\n", smp_processor_id()); \
	} while(0)

#define unsychronize_all() do { \
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
EXPORT_SYMBOL(print_bytes);


static void schedule_hook(void) {


	atomic_inc(&count);


	DEBUG_SCHEDULE_HOOK{
		watch_dog++; // the counter is shared but there is no need for atomicity - the reset will anyhow take place
		if (watch_dog >= 0x000000000000fff){
			printk(KERN_DEBUG "%s: watch dog trigger for thread %d (group leader is %d) CPU-id is %d\n", KBUILD_MODNAME, current->pid, current->tgid, smp_processor_id());
			watch_dog = 0;
		}
	}	
	if(the_hook) the_hook();

	atomic_dec(&count);

	// This gives us space for self patching. There are 5 bytes rather than 4, because
	// we might have to copy here 3 pop instructions, one two-bytes long and the other
	// one byte long.
	__asm__ __volatile__ ("nop; nop; nop; nop; nop");
	return;
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
			if(ptr[j] == 0x41) {
				printk(KERN_DEBUG "%s: first jump assumed to be one-byte, yet it's two-bytes. Self-correcting the patch.\n", KBUILD_MODNAME);
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
	unsigned char *ptr;
	unsigned long cr0;
	unsigned char bytes_to_redirect[6];

	printk(KERN_DEBUG "%s: start patching the schedule function...\n", KBUILD_MODNAME);

	// check_patch_compatibility overrides the content of finish_task_switch_next
	// as it is adjusted to the first byte after the ret instruction of finish_task_switch.
	// Additionally, bytes is set to the number of bytes required to correctly
	// patch the end of finish_task_switch.
	ret = check_patch_compatibility();
	if(ret)
		goto out;

	// Backup the final bytes of finish_task_switch, for later unmounting and finalization of the patch
	memcpy(finish_task_switch_original_bytes, finish_task_switch_next - bytes_to_patch_in_schedule, bytes_to_patch_in_schedule);
	print_bytes("made a backup of bytes", finish_task_switch_original_bytes, bytes_to_patch_in_schedule);

	// Compute the displacement for the jump to be placed at the end of the schedule function
	displacement = ((long)schedule_hook - (long)finish_task_switch_next);
	
	if(displacement != (long)(int)displacement) {
		printk(KERN_NOTICE "%s: Error: displacement out of bounds, I cannot hijack the schedule function postamble\n", KBUILD_MODNAME);
		ret = -1;
		goto out;
	}

	// Assemble the actual jump. Thanks to little endianess, we must manually swap bytes
	pos = 0;
	bytes_to_redirect[pos++] = 0xe9;
	bytes_to_redirect[pos++] = (unsigned char)(displacement & 0xff);
	bytes_to_redirect[pos++] = (unsigned char)(displacement >> 8 & 0xff);
	bytes_to_redirect[pos++] = (unsigned char)(displacement >> 16 & 0xff);
	bytes_to_redirect[pos++] = (unsigned char)(displacement >> 24 & 0xff);

	if(bytes_to_patch_in_schedule > 4)
		bytes_to_redirect[pos++] = 0x90; // Fill it with a nop, after the jump, to make the instrumentation cleaner

	print_bytes("assembled jump is", bytes_to_redirect, 5);

	// Find the correct place where to store backed up bytes from the original schedule function.
	// This is starting exactly at the ret (0xc3) instruction at the end of
	// schedule_hook. We start from the function after it, and for safety we check whether
	// there is enough space or not.

	//print_bytes("schedule_hook before prepare self patch", (unsigned char *)schedule_hook, 600);
	printk("%s: upon calling prepare_self_patch",KBUILD_MODNAME) ;
	ptr = (unsigned char *)prepare_self_patch();
	if(ptr == NULL) {
		ret = -1;
		goto out;
	}
	
//	printk("%s: upon succesfully returning from prepare_self_patch", KBUILD_MODNAME);

	// Synchronize threads. We force all other kernel threads to enter a synchronization function,
	// disable preemption, do the patching and then we can all continue happily
	synchronize_all();
	unsychronize_all();

	// Now do the actual patching. Clear CR0.WP to disable memory protection.
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);

	// Patch the end of our schedule_hook to execute final bytes of finish_task_switch
//	print_bytes("schedule_hook before self patching", (unsigned char *)schedule_hook, 600);
	memcpy(ptr, finish_task_switch_original_bytes, bytes_to_patch_in_schedule);
//	print_bytes("schedule_hook after self patching", (unsigned char *)schedule_hook, 600);

//	printk(KERN_DEBUG "%s: The patching will go from %p to %p, the following bytes will be patched: ", KBUILD_MODNAME, (unsigned char *)finish_task_switch_next - bytes_to_patch_in_schedule, (unsigned char *)finish_task_switch_next);
//	for(pos = 0; pos < bytes_to_patch_in_schedule; pos++) {
//		printk(KERN_CONT "%02x ", ((unsigned char *)finish_task_switch_next - bytes_to_patch_in_schedule)[pos]);
//	}
//	printk(KERN_CONT "\n");

	// Patch finish_task_switch to jump, at the end, to schedule_hook
//	print_bytes("finish_task_switch_next before self patching", (unsigned char *)finish_task_switch_next - bytes_to_patch_in_schedule, 64);
//	memcpy((unsigned char *)finish_task_switch_next - bytes_to_patch_in_schedule, bytes_to_redirect, bytes_to_patch_in_schedule);
//	print_bytes("finish_task_switch_next after self patching", (unsigned char *)finish_task_switch_next - bytes_to_patch_in_schedule, 64);
		
	write_cr0(cr0);

	

	printk(KERN_INFO "%s: schedule function correctly patched...\n", KBUILD_MODNAME);

    out:
	return ret;
}


static void schedule_unpatch(void) {
	unsigned long cr0;
	
	printk(KERN_DEBUG "%s: restoring standard schedule function...\n", KBUILD_MODNAME);

	// To unpatch, simply place back original bytes of finish_task_switch
	// and original bytes in the top half APIC interrupt
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);

	// Here is for the scheduler
//	memcpy((char *)finish_task_switch_next - bytes_to_patch_in_schedule, (char *)finish_task_switch_original_bytes, bytes_to_patch_in_schedule);

	write_cr0(cr0);

	printk(KERN_INFO "%s: standard schedule function correctly restored...\n", KBUILD_MODNAME);
}


static int schedule_hook_init(void) {

	int ret = 0;

	atomic_set(&count,0);
	atomic_set(&reference_count,0);

	printk(KERN_INFO "%s: mounting the module\n", KBUILD_MODNAME);

	ret = schedule_patch();	

	if(ret)
		goto failed_patch;

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

	//smp_call_function((smp_call_func_t)synchronize_patching,NULL,1);
	
	printk(KERN_DEBUG "%s: module umount run by cpu %d\n",KBUILD_MODNAME,smp_processor_id());

	printk(KERN_INFO "%s: module unmounted successfully\n", KBUILD_MODNAME);

}


static void *prepare_self_patch(void) {
	unsigned char *next = NULL;
	// All functions in this module, excepting schedule_hook.
	// Read the comment above function prototypes for an explanation
	void *functions[] = {	synchronize_patching,
				prepare_self_patch,
				schedule_hook_cleanup,
				schedule_hook_init,
				schedule_unpatch,
				schedule_patch,
				print_bytes,
				check_patch_compatibility,
				//time_stretch_ioctl,
				//time_stretch_release,
				//time_stretch_open,
				NULL};
	void *curr_f;
	int i = 0;
	unsigned char *ret_insn;
	unsigned long cr0;


	
	curr_f = functions[0];
	while(curr_f != NULL) {
		curr_f = functions[i++];
		
		if((long)curr_f > (long)schedule_hook) {
			
			if(next == NULL || (long)curr_f < (long)next)
				next = curr_f;
		}
	}
	
	// if ptr == NULL, then schedule_hook is last function in the kernel module...
	if(next == NULL) {
		printk(KERN_DEBUG "%s: schedule_hook is likely the last function of the module. ", KBUILD_MODNAME);
		printk(KERN_DEBUG "%s: Scanning from the end of the page\n", KBUILD_MODNAME);

		next = (unsigned char *)schedule_hook + 4096;
		next = (unsigned char*)((ulong) next & 0xfffffffffffff000);
		next--;

	}

	// We now look for the ret instruction. Some care must be taken here. We assume before the ret
	// there is at least one nop...
	while((long)next >= (long)schedule_hook) {
		if(*next == 0xc3) {
			if(*(next-1) == 0x58 || *(next-1) == 0x59 || *(next-1) == 0x5a || *(next-1) == 0x5b ||
			   *(next-1) == 0x5c || *(next-1) == 0x5d || *(next-1) == 0x5d || *(next-1) == 0x5f ||
			    *(next-1) == 0x90 )
				break;
		}
		next--;
	}

	printk(KERN_DEBUG "%s: Identified ret instruction byte %02x at address %p\n", KBUILD_MODNAME, *next, next);

	// Did we have luck?
	if((long)next == (long)schedule_hook) {
		ret_insn = NULL;
		goto out;
	} else {
		ret_insn = next;
	}

	// Now scan backwards until we find the last nop that we have placed in our code
	while(*next != 0x90)
		next--;

	print_bytes("before self patching", next - (bytes_to_patch_in_schedule  - 1), ret_insn - next + (bytes_to_patch_in_schedule  - 1));

	// Move backwars anything from here to the ret instruction.
	// This gives us space to insert the backed up bytes
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);
	memcpy(next - (bytes_to_patch_in_schedule  - 1), next + 1, ret_insn - next - 1);
	write_cr0(cr0);
	
	print_bytes("after self patching", next - (bytes_to_patch_in_schedule  - 1), ret_insn - next + (bytes_to_patch_in_schedule  - 1));

	// We have moved everything 4 bytes behind, which gives us space for finalizing the patch
	ret_insn -= bytes_to_patch_in_schedule;
    out:
	return ret_insn;
}

