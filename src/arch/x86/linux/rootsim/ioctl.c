#include <asm-generic/errno-base.h>
#include <asm/uaccess.h>

#include "rootsim.h"
#include "ioctl.h"


long rootsim_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct mem_data *md;
	size_t length;
	memory_trace_t memory_trace;


	switch (cmd) {

// IME RELATED COMMANDS
		case IOCTL_GET_MEM_TRACE:
			copy_from_user(&memory_trace, arg, sizeof(memory_trace_t));
			md = per_cpu_ptr(&pcpu_mem_data, memory_trace.cpu);
			// No data to be read
			// if (md->read >= ((md->buf_size * md->pos) + md->index)) {
			if (md->read >= md->index) {
				break;
			}

			length = md->index - md->read;
			if (length > memory_trace.length)
				length = memory_trace.length;

			pr_info("Copying to %llx, from %llx, %d elems\n", memory_trace.addresses,
				(void *)(md->buf_poll[0] + md->read), length);

			ret = copy_to_user(memory_trace.addresses, (void *)(md->buf_poll[0] + md->read), length);
			pr_info("Copied %d\n", ret);
			if (!ret) {
				ret = copy_to_user((void *) arg, (void *)&memory_trace, sizeof(memory_trace_t));
				if (ret) {
					pr_info("Something wrong during copy to user!\n");
					ret = -1;
					break;
				}
			}
			pr_info("Read %llu, Index %llu\n", md->read, md->index);
			md->read += length;
			if (md->read == md->index) {
				md->read = 0;
				md->index = 0;
			}
			ret = length;
			break;
		case IOCTL_ADD_THREAD:
			register_thread((pid_t) arg);
			break;
		case IOCTL_DEL_THREAD:
			unregister_thread((pid_t) arg);
			break;
	}

/*
	switch (cmd) {


// ECS-related commands

		case IOCTL_SET_ANCESTOR_PGD:
			ancestor_pml4 = (void **)current->mm->pgd;
			break;


		case IOCTL_GET_PGD:

			printk("Entering IOCTL_GET_PGD\n");

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

				// already logged by ancestor set
				pml4 = restore_pml4; 
				involved_pml4 = restore_pml4_entries;
	
				pgd_entry = (void **)pgd_addr[descriptor];
	
				for (i = 0; i < involved_pml4; i++) {
				
					address = (void *)__get_free_pages(GFP_KERNEL, 0); // allocate and reset new PDP
					memset(address,0,4096);
				
					temp = pgd_entry[pml4];
			
					temp = (void *)((ulong) temp & 0x0000000000000fff);	
					address = (void *)__pa(address);
					temp = (void *)((ulong)address | (ulong)temp);
					pgd_entry[pml4] = temp;

					pml4++;
				}
			}
			printk("Leaving IOCTL_GET_PGD\n");
			
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

					//scheduled_object = TODO COPY FROM USER check return value
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

					// actual opening of the PDP entry
					my_pdp[OBJECT_TO_PDP(scheduled_object)] = ancestor_pdp[OBJECT_TO_PDP(scheduled_object)];
				}// end for 

				// actual change of the view on memory
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
	
					// actual closure of the PDP entry
	
					my_pdp[OBJECT_TO_PDP(object_to_close)] = NULL;
				}
				open_index[descriptor] = -1;
				ret = 0;
			} else {
				ret = -1;
			}

			break;

		case IOCTL_SET_VM_RANGE:

			flush_cache_all(); // to make new range visible across multiple runs
			
			mapped_processes = (((ioctl_info*)arg)->mapped_processes);
			involved_pml4 = (((ioctl_info*)arg)->mapped_processes) >> 9; 
			if ( (unsigned)((ioctl_info*)arg)->mapped_processes & 0x00000000000001ff ) involved_pml4++;

			callback = ((ioctl_info*)arg)->callback;

			pml4 = (int)PML4(((ioctl_info*)arg)->addr);
			printk("LOGGING CHANGE VIEW INVOLVING %u PROCESSES AND %d PML4 ENTRIES STARTING FROM ENTRY %d (address %p)\n",((ioctl_info*)arg)->mapped_processes,involved_pml4,pml4, ((ioctl_info*)arg)->addr);
			restore_pml4 = pml4;
			restore_pml4_entries = involved_pml4;

			flush_cache_all(); // to make new range visible across multiple runs

			ret = 0;
			break;

		case IOCTL_SET_PAGE_PRIVILEGE:
			set_page_privilege((ioctl_info *)arg);

			ret = 0;
			break;

		case IOCTL_PROTECT_REMOTE_LP:
			set_pte_sticky_flags((ioctl_info *)arg);
			break;


// TIMESTRETCH RELATED COMMANDS
		case IOCTL_SETUP_CALLBACK:

		ret = -1;

		for(i = 0; i < TS_THREADS; i++){

			if(ts_threads[i] == current->pid){
			DEBUG
			printk(KERN_INFO "%s: found registered thread entry %d - setting up callback at address %p\n", KBUILD_MODNAME, i, (void*)arg);

			callback[i] = arg;
			
			if( arg != 0x0 ){
				local_irq_save(aux_flags);
				stretch_flag[smp_processor_id()] = 1;
				stretch_cycles = (*original_calibration) / OVERTICK_SCALING_FACTOR; 
ENABLE 				my__setup_APIC_LVTT(stretch_cycles, 0, 1);
	       			local_irq_restore(aux_flags);
			}
		
			ret = 0;
//			break;
			}	
		}

		break;

 	case IOCTL_REGISTER_THREAD:

		mutex_lock(&ts_thread_register);

		DEBUG
		printk("thread %d - inspecting the array state\n",current->pid);

		DEBUG
		for (i=0;i< TS_THREADS;i++){
			printk("slot[%i] - value %d\n",i,ts_threads[i]);
		}


		data_section_address = arg;


		if(enabled_registering){
			for (i = 0; i < TS_THREADS; i++) {
				if (ts_threads[i] == -1) {
					ts_threads[i] = current->pid;
					overtick_count[i] = 0;
					descriptor = i;
					goto end_register;
				}
			}
		}

		DEBUG
		printk(KERN_INFO "%s: registering thread %d - arg is %p - thi is failure descriptor is %d\n", KBUILD_MODNAME, current->pid, (void*)arg,descriptor);

		descriptor = -1;
		
end_register:
		DEBUG
		printk("thread %d - reinspecting the array state\n",current->pid);

		DEBUG
		for (i=0;i< TS_THREADS;i++){
			printk("slot[%i] - value %d\n",i,ts_threads[i]);
		}

		printk(KERN_INFO "%s: registering thread %d done\n", KBUILD_MODNAME, current->pid);
		mutex_unlock(&ts_thread_register);

		time_cycles = *original_calibration;

		ret = descriptor;

		break;

	case IOCTL_DEREGISTER_THREAD:
		
		mutex_lock(&ts_thread_register);
		
		if(ts_threads[(unsigned int)arg] == current->pid){
			ts_threads[(unsigned int)arg]=-1;
			callback[(unsigned int)arg]=NULL;
		ret = 0;
		} else {
			ret = -EINVAL;
		} 

		mutex_unlock(&ts_thread_register);

		time_cycles = *original_calibration;
		local_irq_save(fl);
ENABLE 		my__setup_APIC_LVTT(time_cycles,0,1);
		stretch_flag[smp_processor_id()] = 0;
		local_irq_restore(fl);

		break;



		default:
			ret = -EINVAL;

	}
*/
	return ret;

}
