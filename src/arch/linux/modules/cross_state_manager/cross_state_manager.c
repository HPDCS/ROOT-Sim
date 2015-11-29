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

//#define SIBLING_PGD 128
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
//extern (*rootsim_pager)(struct task_struct *tsk); 
//extern void rootsim_load_cr3(ulong addr);

static inline void rootsim_load_cr3(pgd_t *pgdir) {
	__asm__ __volatile__ ("mov %0, %%cr3"
			      :
			      : "r" (__pa(pgdir)));
}

void (*rootsim_pager_hook)(void)=0x0;
#define PERMISSION_MASK 0777 
module_param(rootsim_pager_hook, ulong, PERMISSION_MASK); 

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

//TODO MN change currently_open from int to unsigned long
//#define MAX_CROSS_STATE_DEPENDENCIES 1024
unsigned long currently_open[SIBLING_PGD][MAX_CROSS_STATE_DEPENDENCIES];
int open_index[SIBLING_PGD]={[0 ... (SIBLING_PGD-1)] = -1};

void **ancestor_pml4;
int restore_pml4;  /* starting entry of pml4 for release operations of shadow pdp tables */
int restore_pml4_entries; /* entries of the pml4 involvrd in release operations of shadow pdp tables */
int mapped_processes; /* number of processes (application objects) mapped onto the special segment */
//int involved_pml4;

ulong callback;

struct vm_area_struct* changed_mode_mmap;
struct vm_operations_struct * original_vm_ops;
struct vm_operations_struct auxiliary_vm_ops_table;
struct vm_area_struct *target_vma;


int (*original_fault_handler)(struct vm_area_struct *vma, struct vm_fault *vmf);

static DEVICE_ATTR(multimap, S_IRUSR|S_IRGRP|S_IROTH, NULL, NULL);

/// File operations for the module
struct file_operations fops = {
	open:	rs_ktblmgr_open,
	unlocked_ioctl:rs_ktblmgr_ioctl,
	compat_ioctl:rs_ktblmgr_ioctl, // Nothing strange is passed, so 32 bits programs should work out of the box. Never tested, yet.
	release:rs_ktblmgr_release
};

//TODO MN
int dirty_pml4[512]={[0 ... (512-1)] = 0};;

void print_pgd(void** pgd_entry){
	int index_pgd;
	int index_pud;
	int pgd_busy;
	int pud_busy;
	void** pud_entry;
	void* temp;
		
	pgd_busy = 0;
                        
	for (index_pgd=0; index_pgd<PTRS_PER_PGD; index_pgd++){
		if(pgd_entry[index_pgd] != NULL){
			printk(KERN_ERR "\t\t[PML4E]: %d\n",index_pgd);
			pud_busy = 0;
			
			temp = (void *)((ulong) pgd_entry[index_pgd] & 0xfffffffffffff000);
                        temp = (void *)(__va(temp));
			pud_entry = (void **)temp;
			
			for (index_pud=0; index_pud<PTRS_PER_PUD; index_pud++){
				if(pud_entry[index_pud] != NULL) pud_busy++;
			}
			
			printk(KERN_ERR "\t\t\t\t[PDU_BUSY]: %d\n",pud_busy);
                        printk(KERN_ERR "\t\t\t\t[PDU_FREE]: %d\n",PTRS_PER_PUD - pud_busy);
                        pgd_busy++;

		}
	}
	printk(KERN_ERR "[PML4E_BUSY]: %d\n",pgd_busy);
        printk(KERN_ERR "[PML4E_FREE]: %d\n",PTRS_PER_PGD - pgd_busy);	
}

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
	int count_involved_pml4=-1;
        int index_involved_pml4;

	if(current->mm == NULL) return 0;  /* this is a kernel thread - not a rootsim thread */
	
	target_address = (void *)read_cr2();
	
	/* discriminate whether this is a classical fault or a root-sim proper fault */

	for(i=0;i<SIBLING_PGD;i++) {
		if ((root_sim_processes[i])==(current->pid)) {

			my_pgd =(void **)pgd_addr[i];
                        my_pdp =(void *)my_pgd[PML4(target_address)];
			
			ancestor_pdp =(void *) ancestor_pml4[PML4(target_address)];

			if(!dirty_pml4[PML4(target_address)]) {
				return 0; /* a fault outside the root-sim object zone - it needs to be handeld by the traditional fault manager */				
			}


			my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);
			if((void *)my_pdp[PDP(target_address)] != NULL)
				return 0; /* faults at lower levels than PDP - need to be handled by traditional fault manager */

			printk(KERN_ERR "addr: %p entry_pdp: %d dirty_pml4:%d\n",target_address,PDP(target_address),dirty_pml4[PML4(target_address)]);

#ifdef ON_FAULT_OPEN
			ancestor_pdp = __va((ulong)ancestor_pdp & 0xfffffffffffff000);
			my_pdp[PDP(target_address)] = ancestor_pdp[PDP(target_address)];
			rootsim_load_cr3(pgd_addr[i]);

			return 1;
#else
			rs_ktblmgr_ioctl(NULL,IOCTL_UNSCHEDULE_ON_PGD,(int)i);
#endif

			for(index_involved_pml4 = 0;index_involved_pml4 <= PML4(target_address);index_involved_pml4++){
				if(dirty_pml4[index_involved_pml4]) count_involved_pml4++;
			}
			hitted_object = count_involved_pml4*512 + PDP(target_address) ;
			

			auxiliary_stack_pointer = regs->sp;
			auxiliary_stack_pointer--;
			//printk("stack management information : reg->sp is %p - auxiliary sp is %p\n",regs->sp,auxiliary_stack_pointer);
		        copy_to_user((void *)auxiliary_stack_pointer,(void *)&regs->ip,8);	
			auxiliary_stack_pointer--;
		        copy_to_user((void *)auxiliary_stack_pointer,(void *)&hitted_object,8);	
			auxiliary_stack_pointer--;
		        copy_to_user((void *)auxiliary_stack_pointer,(void *)&i,8);	
//			printk("stack management information : reg->sp is %p - auxiliary sp is %p - hitted objectr is %u - pgd descriptor is %u\n",regs->sp,auxiliary_stack_pointer,hitted_object,i);
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

	return 0;

//skip blocking
	// Only one access at a time
	if (!mutex_trylock(&rs_ktblmgr_mutex)) {
		return -EBUSY;
	}

	return 0;
}


int rs_ktblmgr_release(struct inode *inode, struct file *filp) {
      	int s,i;
	void** pgd_entry;
       	void* pml4_entry;
       	void** pdpt_entry;
        void* temp;
	
	for (s=0;s<SIBLING_PGD;s++){
		if(original_view[s]!=NULL){ /* need to recover memory used for PDPs that have not been deallocated */
			
			pgd_entry = (void**) pgd_addr[s];
	
                        for (i=0; i<PTRS_PER_PGD; i++){ 
                        	pml4_entry = pgd_entry[i];
	                      	if(pml4_entry != NULL && dirty_pml4[i]){
                                	temp = (void *)((ulong) pml4_entry & 0xfffffffffffff000);
                                        temp = (void *)(__va(temp));
                                        pdpt_entry = (void **)temp;
					
					if(temp!=NULL)
                                        	__free_pages(temp,0);
                                }
			} 

			if(pgd_entry!=NULL)
				__free_pages(pgd_entry,0);
			original_view[s]=NULL;

		}// enf if != NULL
	}// end for s
	
	printk("\t Done release\n");
	return 0;
}


static void print_bits(unsigned long long number) {
	unsigned long long mask = 0x8000000000000000; // 64 bit
	char digit;

	while(mask) {
		digit = ((mask & number) ? '1' : '0');
//		printk("%c", digit);
		mask >>= 1 ;
	}
//	printk("\n");
}


static long rs_ktblmgr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

	/*void **my_pgd;
	void **my_pdp;
	void **ancestor_pdp;
	void **pgd_entry;
	void **source_pgd_entry;
	void *pdp_entry;
	void *pde_entry;
	void *pte_entry;
	void **temp;
	void **temp1;
	void **temp2;
	int pml4, pdp;
	int involved_pml4;
	void *source_pdp;
	//ulong aux;

	char* aux;
	char* aux1;*/


	//TODO MN
	int ret = 0;
	void *cr3;
	int i;
	void** pml4_table;
        int pml4_index;
        void** original_pml4;
	int descriptor;
	void** sheduled_mmaps_pointers;
	int obj_mmap_count;
	int index_mdt;
        int pdpt_index;
        void* pml4_entry;
        void* address_pdpt;
        void* temp;
        void** original_pdpt;
        void** pdpt_table;
        void* pdpt_entry;
	struct vm_area_struct *mmap;
	unsigned long object_to_close;


	switch (cmd) {

	case IOCTL_SET_ANCESTOR_PGD:
		printk("IOCTL_SET_ANCESTOR_ANCESTOR\n");
		ancestor_pml4 = (void **)current->mm->pgd;
	//	printk("ANCESTOR PML4 SET - ADDRESS IS %p\n",ancestor_pml4);
		break;

	case IOCTL_GET_PGD:
		printk("IOCTL_GET_PGD\n");
		mutex_lock(&pgd_get_mutex);
		for (i = 0; i < SIBLING_PGD; i++) {
			if (original_view[i] == NULL) {
				memcpy((void *)pgd_addr[i], (void *)(current->mm->pgd), 4096);
				
				//PML4 of current
 	                        pml4_table =(void **) pgd_addr[i];

				for(pml4_index=0;pml4_index<PTRS_PER_PGD;pml4_index++){
					if((pml4_table[pml4_index]!=NULL)&&(dirty_pml4[pml4_index])){
                                        	pml4_table[pml4_index] = NULL;
                                	}	
                        	}
	
			
				original_view[i] = current->mm;
				descriptor = i;
				ret = descriptor;
				goto pgd_get_done;
			}
		}
		ret = -1;
		pgd_get_done:
		mutex_unlock(&pgd_get_mutex);
goto bridging_from_get_pgd;
		break;

	case IOCTL_SCHEDULE_ON_PGD:	
//		printk("IOCTL_SCHEDULE_ON_PGD\n");
		//flush_cache_all();
		descriptor = ((ioctl_info*)arg)->ds;
//TODO MN
		sheduled_mmaps_pointers = ((ioctl_info*)arg)->objects_mmap_pointers;
		obj_mmap_count = ((ioctl_info*)arg)->objects_mmap_count;

		if (original_view[descriptor] != NULL) { //sanity check
			//PML4 of current
                         pml4_table =(void **) pgd_addr[descriptor];
				
		//	printk(KERN_ERR "[SCHEDULE_ON_PGD] before enter in update hitted object\n");
		//	print_pgd(pml4_table);			

                        //Original PML4
                        original_pml4 = (void **) original_view[descriptor]->pgd;

			for(pml4_index=0;pml4_index<PTRS_PER_PGD;pml4_index++){
				if((original_pml4[pml4_index]!=NULL)&&(pml4_table[pml4_index]==NULL)&&(!dirty_pml4[pml4_index])){
					printk("PML4_index: %d, descriptor: %d\n",pml4_index,descriptor);
					pml4_table[pml4_index] = original_pml4[pml4_index];					
				}
			}
                        
			for(index_mdt=0; index_mdt<obj_mmap_count; index_mdt++){

			    //Update currently_open with address of hitted obecjt
			    open_index[descriptor]++;
                            currently_open[descriptor][open_index[descriptor]]=(unsigned long) sheduled_mmaps_pointers[index_mdt];
                            
                            //Index of PML4 
                            pml4_index = pgd_index((unsigned long) sheduled_mmaps_pointers[index_mdt]);

                            //Entry PML4
                            pml4_entry =(void *) pml4_table[pml4_index];
				
                            if(original_pml4[pml4_index]==NULL){
                                    printk(KERN_ERR "[SCHEDULE_ON_PGD]: Rootsim error original_pml4[%d]=NULL\n",pml4_index);
                                    break;
                            }

                            if(pml4_entry==NULL){//DA QUI

                                //New page PDPT
                                address_pdpt = (void *)__get_free_pages(GFP_KERNEL, 0);
                                memset(address_pdpt,0,4096);

                                //Control bits
                                pml4_entry = (void *)((ulong) original_pml4[pml4_index] & 0x0000000000000fff);

                                //Final value of PML4E
                                address_pdpt = (void *)__pa(address_pdpt);
                                pml4_entry = (void *)((ulong)address_pdpt | (ulong)pml4_entry);
                      //          printk(KERN_ERR "NEW PAGE PML4");

                            }

                            //Pointer to Original PDPT
                            temp = (void *)((ulong) original_pml4[pml4_index] & 0xfffffffffffff000);
                            temp = (void *)(__va(temp));
                            original_pdpt = (void **)temp;

                            //Pointer to new PDPT                   
                            temp = (void *)((ulong) pml4_entry & 0xfffffffffffff000);
                            temp = (void *)(__va(temp));
                            pdpt_table = (void **)temp;
				
                    //        printk(KERN_ERR "pdpt_table: %p\n",pdpt_table);
			
                            pdpt_index = pud_index((unsigned long) sheduled_mmaps_pointers[index_mdt]);

                            if(original_pdpt[pdpt_index] == NULL){
                                printk(KERN_ERR "[SCHEDULE_ON_PGD]: Rootsim error original_pdpt[%d]=NULL\n",pdpt_index);
                                break;
                            }

                            pdpt_entry = (void *)pdpt_table[pdpt_index];
                            if(pdpt_entry == NULL ){
				pdpt_entry = original_pdpt[pdpt_index];
                  //          	printk("Value of pdtp_index: %d\n",pdpt_index);
                            }
                           
				 
                            //Update new PDPTE                                      
                            pdpt_table[pdpt_index] = pdpt_entry;

                            //Update new PML4E
                            pml4_table[pml4_index] = pml4_entry;
                        }
	
			/* actual change of the view on memory */
			root_sim_processes[descriptor] = current->pid;
			rootsim_load_cr3(pgd_addr[descriptor]);
			

		//	printk(KERN_ERR "[SCHEDULE_ON_PGD] After update hitted object\n");
		//	print_pgd(pml4_table);
			ret = 0;
		}else{
			 ret = -1;
		}
		break;


	case IOCTL_UNSCHEDULE_ON_PGD:	
	//	printk("IOCTL_UNSCHEDULE_ON_PGD\n");
	
		//flush_cache_all();
		descriptor = arg;

		if ((original_view[descriptor] != NULL) && (current->mm->pgd != NULL)) { //sanity check
			root_sim_processes[descriptor] = -1;	
			rootsim_load_cr3(current->mm->pgd);
			
			for(i=open_index[descriptor];i>=0;i--){

				object_to_close = currently_open[descriptor][i];
				
				pml4_index = pgd_index(object_to_close);
//				printk("UNSCHEDULE: closing pml4 %d - object %d\n",pml4,object_to_close);
	//			continue;
				pml4_table =(void **)pgd_addr[descriptor];
				pdpt_table =(void *)pml4_table[pml4_index];
				pdpt_table = __va((ulong)pdpt_table & 0xfffffffffffff000);


				pdpt_table[pud_index(object_to_close)] = NULL;
				
				//printk(KERN_ERR "At the end of UNSCHEDULE \n");
				//print_pgd(pgd_addr[descriptor]);
			}

			open_index[descriptor] = -1;
			ret = 0;
		}else{
			ret = -1;
		}

		break;

	case IOCTL_GET_INFO_PGD:
//		printk("--------------------------------\n");	
//		printk("mm is  %p --  mm->pgd is %p -- PA(pgd) is %p\n",(void *)current->mm,(void *)current->mm->pgd,(void *)__pa(current->mm->pgd));
//		printk("PRINTING THE WHOLE PGD (non-NULL entries)\n");	
		pml4_table = (void **)current->mm->pgd;
		for(i=0;i<512;i++){
			if (*(pml4_table + i) != NULL){
//				printk("\tentry \t%d \t- value \t%p\n",i,(void *)(*(pgd_entry+i)));	
			//printk("\tentry \t%d \t- value \t%X\n",i,current->mm->pgd[i]);	
			}
		}	

		break;

	case IOCTL_GET_INFO_VMAREA:
		mmap = current->mm->mmap;
//		printk("--------------------------------\n");	

//		printk("PRINTING THE WHOLE VMAREA LIST\n");	
	//	pgd_entry = (void **)current->mm->pgd;
		for(i=0;mmap;i++){
			//if (*(pgd_entry + i) != NULL){
//			printk("\t VMAREA entry \t%d - start = \t%p - end = \t%p - ops addr = \t%p\n",i,(void *)mmap->vm_start,(void *)mmap->vm_end,(void *)mmap->vm_ops);	
			mmap = mmap->vm_next;
			//printk("\tentry \t%d \t- value \t%X\n",i,current->mm->pgd[i]);	
		//	}

		}	

		break;


	case IOCTL_GET_CR_REGISTERS:
		asm volatile("movq %%CR0, %0":"=r" (cr3));
//		printk("CR0 = ");
		print_bits((unsigned long long)cr3);
		
		asm volatile("\nmovq %%CR2, %0":"=r" (cr3));
//		printk("CR2 = ");
		print_bits((unsigned long long)cr3);
		
		asm volatile("\nmovq %%CR3, %0":"=r" (cr3));
//		printk("CR3 = ");
		print_bits((unsigned long long)cr3);
		
		asm volatile("\nmovq %%CR4, %0":"=r" (cr3));
//		printk("CR4 = ");
		print_bits((unsigned long long)cr3);

		break;

	case IOCTL_SET_VM_RANGE:
	printk("IOCTL_SET_VM_RANGE\n");
//TODO MN
			flush_cache_all(); /* to make new range visible across multiple runs */
			
			mapped_processes = (((ioctl_info*)arg)->mapped_processes);

			callback = ((ioctl_info*)arg)->callback;

			flush_cache_all(); /* to make new range visible across multiple runs */

		break;

bridging_from_get_pgd:
		arg = ret;
	case IOCTL_CHANGE_VIEW:
		 printk("IOCTL_CHANGE_VIEW\n");

//TODO MN
			flush_cache_all();
		break;
	
		case IOCTL_GET_FREE_PML4:
			original_pml4 = (void **)current->mm->pgd;
                        
			for (i=0; i<PTRS_PER_PGD; i++){
                                if(original_pml4[i]==NULL){ 
					dirty_pml4[i] = 1;
					return i;
					
				}
			}

			return -1;
		break;
	
		case IOCTL_PGD_PRINT:
			print_pgd((void**)current->mm->pgd);
                        return 0;
                break;

	case IOCTL_SYNC_SLAVES:

		break;

	case IOCTL_SCHEDULE_ID:
		
		break;

	case IOCTL_UNSCHEDULE_CURRENT:

		break;

	default:
		ret = -EINVAL;
	}

	return ret;

}



void foo(struct task_struct *tsk) {
	int i;

	if(current->mm != NULL){
		for(i=0;i<SIBLING_PGD;i++){	
			if ((root_sim_processes[i])==(current->pid)){	
				rootsim_load_cr3(pgd_addr[i]);
			}
		}
	}
}



static int rs_ktblmgr_init(void) {
	int ret;
	int i;
	struct kprobe kp;

	rootsim_pager_hook = foo;

	mutex_init(&pgd_get_mutex);


	// Dynamically allocate a major for the device
	major = register_chrdev(0, "rs_ktblmgr", &fops);
	if (major < 0) {
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

//	rootsim_pager = NULL;
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
