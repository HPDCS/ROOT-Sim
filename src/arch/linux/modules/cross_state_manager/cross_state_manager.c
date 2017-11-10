/*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file cross_state_manager.c 
* @brief This Linux kernel module implements a modification to the x86_64 page
* 	 table management to support event cross state dependency tracking
* 
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*
* @date 
*       November 15, 2013 - Initial version
*       September 19, 2015 - Full restyle of the module, to use dynamic scheduler
* 			     patching
*/

//#ifdef HAVE_CROSS_STATE

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
#include <asm/tlbflush.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>

#include "cross_state_manager.h"

#define AUXILIARY_FRAMES 256

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,25)
#error Unsupported Kernel Version
#endif


/* FUNCTION PROTOTYPES */
static int rs_ktblmgr_init(void);
static void rs_ktblmgr_cleanup(void);
static int rs_ktblmgr_open(struct inode *, struct file *);
static int rs_ktblmgr_release(struct inode *, struct file *);
static long rs_ktblmgr_ioctl(struct file *, unsigned int, unsigned long);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Pellegrini <pellegrini@dis.uniroma1.it>, Francesco Quaglia <quaglia@dis.uniroma1.it>");
MODULE_DESCRIPTION("ROOT-Sim Multiple Page Table Kernel Module");
module_init(rs_ktblmgr_init);
module_exit(rs_ktblmgr_cleanup);


/* MODULE VARIABLES */
void (*rootsim_pager)(void)=0x0;

static inline void rootsim_load_cr3(pgd_t *pgdir) {
	__asm__ __volatile__ ("mov %0, %%cr3" :: "r" (__pa(pgdir)));
}

/// Device major number
static int major;

/// Device class being created
static struct class *dev_cl = NULL;

/// Device being created
static struct device *device = NULL;

/// Only one process can access this device (before spawning threads!)
static DEFINE_MUTEX(rs_ktblmgr_mutex);

struct mutex pgd_get_mutex;
struct mm_struct *mm_struct_addr[SIBLING_PGD];
void *pgd_addr[SIBLING_PGD];
unsigned int managed_pgds = 0;
struct mm_struct *original_view[SIBLING_PGD];

/* stack of auxiliary frames - used for chnage view */
int stack_index = AUXILIARY_FRAMES - 1;
void * auxiliary_frames[AUXILIARY_FRAMES];

int root_sim_processes[SIBLING_PGD]={[0 ... (SIBLING_PGD-1)] = -1};
fault_info_t *root_sim_fault_info[SIBLING_PGD];

//#define MAX_CROSS_STATE_DEPENDENCIES 1024
int currently_open[SIBLING_PGD][MAX_CROSS_STATE_DEPENDENCIES];
int open_index[SIBLING_PGD]={[0 ... (SIBLING_PGD-1)] = -1};

void **ancestor_pml4;
int restore_pml4;  /* starting entry of pml4 for release operations of shadow pdp tables */
int restore_pml4_entries; /* entries of the pml4 involvrd in release operations of shadow pdp tables */
int mapped_processes; /* number of processes (application objects) mapped onto the special segment */

ulong callback;

struct vm_area_struct* changed_mode_mmap;
struct vm_operations_struct * original_vm_ops;
struct vm_operations_struct auxiliary_vm_ops_table;
struct vm_area_struct *target_vma;

int (*original_fault_handler)(struct vm_area_struct *vma, struct vm_fault *vmf);

static DEVICE_ATTR(multimap, S_IRUSR|S_IRGRP|S_IROTH, NULL, NULL);

/// File operations for the module
struct file_operations fops = {
	open: rs_ktblmgr_open,
	unlocked_ioctl: rs_ktblmgr_ioctl,
	compat_ioctl: rs_ktblmgr_ioctl, // Nothing strange is passed, so 32 bits programs should work out of the box. Never tested, yet.
	release: rs_ktblmgr_release
};

/// This is to access the actual flush_tlb_all using a kernel proble
void (*flush_tlb_all_lookup)(void) = NULL;

int root_sim_page_fault(struct pt_regs* regs, long error_code){
 	void *target_address;
	void **my_pgd;
	void **my_pdp;
	void **target_pdp_entry;
	void **ancestor_pdp;
	ulong i;
	void *cr3;
	ulong *auxiliary_stack_pointer;
	ulong hitted_object;

	if(current->mm == NULL) return 0;  /* this is a kernel thread - not a rootsim thread */

	target_address = (void *)read_cr2();

	/* discriminate whether this is a classical fault or a root-sim proper fault */

	for(i=0;i<SIBLING_PGD;i++) {
		if ((root_sim_processes[i])==(current->pid)) {

			if((PML4(target_address)<restore_pml4) || (PML4(target_address))>=(restore_pml4+restore_pml4_entries))
				return 0; /* a fault outside the root-sim object zone - it needs to be handeld by the traditional fault manager */

			// The faulting address determines the gid of the LP whose state we are accessing	
			hitted_object = (PML4(target_address) - restore_pml4) * 512 + PDP(target_address) ;

			// Post information about the fault: this is required in case the fault
			// is related to a memory protection fault. In this way, the userspace
			// signal handler which will be called due to the following return 0
			// will let the handler compute for how many pages we need to get a lease.
			root_sim_fault_info[i]->rcx = regs->cx;
			root_sim_fault_info[i]->rip = regs->ip;
			root_sim_fault_info[i]->target_address = (long long)target_address;
			root_sim_fault_info[i]->target_gid = hitted_object;

			my_pgd =(void **)pgd_addr[i];
			my_pdp =(void *)my_pgd[PML4(target_address)];
			my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);
			if((void *)my_pdp[PDP(target_address)] != NULL) {
				return 0; /* faults at lower levels than PDP - need to be handled by traditional fault manager */
			}

			rs_ktblmgr_ioctl(NULL,IOCTL_UNSCHEDULE_ON_PGD,(int)i);

			// Pass required parameters to userland
			auxiliary_stack_pointer = regs->sp;
			auxiliary_stack_pointer--;
		    copy_to_user((void *)auxiliary_stack_pointer,(void *)&regs->ip,8);	
			regs->sp = auxiliary_stack_pointer;
			regs->ip = callback;

			return 1;
		}
	}

	return 0;
}

EXPORT_SYMBOL(root_sim_page_fault);

int rs_ktblmgr_open(struct inode *inode, struct file *filp) {

	// It's meaningless to open this device in write mode
	if (((filp->f_flags & O_ACCMODE) == O_WRONLY)
	    || ((filp->f_flags & O_ACCMODE) == O_RDWR)) {
		return -EACCES;
	}

	// Only one access at a time
	if (!mutex_trylock(&rs_ktblmgr_mutex)) {
		printk(KERN_INFO "%s: Trying to open an already-opened special device file\n", KBUILD_MODNAME);
		return -EBUSY;
	}

	return 0;
}


int rs_ktblmgr_release(struct inode *inode, struct file *filp) {
      	int i,j;
	int pml4, pdp;
	int involved_pml4;
	void **pgd_entry;
	void **temp;
	void *address;

	/* already logged by ancestor set */
	pml4 = restore_pml4; 
	involved_pml4 = restore_pml4_entries;

	for (j=0;j<SIBLING_PGD;j++){


		original_view[j] = NULL;
		continue;

		// SOme super bug down here!

		if(original_view[j]!=NULL){ /* need to recover memory used for PDPs that have not been deallocated */

			pgd_entry = (void **)pgd_addr[i];
			for (i=0; i<involved_pml4; i++){
			
			 	printk("\tPML4 ENTRY FOR CLOSE DEVICE IS %d\n",pml4);

				temp = pgd_entry[pml4];
				temp = (void *)((ulong) temp & 0xfffffffffffff000);	
				printk("temp is %p\n", temp);
				address = (void *)__va(temp);
				if(address!=NULL){
			//		__free_pages(address, 0); // TODO: there's a bug here! We leak memory instead! :-)
					printk("would free address at %p\n", address);
				}
				pgd_entry[pml4] = ancestor_pml4[pml4];

			}// end for i
			original_view[j]=NULL;
		}// enf if != NULL
	}// end for j

	mutex_unlock(&rs_ktblmgr_mutex);

	return 0;
}

static long rs_ktblmgr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

	int ret = 0;
	int i;
	void **my_pgd;
	void **my_pdp;
	void **pgd_entry;
	void *pdp_entry;
	void *pde_entry;
	void *pte_entry;
	void **temp;
	int descriptor;
	struct vm_area_struct *mmap;
	void *address;
	int pml4;
	int involved_pml4;
	int scheduled_object;
	void **ancestor_pdp;
	int *scheduled_objects;
	int scheduled_objects_count;
	int object_to_close;

	switch (cmd) {

		case IOCTL_SET_ANCESTOR_PGD:
			ancestor_pml4 = (void **)current->mm->pgd;
			break;

		case IOCTL_GET_PGD:
			descriptor = -1;
			mutex_lock(&pgd_get_mutex);
			for (i = 0; i < SIBLING_PGD; i++) {
				if (original_view[i] == NULL) {
					memcpy((void *)pgd_addr[i], (void *)(current->mm->pgd), 4096);
					original_view[i] = current->mm;
					root_sim_fault_info[i] = (fault_info_t *)arg;
					descriptor = i;
					goto pgd_get_done;
				}
			}

		    pgd_get_done:
			ret = descriptor; // Return -1 if no PGD is available
			mutex_unlock(&pgd_get_mutex);

			// If a valid descriptor is found, make a copy of the PGD entries
			if(descriptor != -1) {
				flush_cache_all();

				/* already logged by ancestor set */
				pml4 = restore_pml4; 
				involved_pml4 = restore_pml4_entries;
	
				pgd_entry = (void **)pgd_addr[descriptor];
	
				for (i = 0; i < involved_pml4; i++) {
				
					address = (void *)__get_free_pages(GFP_KERNEL, 0); /* allocate and reset new PDP */
					memset(address,0,4096);
				
					temp = pgd_entry[pml4];
			
					temp = (void *)((ulong) temp & 0x0000000000000fff);	
					address = (void *)__pa(address);
					temp = (void *)((ulong)address | (ulong)temp);
					pgd_entry[pml4] = temp;

					pml4++;
				}
			}
			
			break;

	 	case IOCTL_SCHEDULE_ON_PGD:	
			//flush_cache_all();
			descriptor = ((ioctl_info*)arg)->ds;
			//scheduled_object = ((ioctl_info*)arg)->id;
			scheduled_objects_count = ((ioctl_info*)arg)->count;
			scheduled_objects = ((ioctl_info*)arg)->objects;

			//scheduled_object = ((ioctl_info*)arg)->id;
			if (original_view[descriptor] != NULL) { //sanity check

				for(i = 0; i < scheduled_objects_count; i++) {

					//scheduled_object = TODO COPY FROM USER;
			        	copy_from_user((void *)&scheduled_object,(void *)&scheduled_objects[i],sizeof(int));	
					open_index[descriptor]++;
					currently_open[descriptor][open_index[descriptor]]=scheduled_object;

					//loadCR3 with pgd[arg]
					pml4 = restore_pml4 + OBJECT_TO_PML4(scheduled_object);
					my_pgd =(void **) pgd_addr[descriptor];
					my_pdp =(void *) my_pgd[pml4];
					my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);

					ancestor_pdp =(void *) ancestor_pml4[pml4];
					ancestor_pdp = __va((ulong)ancestor_pdp & 0xfffffffffffff000);

					/* actual opening of the PDP entry */
					my_pdp[OBJECT_TO_PDP(scheduled_object)] = ancestor_pdp[OBJECT_TO_PDP(scheduled_object)];
				}// end for 

				/* actual change of the view on memory */
				root_sim_processes[descriptor] = current->pid;
				rootsim_load_cr3(pgd_addr[descriptor]);
				ret = 0;
			} else {
				 ret = -1;
			}
			break;


		case IOCTL_UNSCHEDULE_ON_PGD:	

			descriptor = arg;

			if ((original_view[descriptor] != NULL) && (current->mm->pgd != NULL)) { //sanity check

				root_sim_processes[descriptor] = -1;
				rootsim_load_cr3(current->mm->pgd);

				for(i=open_index[descriptor];i>=0;i--){

					object_to_close = currently_open[descriptor][i];
	
					pml4 = restore_pml4 + OBJECT_TO_PML4(object_to_close);
					my_pgd =(void **)pgd_addr[descriptor];
					my_pdp =(void *)my_pgd[pml4];
					my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);
	
					/* actual closure of the PDP entry */
	
					my_pdp[OBJECT_TO_PDP(object_to_close)] = NULL;
				}
				open_index[descriptor] = -1;
				ret = 0;
			} else {
				ret = -1;
			}

			break;

		case IOCTL_SET_VM_RANGE:

			flush_cache_all(); /* to make new range visible across multiple runs */
			
			mapped_processes = (((ioctl_info*)arg)->mapped_processes);
			involved_pml4 = (((ioctl_info*)arg)->mapped_processes) >> 9; 
			if ( (unsigned)((ioctl_info*)arg)->mapped_processes & 0x00000000000001ff ) involved_pml4++;

			callback = ((ioctl_info*)arg)->callback;

			pml4 = (int)PML4(((ioctl_info*)arg)->addr);
			printk("LOGGING CHANGE VIEW INVOLVING %u PROCESSES AND %d PML4 ENTRIES STARTING FROM ENTRY %d (address %p)\n",((ioctl_info*)arg)->mapped_processes,involved_pml4,pml4, ((ioctl_info*)arg)->addr);
			restore_pml4 = pml4;
			restore_pml4_entries = involved_pml4;

			flush_cache_all(); /* to make new range visible across multiple runs */

			ret = 0;

			break;

		default:
			ret = -EINVAL;
	}

	return ret;

}



void the_pager(struct task_struct *tsk) {
	int i;
	void *cr3;

	if(current->mm != NULL){
		for(i=0;i<SIBLING_PGD;i++){	
			if ((root_sim_processes[i])==(current->pid)){	
	//		if(current->mm != NULL){
	//			rootsim_load_cr3(current->mm->pgd);
				rootsim_load_cr3(pgd_addr[i]);
//				printk("flushing thread cr3 onto the original PML4\n");
//				printk("flushing thread cr3 onto the sibling PML4\n");
			}
		}
	}
	//printk("OK\n");
	//rootsim_pager = NULL;
}



static int rs_ktblmgr_init(void) {

	int ret;
	int i;
	//int j;
	struct kprobe kp;


	rootsim_pager = the_pager;
	//rootsim_pager(NULL);

	mutex_init(&pgd_get_mutex);


	// Dynamically allocate a major for the device
	major = register_chrdev(0, "rs_ktblmgr", &fops);
	if (major < 0) {
//		printk(KERN_ERR "rs_ktblmgr: failed to register device. Error %d\n", major);
		ret = major;
		goto failed_chrdevreg;
	}
	printk("major for ktblmgr is %d\n",major);
	goto allocate;

	// Create a class for the device
	dev_cl = class_create(THIS_MODULE, "rootsim");
	if (IS_ERR(dev_cl)) {
//		printk(KERN_ERR "rs_ktblmgr: failed to register device class\n");
		ret = PTR_ERR(dev_cl);
		goto failed_classreg;
	}

	// Create a device in the previously created class
	device = device_create(dev_cl, NULL, MKDEV(major, 0), NULL, "ktblmgr");
	if (IS_ERR(device)) {
//		printk(KERN_ERR "rs_ktblmr: failed to create device\n");
		ret = PTR_ERR(device);
		goto failed_devreg;
	}


	// Create sysfs endpoints
	// dev_attr_multimap comes from the DEVICE_ATTR(...) at the top of this module
	// If this call succeds, then there is a new file in:
	// /sys/devices/virtual/rootsim/ktblmgr/multimap
	// Which can be used to dialogate with the driver
	ret = device_create_file(device, &dev_attr_multimap);
	if (ret < 0) {
		printk(KERN_WARNING "rs_ktblmgr: failed to create write /sys endpoint - continuing without\n");
	}

	// Initialize the device mutex
	mutex_init(&rs_ktblmgr_mutex);

	allocate:

	// Preallocate pgd
	for (i = 0; i < SIBLING_PGD; i++) {

		original_view[i] = NULL;

		if ( ! (mm_struct_addr[i] = kmalloc(sizeof(struct mm_struct), GFP_KERNEL)))
			goto bad_alloc;

		//if (!(pgd_addr[i] = pgd_alloc(mm_struct_addr[i]))) {kfree(mm_struct_addr[i]); goto bad_startup;}

		if (!(pgd_addr[i] = (void *)__get_free_pages(GFP_KERNEL, 0))) {
			kfree(mm_struct_addr[i]);
			goto bad_alloc;
		}
		mm_struct_addr[i]->pgd = pgd_addr[i];
		if ((void *)pgd_addr[i] != (void *)((struct mm_struct *)mm_struct_addr[i])->pgd) {
			printk("bad referencing between mm_struct and pgd\n");
			goto bad_alloc;
		}
		managed_pgds++;
	}

	printk(KERN_INFO "Correctly allocated %d sibling pgds\n", managed_pgds);

/*
	for (i=0;i<AUXILIARY_FRAMES; i++) {
		auxiliary_frames[i]=(void *)__get_free_pages(GFP_KERNEL,0);
		if(auxiliary_frames[i]==NULL){
			for(j=0;j<i;j++) __free_pages(auxiliary_frames[j],0);
			printk("cannot allocate auxiliary frames\n");
		}
		else{
		  if(i==(AUXILIARY_FRAMES-1)) printk(KERN_INFO "Correctly allocated %d auxiliary fames\n", i);
		}
	}
*/

	// Get a kernel probe to access flush_tlb_all
	memset(&kp, 0, sizeof(kp));
	kp.symbol_name = "flush_tlb_all";
	if (!register_kprobe(&kp)) {
		flush_tlb_all_lookup = (void *) kp.addr;
		unregister_kprobe(&kp);
	} 


	return 0;


    failed_devreg:
	class_unregister(dev_cl);
	class_destroy(dev_cl);
    failed_classreg:
	unregister_chrdev(major, "rs_ktblmgr");
    failed_chrdevreg:
	return ret;
 

    bad_alloc:
	printk(KERN_ERR "rs_ktblmgr: something wrong while preallocatin pgds\n");
	return -1;
}



static void rs_ktblmgr_cleanup(void) {

//	int i;
//	device_remove_file(device, &dev_attr_multimap);
//	device_destroy(dev_cl, MKDEV(major, 0));
//	class_unregister(dev_cl);
//	class_destroy(dev_cl);

	rootsim_pager = NULL;
	unregister_chrdev(major, "rs_ktblmgr");

	for (; managed_pgds > 0; managed_pgds--) {
		__free_pages((void *)mm_struct_addr[managed_pgds-1],0);
		kfree(mm_struct_addr[managed_pgds-1]);

	}

/*
	for (i=0;i<AUXILIARY_FRAMES; i++) {
		if(auxiliary_frames[i]==NULL){
			__free_pages(auxiliary_frames[i],0);
		}
		else{
		  printk(KERN_INFO "Error while deallocating auxiliary fames\n");
		}
	}
*/


}

//#endif	/* HAVE_CROSS_STATE */
