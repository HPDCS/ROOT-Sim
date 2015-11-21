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

//#define MAX_CROSS_STATE_DEPENDENCIES 1024
int currently_open[SIBLING_PGD][MAX_CROSS_STATE_DEPENDENCIES];
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

void print_pgd(void){
	int index_pgd;
	int index_pud;
	int pgd_busy;
	int pud_busy;
	void** pgd_entry;
	void** pud_entry;
	void* temp;
		
	pgd_busy = 0;
	pgd_entry = (void **)current->mm->pgd;
                        
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

	if(current->mm == NULL) return 0;  /* this is a kernel thread - not a rootsim thread */
	
	target_address = (void *)read_cr2();


	/* discriminate whether this is a classical fault or a root-sim proper fault */

	for(i=0;i<SIBLING_PGD;i++) {
		if ((root_sim_processes[i])==(current->pid)) {

			if((PML4(target_address)<restore_pml4) || (PML4(target_address))>=(restore_pml4+restore_pml4_entries)) return 0; /* a fault outside the root-sim object zone - it needs to be handeld by the traditional fault manager */

			my_pgd =(void **)pgd_addr[i];
			my_pdp =(void *)my_pgd[PML4(target_address)];
			my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);
			if((void *)my_pdp[PDP(target_address)] != NULL)
				return 0; /* faults at lower levels than PDP - need to be handled by traditional fault manager */

#ifdef ON_FAULT_OPEN
			ancestor_pdp =(void *) ancestor_pml4[PML4(target_address)];
			ancestor_pdp = __va((ulong)ancestor_pdp & 0xfffffffffffff000);
			my_pdp[PDP(target_address)] = ancestor_pdp[PDP(target_address)];
//			printk("\tthread %d - root-sim is opening the access to the address %p (loading the mask %p into the page table)\n",current->pid,target_address, (void *)ancestor_pdp[PDP(target_address)]);

			//PATCH cr3 = (void *)__pa(current->mm->pgd);
			rootsim_load_cr3(pgd_addr[i]);
		//	cr3 = (void *)__pa(pgd_addr[i]);
		//	asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (cr3));
/* to be improved with selective tlb invalidation */

			return 1;

#else
			rs_ktblmgr_ioctl(NULL,IOCTL_UNSCHEDULE_ON_PGD,(int)i);

#endif
			hitted_object = (PML4(target_address) - restore_pml4)*512 + PDP(target_address) ;
			

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

	if (!target_pdp_entry){ /* root-sim fault - open access and notify */
//		printk("root-sim fault at address %p (pml4 is %d - PDP is %d)\n",target_address,(int)PML4(target_address),(int)PDP(target_address));

		
		ancestor_pdp =(void *) ancestor_pml4[PML4(target_address)];
		my_pdp[PDP(target_address)] = ancestor_pdp[PDP(target_address)]; /* access opened */
	        return 1;	
	}
	else{ /* classical fault - just push the fault to the original handler */
		//original_fault_handler(vma,vmf);
		return 0;

	}

//	return 0;

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
      	int s,i,j;
	void** pgd_entry;
       	void* pml4_entry;
       	void** pdpt_entry;
        void* temp;
        void* temp_pdpt;
	
	for (s=0;s<SIBLING_PGD;s++){
		if(original_view[s]!=NULL){ /* need to recover memory used for PDPs that have not been deallocated */
			
			pgd_entry = (void **)pgd_addr[s];
	
                        for (i=0; i<PTRS_PER_PGD; i++){ 
                                pml4_entry = pgd_entry[i];

                                if(pml4_entry != NULL){
                                        temp = (void *)((ulong) pml4_entry & 0xfffffffffffff000);
                                        temp = (void *)(__va(temp));
                                        pdpt_entry = (void **)temp;
					/*
                                        for(j=0; j<PTRS_PER_PUD; j++){
                                                if(pdpt_entry[j] != NULL){
                                                       temp_pdpt = (void *)((ulong) pdpt_entry[j] & 0xfffffffffffff000);
                                                       temp_pdpt = (void *)(__va(temp_pdpt));
							
							if(temp_pdpt!=NULL)
								__free_pages(temp_pdpt,0);
                                                }
                                        }
					*/
					if(temp!=NULL)
                                        	__free_pages(temp,0);
                                }
			}	
                        
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

	int ret = 0;
	int i,j,z;
	void **my_pgd;
	void **my_pdp;
	void **ancestor_pdp;
	void *cr3;
	void **pgd_entry;
	void **source_pgd_entry;
	void *pdp_entry;
	void *pde_entry;
	void *pte_entry;
	void **temp;
	void **temp1;
	void **temp2;
	int descriptor;
	struct vm_area_struct *mmap;
	void *address;
	int pml4, pdp;
	int involved_pml4;
	void *source_pdp;
	int scheduled_object;
	int *scheduled_objects;
	int scheduled_objects_count;
	int object_to_close;
	//ulong aux;

	char* aux;
	char* aux1;

	switch (cmd) {

	case IOCTL_INIT_PGD:
//		printk(KERN_INFO "Correctly received an INIT_PGD code\n");
//		printk("int is %d - %d - %d %d %d\n",sizeof(int),sizeof(struct mm_struct),sizeof(struct vm_area_struct),sizeof(struct rb_root),sizeof(struct vm_area_struct));
//		printk("base  is %ul - field is %ul - diff is %ul \n",(ulong)(&current->mm->mmap),(ulong)(&current->mm->faultstamp),(ulong)(&current->mm->faultstamp)- (ulong)(&current->mm->mmap));
//		printk("base  is %ul - field is %ul - diff is %ul \n",(ulong)(&current->mm->mmap),(ulong)(&current->mm->pgd),(ulong)(&current->mm->pgd)- (ulong)(&current->mm->mmap));
//		printk("base  is %p - field is %p - diff is NA \n",(&current->mm->mmap),(&current->mm->total_vm));
		break;


	case IOCTL_REGISTER_THREAD:
	
		root_sim_processes[arg] = current->pid;

		//flush_cache_all();
/*
		for(i=0;i<SIBLING_PGD;i++){
			if (root_sim_processes[i]==-1) {
				root_sim_processes[i]=current->pid;
				break;
			}
		}
*/

		/* audit */
//		printk("LIST OF ROOT SIM PROCESSES AFTER REGISTERING\n");
//		for(i=0;i<SIBLING_PGD;i++) printk("%d  ",root_sim_processes[i]);
//		printk("\n");
		break;

	case IOCTL_DEREGISTER_THREAD:

/*
		flush_cache_all();
		for(i=0;i<SIBLING_PGD;i++){
			if (root_sim_processes[i]==current->pid) {
				root_sim_processes[i]=-1;
				break;
			}
		}
*/

		root_sim_processes[arg] = -1;
		/* audit */
//		printk("LIST OF ROOT SIM PROCESSES AFTER DEREGISTERING\n");
//		for(i=0;i<SIBLING_PGD;i++) printk("%d  ",root_sim_processes[i]);
//		printk("\n");
		break;

	case IOCTL_SET_ANCESTOR_PGD:
		//flush_cache_all();
		ancestor_pml4 = (void **)current->mm->pgd;
	//	printk("ANCESTOR PML4 SET - ADDRESS IS %p\n",ancestor_pml4);
		break;

	case IOCTL_GET_PGD:
		//flush_cache_all();
		mutex_lock(&pgd_get_mutex);
		printk(KERN_ERR "[IOCTL_GET_PGD] before enter in for");
		for (i = 0; i < SIBLING_PGD; i++) {
			if (original_view[i] == NULL) {
				//memcpy(mm_struct_addr[i], current->mm, sizeof(struct mm_struct));
				memcpy((void *)pgd_addr[i], (void *)(current->mm->pgd), 4096);
				original_view[i] = current->mm;
				descriptor = i;
				ret = descriptor;
				//flush_cache_all();
				//break;
				goto pgd_get_done;
			}
		}
		ret = -1;
		pgd_get_done:
		mutex_unlock(&pgd_get_mutex);
goto bridging_from_get_pgd;
		break;

	case IOCTL_RELEASE_PGD:
		//flush_cache_all();
goto bridging_from_pgd_release;
back_to_pgd_release:
		rootsim_load_cr3(current->mm->pgd);
		if (original_view[arg] != NULL) {
			original_view[arg] = NULL;
			ret = 0;
			break;
		}
		else{
//			printk("bad pgd release\n");
		}

		break;

	case IOCTL_SCHEDULE_ON_PGD:	
		//flush_cache_all();
		descriptor = ((ioctl_info*)arg)->ds;
		//scheduled_object = ((ioctl_info*)arg)->id;
		scheduled_objects_count = ((ioctl_info*)arg)->count;
		scheduled_objects = ((ioctl_info*)arg)->objects;
//TODO MN
		void** sheduled_mmaps_pointers;
		sheduled_mmaps_pointers = ((ioctl_info*)arg)->objects_mmap_pointers;
		int obj_mmap_count;
		obj_mmap_count = ((ioctl_info*)arg)->objects_mmap_count;

		//scheduled_object = ((ioctl_info*)arg)->id;
		if (original_view[descriptor] != NULL) { //sanity check

			for(i=0;i<scheduled_objects_count;i++){
				//scheduled_object = TODO COPY FROM USER;
		        	copy_from_user((void *)&scheduled_object,(void *)&scheduled_objects[i],sizeof(int));	
				open_index[descriptor]++;
				currently_open[descriptor][open_index[descriptor]]=scheduled_object;
				//loadCR3 with pgd[arg]
			}
			
			int index_mdt;
                        int pml4_index;
                        int pdpt_index;
                        int pd_index;
                        void** pml4_table;
                        void* pml4_entry;
                        void* address_pdpt;
                        void* temp;
                        void** original_pml4;
                        void** original_pdpt;
                        void** pdpt_table;
                        void* address_pd;
                        void* pdpt_entry;
                        void** pd_table;
                        void** original_pd;

                        printk(KERN_ERR "obj_mmap_count: %d\n",obj_mmap_count);
                        for(index_mdt=0; index_mdt<obj_mmap_count; index_mdt++){

                            //printk(KERN_ERR "index_mdt:%d \t obj_mem_size:%d \t  involved_pde:%d\n",index_mdt,obj_mem_size,involved_pde);
                            //printk(KERN_ERR "sheduled_mmaps_pointers[%d]:%p\n",index_mdt,sheduled_mmaps_pointers[index_mdt]);

                            //Index of PML4 
                            pml4_index = pgd_index((unsigned long) sheduled_mmaps_pointers[index_mdt]);

                            //PML4 of current
                            pml4_table =(void **) pgd_addr[descriptor];

                            //Original PML4
                            original_pml4 = (void **) original_view[descriptor]->pgd;

                            //Entry PML4
                            pml4_entry =(void *) pml4_table[pml4_index];
                            
			    printk(KERN_ERR "current->mm->pgd: %p \t original_view: %p \t pgd_addr: %p\n",current->mm->pgd,original_view[descriptor]->pgd,pgd_addr[descriptor]);
				
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
                                //printk(KERN_ERR "NEW PAGE PML4");

                            }

                            //Pointer to Original PDPT
                            temp = (void *)((ulong) original_pml4[pml4_index] & 0xfffffffffffff000);
                            temp = (void *)(__va(temp));
                            original_pdpt = (void **)temp;

                            //Pointer to new PDPT                   
                            temp = (void *)((ulong) pml4_entry & 0xfffffffffffff000);
                            temp = (void *)(__va(temp));
                            pdpt_table = (void **)temp;
				
                            //printk(KERN_ERR "pdpt_table: %p\n",pdpt_table);
			
                            pdpt_index = pud_index((unsigned long) sheduled_mmaps_pointers[index_mdt]);

                            if(original_pdpt[pdpt_index] == NULL){
                                printk(KERN_ERR "[SCHEDULE_ON_PGD]: Rootsim error original_pdpt[%d]=NULL\n",pdpt_index);
                                break;
                            }

                            pdpt_entry = (void *)pdpt_table[pdpt_index];
                            if(pdpt_entry == NULL ){
				pdpt_entry = original_pdpt[pdpt_index];
                            }
                            
                            //Update new PDPTE                                      
                            pdpt_table[pdpt_index] = pdpt_entry;

                            //Update new PML4E
                            pml4_table[pml4_index] = pml4_entry;
                        }
	
			//TODO MN DEBUG	
			//	break;

			/* actual change of the view on memory */
			root_sim_processes[descriptor] = current->pid;
			rootsim_load_cr3(pgd_addr[descriptor]);
			ret = 0;
		}else{
			 ret = -1;
		}
		break;


	case IOCTL_UNSCHEDULE_ON_PGD:	

		//flush_cache_all();
		descriptor = arg;

		if ((original_view[descriptor] != NULL) && (current->mm->pgd != NULL)) { //sanity check

			rootsim_load_cr3(current->mm->pgd);

			for(i=open_index[descriptor];i>=0;i--){

				object_to_close = currently_open[descriptor][i];
	
			
				pml4 = restore_pml4 + OBJECT_TO_PML4(object_to_close);
//				printk("UNSCHEDULE: closing pml4 %d - object %d\n",pml4,object_to_close);
	//			continue;
				my_pgd =(void **)pgd_addr[descriptor];
				my_pdp =(void *)my_pgd[pml4];
				my_pdp = __va((ulong)my_pdp & 0xfffffffffffff000);


				/* actual closure of the PDP entry */
	
				my_pdp[OBJECT_TO_PDP(object_to_close)] = NULL;
			}
			open_index[descriptor] = -1;
			ret = 0;
		}else{
			ret = -1;
		}

		break;

	case IOCTL_INSTALL_PGD:	
	//	flush_cache_all();
		if (original_view[arg] != NULL) {

			//loadCR3 with pgd[arg]
			root_sim_processes[arg] = current->pid;
			rootsim_load_cr3(pgd_addr[arg]);
			ret = 0;
			break;


			current->mm = mm_struct_addr[arg];
			current->active_mm = original_view[arg]; /* 30-1-2014 */
			atomic_inc(&original_view[arg]->mm_count); /* 30-1-2014 */
			current->mm->pgd = (void *)(pgd_addr[arg]);

//			printk("mm->pgd is %p -- cr3 is %p -- PA(pgd) is %p",(void *)current->mm->pgd,(void *)cr3,(void *)__pa(current->mm->pgd));
			flush_cache_all();
			cr3 = (void *)__pa(current->mm->pgd);
			asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (cr3));
			//flush_tlb_all_lookup();
			ret = 0;
			break;
		}
		else{
		 	printk("bad pgd install\n");
		}

		break;

	case IOCTL_GET_INFO_PGD:
//		printk("--------------------------------\n");	
//		printk("mm is  %p --  mm->pgd is %p -- PA(pgd) is %p\n",(void *)current->mm,(void *)current->mm->pgd,(void *)__pa(current->mm->pgd));
//		printk("PRINTING THE WHOLE PGD (non-NULL entries)\n");	
		pgd_entry = (void **)current->mm->pgd;
		for(i=0;i<512;i++){
			if (*(pgd_entry + i) != NULL){
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


	case IOCTL_UNINSTALL_PGD:
	//	flush_cache_all();
		if(current->mm != NULL){
			root_sim_processes[arg] = -1;
			rootsim_load_cr3(current->mm->pgd);
		}
	//		cr3 = (void *)__pa(current->mm->pgd);
	//		asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (cr3));
			//flush_tlb_all_lookup();
			ret = 0;
			break;
		if (original_view[arg] != NULL) {
//			printk("uninstalling the view - auditing the content of current and restore mm tables\n");
			aux = (char*)current->mm;
			aux1 = (char*)original_view[arg];
//			for(i=0;i<sizeof(struct mm_struct);i++) {
//				if (aux[i] != aux1[i]) printk("position %d - found %X  %X\n",i,aux[i],aux1[i]);
//			}
//			printk("vm_counts are : %d (current)  %d (original)\n",current->mm->mm_count,original_view[arg]->mm_count);
//			printk("printing current and original aux vectors\n");
//			for(i=0;i<AT_VECTOR_SIZE;i++){
//				printk("%ul ",current->mm->saved_auxv[i]);
//			}
//			printk("\n");
//			for(i=0;i<AT_VECTOR_SIZE;i++){
//				printk("%ul ",original_view[arg]->saved_auxv[i]);
//			}
//			printk("\n");
//			printk("contexts are : %d (current) - %d (original)\n",current->mm->context.size,original_view[arg]->context.size);
//			printk("compare on contexts is : %d\n",memcmp((char*)&current->mm->context,(char*)&original_view[arg]->context,sizeof(mm_context_t)));
//			printk("compare on cpu mask is : %d\n",memcmp((char*)&current->mm->cpu_vm_mask,(char*)&original_view[arg]->cpu_vm_mask,sizeof(cpumask_t)));
//			printk("compare on PML4 is : %d\n",memcmp((char*)current->mm->pgd,(char*)original_view[arg]->pgd,4096));
			//printk("vm_counts are : %d (current)  %d (original)\n",(struct mm_struct*)aux->mm_count,(struct mm_struct*)aux1->mm_count);

/*
			printk("LOGGING CURRENT PML4\n");
			pgd_entry=(void *)current->mm->pgd;
			for (i=0;i<512;i++){
				if((void *)pgd_entry[i]) printk("entry %i  is  %p\n",i,(void *)pgd_entry[i]);
			}
			printk("LOGGING CURRENT PML4\n");
			pgd_entry=(void *)current->mm->pgd;
			for (i=0;i<512;i++){
				if((void *)pgd_entry[i]) printk("entry %i  is  %p\n",i,(void *)pgd_entry[i]);
			}
*/

//			printk("\nDONE FIRST\n");
//			aux = (char*)original_view[arg];
//			for(i=0;i<sizeof(struct mm_struct);i++) printk("%X",aux[i]);
			printk("\n");
			current->mm = original_view[arg];
			atomic_dec(&original_view[arg]->mm_count); /* 30-1-2014 */
//			current->active_mm = original_view[arg]; /* 30-1-2014 */
			//atomic_inc(&(current->mm->mm_count));

			cr3 = (void *)__pa(current->mm->pgd);
			asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (cr3));

//			asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (current->mm->pgd):"rax","rbx");
			//asm volatile("movq %0,%%rax; movq %%rax,%%CR3"::"m" (current->mm->pgd):"rax");
			//flush_tlb_all_lookup();
			//current->mm->pgd = pgd_addr[arg];
			//memcpy(mm_struct_addr[i], current->mm, sizeof(struct mm_struct));
			//memcpy((void *)pgd_addr[i] ,(void *)(current->mm->pgd), 4096);
			//original_view[arg] = NULL;
			//descriptor = i;
			ret = 0;
			break;
		}
//		printk("bad pgd install\n");
		break;

	case IOCTL_SET_VM_RANGE:
//TODO MN
			flush_cache_all(); /* to make new range visible across multiple runs */
			
			mapped_processes = (((ioctl_info*)arg)->mapped_processes);

			callback = ((ioctl_info*)arg)->callback;

			flush_cache_all(); /* to make new range visible across multiple runs */

		break;

	case IOCTL_CHANGE_MODE_VMAREA:
			
		mmap = current->mm->mmap;
//		printk("--------------------------------\n");	
//		printk("FINDING VMAREA CONTAINIG ADDRESS %p FOR CHANGING ACCESS MODE\n",(void *)arg);
		//printk("PRINTING THE WHOLE VMAREA LIST\n");	
		//pgd_entry = (void **)current->mm->pgd;
		for(i=0;mmap;i++){
			//if (*(pgd_entry + i) != NULL){
			if (((void *)arg >= (void *)mmap->vm_start) && ((void *)(arg)<=(void *)mmap->vm_end)){
//				printk("\tFOUND TARGET VMAREA entry \t%d - start = \t%p - end = \t%p\n",i,(void *)mmap->vm_start,(void *)mmap->vm_end);	
				goto redirect;

			}
//			printk("\t VMAREA entry \t%d - start = \t%p - end = \t%p\n",i,(void *)mmap->vm_start,(void *)mmap->vm_end);	
			mmap = mmap->vm_next;
			//printk("\tentry \t%d \t- value \t%X\n",i,current->mm->pgd[i]);	
		//	}

		}	
redirect:

		/* logging current snapshot and redirecting the vmarea to auxiliary vmarea ops table */
		changed_mode_mmap = mmap;
		//original_vm_ops = mmap->vm_ops;
		//memcpy(&mmap->vm_ops,&auxiliary_vm_ops_table,sizeof(struct vm_operations_struct));
		//mmap->vm_ops = &auxiliary_vm_ops_table;
		//original_fault_handler = auxiliary_vm_ops_table.fault;
		//auxiliary_vm_ops_table.fault = root_sim_fault_handler;
		//mmap->
		//mmap->
	
		break;

	case IOCTL_TRACE_VMAREA:
			
		mmap = current->mm->mmap;
//		printk("--------------------------------\n");	
//		printk("FINDING VMAREA CONTAINIG ADDRESS %p\n",(void *)arg);
		//printk("PRINTING THE WHOLE VMAREA LIST\n");	
		//pgd_entry = (void **)current->mm->pgd;
		for(i=0;mmap && (i<128);i++){
			//if (*(pgd_entry + i) != NULL){
			if (((void *)arg >= (void *)mmap->vm_start) && ((void *)(arg)<=(void *)mmap->vm_end)){
//				printk("\tFOUND TARGET VMAREA entry \t%d - start = \t%p - end = \t%p\n",i,(void *)mmap->vm_start,(void *)mmap->vm_end);	
				goto secondlevel;

			}
//			printk("\t VMAREA entry \t%d - start = \t%p - end = \t%p\n",i,(void *)mmap->vm_start,(void *)mmap->vm_end);	
			mmap = mmap->vm_next;
			//printk("\tentry \t%d \t- value \t%X\n",i,current->mm->pgd[i]);	
		//	}

		}	
	
		if(!mmap){
//			printk("ERROR IN TRACING VMAREA\n");
			break;
		}

secondlevel:
		pgd_entry = (void *)current->mm->pgd;
	
		address = (void *)mmap->vm_start;

		for ( ; PML4(address) <= PML4((void *)mmap->vm_end) ; ){

			pdp_entry = (void *)pgd_entry[(int)PML4(address)];
			pdp_entry = (void *)((ulong) pdp_entry & 0xfffffffffffff000);
//			printk("\tPL4 TRACED ENTRY IS %d - value is %p - address is  %p\n",(int)PML4(address),pgd_entry[(int)PML4(address)],pdp_entry);
			if(pdp_entry != NULL){
				pdp_entry = __va(pdp_entry);
		
				temp = (void *)pdp_entry;

//					printk("\tPRINTING PDP (non-null entries) and the chain of PDE/PTE related entries\n");	
				for(i=0;i<512;i++){
				//	print_bits((unsigned long long)temp[i]);
					if ((temp[i]) != NULL){
//						printk("\t\tentry \t%d \t- value \t%p -- address is  %p\n",i,(void *)(temp[i]),(void *)((ulong) temp[i] & 0xfffffffffffff000));	

					//internal loop om PDE entries
				}
			}	
			}

			address = PML4_PLUS_ONE(address);
		}
		break;

		for ( ; PML4(address) <= PML4((void *)mmap->vm_end) ; ){

			pdp_entry = (void *)pgd_entry[(int)PML4(address)];
			pdp_entry = (void *)((ulong) pdp_entry & 0xfffffffffffff000);
//			printk("\tPL4 TRACED ENTRY IS %d - value is %p - address is  %p\n",(int)PML4(address),pgd_entry[(int)PML4(address)],pdp_entry);
			//pdp_entry = (void *)GET_ADDRESS(pdp_entry);
			//pdp_entry = pdp_entry >> 12; 
			//pdp_entry = pdp_entry << 12; 
			//printk("\tADDRES IN PL4 TRACED ENTRY IS %p\n",pdp_entry);
			pdp_entry = __va(pdp_entry);
		
			temp = (void **)pdp_entry;

//			printk("\tPRINTING PDP (non-null entries) and the chain of PDE/PTE related entries\n");	
			for(i=0;i<512;i++){
			//	print_bits((unsigned long long)temp[i]);
				if ((temp[i]) != NULL){
//					printk("\t\tentry \t%d \t- value \t%p -- address is  %p\n",i,(void *)(temp[i]),(void *)((ulong) temp[i] & 0xfffffffffffff000));	

					//internal loop om PDE entries
				
					pde_entry = (void *)((ulong) temp[i] & 0xfffffffffffff000);  
					//printk("\t\t\tADDRES IN PDE TRACED ENTRY IS %p\n",pde_entry);
					pde_entry = __va(pde_entry);
					temp1 = (void **)pde_entry;

//					printk("\t\tPDE TRACED ENTRIES\n");

					for(j=0;j<512;j++){
						if ((temp1[j]) != NULL){
//						printk("\t\t\tentry \t%d \t- value \t%p - address is  %p\n",j,(void *)(temp1[j]),(void *)((ulong) temp1[j] & 0xfffffffffffff000));	
						//printk("\t\t\tPDP TRACED ENTRY is %d\n",j);

						//now tracing the PTE
//						printk("\t\t\tPTE TRACED ENTRIES\n");
						pte_entry = (void *)((ulong) temp1[j] & 0xfffffffffffff000);  
						//printk("\t\t\t\tADDRES IN PTE TRACED ENTRY IS %p\n",pte_entry);
						pte_entry = __va(pte_entry);
						temp2 = (void **)pte_entry;
				//		printk("\t\t\t\tentry \t%d \t- value \t%p - address is  %p\n",j,(void *)(temp1[j]),(ulong) temp1[j] & 0xfffffffffffff000);	

						for(z=0;z<512;z++){
							if ((temp2[z]) != NULL){
//							printk("\t\t\t\tentry \t%d \t - value \t%p - address is %p\n",z,(void *)(temp2[z]),(void *)((ulong) temp2[z] & 0xfffffffffffff000));	
							//printk("\t\t\tPDP TRACED ENTRY is %d\n",i);
							} // end if temp2
						}// end for z

				   	}// end if temp1

				}// end for j

					//printk("\tentry \t%d \t- value \t%X\n",i,current->mm->pgd[i]);	
				} // end if temp
			}// end for i	

			address = PML4_PLUS_ONE(address);

		}// end lopp pn PML4	
		


		break;


bridging_from_get_pgd:
		arg = ret;
	case IOCTL_CHANGE_VIEW:
//TODO MN
			flush_cache_all();
			/*			
			pgd_entry = (void **)pgd_addr[arg];
			
			printk(KERN_ERR "pgd_entry: %p\n",pgd_addr[arg]);
		        
			void** original_pdpt_entry;
			void* pml4_entry;
			void* address_pdpt;
			void** new_pdpt_entry;
			void* address_pd;
			//void* temp;
			for (i=0; i<PTRS_PER_PGD; i++){
				//Current entry         
				pml4_entry = pgd_entry[i];

				if(pml4_entry != NULL){
					//New page PDPT
					address_pdpt = (void *)__get_free_pages(GFP_KERNEL, 0);
					memset(address_pdpt,0,4096);

					//Control bits
					pml4_entry = (void *)((ulong) pml4_entry & 0x0000000000000fff);

					//Final value of PML4E
					address_pdpt = (void *)__pa(address_pdpt);
					pml4_entry = (void *)((ulong)address_pdpt | (ulong)pml4_entry);

					//Pointer to Orinal PDPT
					temp = (void *)((ulong) pgd_entry[i] & 0xfffffffffffff000);
					temp = (void *)(__va(temp));
					original_pdpt_entry = (void **)temp;

					//Pointer to new PDPT                   
					temp = (void *)((ulong) pml4_entry & 0xfffffffffffff000);
					temp = (void *)(__va(temp));
					new_pdpt_entry = (void **)temp;

					int j;
					void* address_pd;
					void* pdpt_entry;
					for(j=0; j<PTRS_PER_PUD; j++){
						if(original_pdpt_entry[j] != NULL){
							//Control bits
							pdpt_entry = (void *)((ulong) original_pdpt_entry[j] & 0x0000000000000fff);
							
							if(!((ulong)pdpt_entry ^ 0x0000000000000061)){
								printk(KERN_ERR "i:%d j:%d \t",i,j);
								pdpt_entry = original_pdpt_entry[j];
							}
							else{
								//New page PD
								address_pd = (void *)__get_free_pages(GFP_KERNEL, 0);
								memset(address_pd,0,4096);

								//Final value of PDPTE
								address_pd = (void *)__pa(address_pd);
								pdpt_entry = (void *)((ulong)address_pd | (ulong)pdpt_entry);

								void** new_pd_entry;
								void** original_pd_entry;

								//Pointer to new PD                  
								temp = (void *)((ulong) pdpt_entry & 0xfffffffffffff000);
								temp = (void *)(__va(temp));
								new_pd_entry = (void **)temp;

								//Pointer to Orinal PD
								temp = (void *)((ulong) original_pdpt_entry[j] & 0xfffffffffffff000);
								temp = (void *)(__va(temp));
								original_pd_entry = (void **)temp;
								
								for(y=0; y<PTRS_PER_PMD; y++)
									if(original_pd_entry[y] != NULL)
										new_pd_entry[y] = original_pd_entry[y];
							//}

							//Update new PDPTE                                      
							new_pdpt_entry[j] = pdpt_entry;
						}
					}

					//Update new PML4E
					pgd_entry[i] = pml4_entry;
				}
			}

			*/
			printk("Done\n");
	//		rootsim_load_cr3(pgd_addr[arg]);
			//cr3 = (void *)__pa(current->mm->pgd);
			//asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (cr3)); /* flush the TLB - to be optimized with selective invalidation */

		break;
	
		case IOCTL_GET_FREE_PML4:
			pgd_entry = (void **)current->mm->pgd;
                        
			for (i=0; i<PTRS_PER_PGD; i++){
                                if(pgd_entry[i]==NULL){ return i;}
			}

			return -1;
		break;
	
		case IOCTL_PGD_PRINT:
			print_pgd();
                        return 0;
                break;



bridging_from_pgd_release:

	case IOCTL_RESTORE_VIEW:

	//		flush_cache_all();

			/* already logged by ancestor set */
			pml4 = restore_pml4; 
			involved_pml4 = restore_pml4_entries;

			// PATCH pgd_entry = (void **)current->mm->pgd;
			pgd_entry = (void **)pgd_addr[arg];
//		printk("RESTORE VIEW INVOLVING %u PROCESSES AND %d PML4 ENTRIES STARTING FROM ENTRY %d\n",mapped_processes,involved_pml4,pml4);

	//		break;

			for (i=0; i<involved_pml4; i++){
			
//			 	printk("\tPML4 ENTRY FOR RESTORE VIEW IS %d\n",pml4);

				//address = (void *)__get_free_pages(GFP_KERNEL, 0); /* allocate and reset new PDP */
				//memset(address,0,1024);
			
				temp = pgd_entry[pml4];
//				printk("changing this value %p\n",temp);
				
// TO PATCH IMMEDIATELY
				//temp = (void *)((ulong) temp & 0x0000000000000fff);	
				temp = (void *)((ulong) temp & 0xfffffffffffff000);	
				address = (void *)__va(temp);
				if(address!=NULL){
					__free_pages(address, 0);
				}
				//temp = (void *)((ulong)address | (ulong)temp);
				pgd_entry[pml4] = ancestor_pml4[pml4];

				pml4++;

			}

/*
			if(flag){
goto back_to_close;
			}
*/
			

	//		rootsim_load_cr3(pgd_addr[arg]);
			//cr3 = (void *)__pa(current->mm->pgd);
			//asm volatile("movq %%CR3, %%rax; andq $0x0fff,%%rax; movq %0, %%rbx; orq %%rbx,%%rax; movq %%rax,%%CR3"::"m" (cr3)); /* flush the TLB - to be optimized with selective invalidation */

goto back_to_pgd_release;

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

	rootsim_pager_hook = foo;
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
