#include <asm-generic/irq_regs.h>
#include <asm/ptrace.h>

#include "rootsim.h"


/* The following function should be re-engineered by relying on hrtimers */


void my_smp_apic_timer_interrupt(struct pt_regs* regs) {

/* 	int i, target;
	unsigned long auxiliary_stack_pointer;
	unsigned long flags;
	unsigned int stretch_cycles;

        struct pt_regs *old_regs = set_irq_regs(regs);
*/
	/*
	 * NOTE! We'd better ACK the irq immediately,
	 * because timer handling can be slow.
	 *
	 * update_process_times() expects us to have done irq_enter().
	 * Besides, if we don't timer interrupts ignore the global
	 * interrupt lock, which is the WrongThing (tm) to do.
	 */

	// entering_ack_irq();
	//this is replaced by the below via function pointers
	//apic->eoi_write(APIC_EOI, APIC_EOI_ACK);//eoi_write unavailable, we fall back to write
//	apic->write(APIC_EOI, APIC_EOI_ACK);
//	my_irq_enter();
//	my_exit_idle();

/*	apic_interrupt_watch_dog++;//no problem if we do not perform this atomically, its just a periodic audit
	if (apic_interrupt_watch_dog >= 0x000000000000ffff){
		printk(KERN_DEBUG "%s: watch dog trigger for smp apic timer interrrupt %d CPU-id is %d\n", KBUILD_MODNAME, current->pid, smp_processor_id());
		apic_interrupt_watch_dog = 0;
	}

	f(current->mm == NULL) goto normal_APIC_interrupt;  // this is a kernel thread

	//for normal (non-overtick) scenarios we only pay the extra cost of the below cycle
	for(i = 0; i < TS_THREADS; i++){

		if(ts_threads[i] == current->pid){
			DEBUG_APIC
			printk(KERN_INFO "%s: found APIC registered thread %d on CPU %d - overtick count is %d - return instruction is at address %p\n", KBUILD_MODNAME, current->pid,smp_processor_id(),overtick_count[i],regs->ip);

			target = i;
			goto overtick_APIC_interrupt;
		}	

	} // end for

normal_APIC_interrupt:

	//still based on function pointers
//	if(stretch_flag[smp_processor_id()] == 0 || CPU_overticks[smp_processor_id()]<=0){
//		local_apic_timer_interrupt();
//	}
	

	//again - no additional cost (except for the predicate evaluation) in non-overtick scenarios
	if(stretch_flag[smp_processor_id()] == 1){
		local_irq_save(flags);
		CPU_overticks[smp_processor_id()] = 0;
		stretch_flag[smp_processor_id()] = 0;
		stretch_cycles = (*original_calibration) ; 
ENABLE 		my__setup_APIC_LVTT(stretch_cycles, 0, 1);
   		local_irq_restore(flags);
	}

	my_irq_exit();
	set_irq_regs(old_regs);

	return;

overtick_APIC_interrupt:

	//if(overtick_count[target] <= 0){// '<' should be redundant
	if( CPU_overticks[smp_processor_id()] <= 0 ){// '<' should be redundant
		CPU_overticks[smp_processor_id()] = OVERTICK_SCALING_FACTOR;
        	local_apic_timer_interrupt();
	}
	else{
		CPU_overticks[smp_processor_id()] -= 1;
	}


	if( old_regs != NULL ){//interrupted while in kernel mode running
		goto overtick_APIC_interrupt_kernel_mode;
	}

	if((regs->ip >= data_section_address) ){//interrupted while in kernel mode running
		goto overtick_APIC_interrupt_kernel_mode;
	}

	if(callback[target] != NULL){//check 1) callback existence and 2) no kernel mode running upon APIC timer interrupt

	local_irq_save(flags);
		if(regs->ip != callback[target]) {



			//printk("target callback address is %p\n",callback[target]);
			auxiliary_stack_pointer = regs->sp;
			auxiliary_stack_pointer -= sizeof(regs->ip);
			//printk("stack management information : reg->sp is %p - auxiliary sp is %p\n",regs->sp,auxiliary_stack_pointer);
	       	 	copy_to_user((void *)auxiliary_stack_pointer,(void *)&regs->ip, sizeof(regs->ip));	
	//		auxiliary_stack_pointer--;
	//       	 copy_to_user((void*)auxiliary_stack_pointer,(void*)&current->pid,8);	
	//		auxiliary_stack_pointer--;
	 //      	 copy_to_user((void*)auxiliary_stack_pointer,(void*)&current->pid,8);	
			//printk("stack management information : reg->sp is %p - auxiliary sp is %p - hitted objectr is %u - pgd descriptor is %u\n",regs->sp,auxiliary_stack_pointer,hitted_object,i);
			regs->sp = auxiliary_stack_pointer;
			regs->ip = callback[target];
		}
   	local_irq_restore(flags);
	}

overtick_APIC_interrupt_kernel_mode:

	local_irq_save(flags);
	stretch_flag[smp_processor_id()] = 1;
	stretch_cycles = (*original_calibration) / OVERTICK_SCALING_FACTOR; 
ENABLE 	my__setup_APIC_LVTT(stretch_cycles, 0, 1);
   	local_irq_restore(flags);

        my_irq_exit();
        set_irq_regs(old_regs);

	return;




//	overtick_count[target] -= COMPENSATE;
	local_irq_save(flags);
	stretch_flag[smp_processor_id()] = 1;
//	stretch_cycles = (*original_calibration) / OVERTICK_SCALING_FACTOR; 
//	stretch_cycles = (*original_calibration) / OVERTICK_COMPENSATION_FACTOR; 
	stretch_cycles = (*original_calibration) ; 
ENABLE 	my__setup_APIC_LVTT(stretch_cycles, 0, 1);
   	local_irq_restore(flags);
*/
	return;
}

void timer_init(void)
{
}

void timer_fini(void)
{
}
