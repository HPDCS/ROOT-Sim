#include <linux/percpu.h>	/* Macro per_cpu */
#include <linux/slab.h>
#include <asm/smp.h>
#include <linux/vmalloc.h>

#include "msr_config.h" 
#include "ime_pebs.h"
#include "../main/ime-ioctl.h"

#define PEBS_STRUCT_SIZE	sizeof(pebs_arg_t)
#define MAX_BUFFER_PEBS		32
typedef struct{
	u64 eflags;	// 0x00
	u64 eip;	// 0x08
	u64 eax;	// 0x10
	u64 ebx;	// 0x18
	u64 ecx;	// 0x20
	u64 edx;	// 0x28
	u64 esi;	// 0x30
	u64 edi;	// 0x38
	u64 ebp;	// 0x40
	u64 esp;	// 0x48
	u64 r8;		// 0x50
	u64 r9;		// 0x58
	u64 r10;	// 0x60
	u64 r11;	// 0x68
	u64 r12;	// 0x70
	u64 r13;	// 0x78
	u64 r14;	// 0x80
	u64 r15;	// 0x88
	u64 stat;	// 0x90 IA32_PERF_GLOBAL_STATUS
	u64 add;	// 0x98 Data Linear Address
	u64 enc;	// 0xa0 Data Source Encoding
	u64 lat;	// 0xa8 Latency value (core cycles)
	u64 eventing_ip;	//0xb0 EventingIP
	u64 tsx;	// 0xb8 tx Abort Information
	u64 tsc;	// 0xc0	TSC
				// 0xc8
}pebs_arg_t;
typedef struct{
	u64 bts_buffer_base;					// 0x00 
	u64 bts_index;							// 0x08
	u64 bts_absolute_maximum;				// 0x10
	u64 bts_interrupt_threshold;			// 0x18
	pebs_arg_t *pebs_buffer_base;			// 0x20
	pebs_arg_t *pebs_index;					// 0x28
	pebs_arg_t *pebs_absolute_maximum;		// 0x30
	pebs_arg_t *pebs_interrupt_threshold;	// 0x38
	u64 pebs_counter0_reset;				// 0x40
	u64 pebs_counter1_reset;				// 0x48
	u64 pebs_counter2_reset;				// 0x50
	u64 pebs_counter3_reset;				// 0x58
	u64 reserved;							// 0x60
}debug_store_t;

int nRecords_pebs = MAX_BUFFER_PEBS;
int nRecords_module = MAX_BUFFER_SIZE;
int write_index = 0;
int read_index = 0;
unsigned long write_cycle = 0;
unsigned long read_cycle = 0;
debug_store_t* percpu_ds;
struct pebs_user* buffer_sample;
static DEFINE_PER_CPU(unsigned long, percpu_old_ds);
static DEFINE_PER_CPU(pebs_arg_t *, percpu_pebs_last_written);
u64 reset_value[MAX_ID_PMC];

static int allocate_buffer(void)
{
	pebs_arg_t *ppebs;
	debug_store_t *ds = this_cpu_ptr(percpu_ds);
	ppebs = (pebs_arg_t *) kzalloc (PEBS_STRUCT_SIZE*nRecords_pebs, GFP_KERNEL);
	if (!ppebs) {
		pr_err("Cannot allocate PEBS buffer\n");
		return -1;
	}

	buffer_sample = (struct pebs_user*)kzalloc(PEBS_STRUCT_SIZE*nRecords_module, GFP_KERNEL);
	if (!buffer_sample) {
		pr_err("Cannot allocate BUFFER sample buffer\n");
		return -1;
	}

	__this_cpu_write(percpu_pebs_last_written, ppebs);

	ds->bts_buffer_base 			= 0;
	ds->bts_index					= 0;
	ds->bts_absolute_maximum		= 0;
	ds->bts_interrupt_threshold		= 0;
	ds->pebs_buffer_base			= ppebs;
	ds->pebs_index					= ppebs;
	ds->pebs_absolute_maximum		= ppebs + (nRecords_pebs-1);
	ds->pebs_interrupt_threshold	= ppebs + 2;
	ds->pebs_counter0_reset			= ~(reset_value[0]);
	ds->pebs_counter1_reset			= ~(reset_value[1]);
	ds->pebs_counter2_reset			= ~(reset_value[2]);
	ds->pebs_counter3_reset			= ~(reset_value[3]);
	ds->reserved					= 0;
	return 0;
}


void write_buffer(void){
	debug_store_t *ds;
	pebs_arg_t *pebs, *end;
	ds = this_cpu_ptr(percpu_ds);
	pebs = (pebs_arg_t *) ds->pebs_buffer_base;
	end = (pebs_arg_t *)ds->pebs_index;
	for (; pebs < end; pebs = (pebs_arg_t *)((char *)pebs + PEBS_STRUCT_SIZE)) {
		memcpy(&(buffer_sample[write_index]), pebs, sizeof(struct pebs_user));
		write_index++;
		if(read_cycle < write_cycle){
			unsigned long new_read_index = write_index%nRecords_module;
			if(new_read_index < read_index){ 
				read_cycle++;
			}
			read_index = new_read_index;
		}
		if(write_index == nRecords_module){
			write_index = 0;
			write_cycle++;
		}
	}
	ds->pebs_index = (pebs_arg_t *) ds->pebs_buffer_base;
}

int init_pebs_struct(void){
	percpu_ds = alloc_percpu(debug_store_t);
	if(!percpu_ds) return -1;
	return 0;
}

void exit_pebs_struct(void){
	free_percpu(percpu_ds);
}

void pebs_init(void *arg)
{
	u64 pebs;
	int pmc_id;
	unsigned long old_ds;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->enable_PEBS[smp_processor_id()] == 0) return;
	pmc_id = args->pmc_id;
	reset_value[pmc_id] = args->start_value;
	if(args->buffer_pebs_length > 0){
		nRecords_pebs = args->buffer_pebs_length;
	}

	if(args->buffer_module_length > 0){
		nRecords_module = args->buffer_module_length;
	}

	allocate_buffer(); 

	rdmsrl(MSR_IA32_DS_AREA, old_ds);
	__this_cpu_write(percpu_old_ds, old_ds);
	wrmsrl(MSR_IA32_DS_AREA, this_cpu_ptr(percpu_ds));

	rdmsrl(MSR_IA32_PEBS_ENABLE, pebs);
	wrmsrl(MSR_IA32_PEBS_ENABLE, pebs | (BIT(32+pmc_id) | BIT(pmc_id)));
}

void pebs_exit(void *arg)
{
	u64 pebs;
	int pmc_id;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->enable_PEBS[smp_processor_id()] == 0) return;
	pmc_id = args->pmc_id;
	rdmsrl(MSR_IA32_PEBS_ENABLE, pebs);
	wrmsrl(MSR_IA32_PEBS_ENABLE, pebs & ~(BIT(32+pmc_id) | BIT(pmc_id)));
	wrmsrl(MSR_IA32_DS_AREA, __this_cpu_read(percpu_old_ds));

}

ssize_t ime_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	debug_store_t *ds;
	pebs_arg_t *pebs, *end;
	int samp = 0;
	size_t samples = count / sizeof(struct pebs_user);
	struct pebs_user *nbuf = (struct pebs_user *) buf;

	ds = this_cpu_ptr(percpu_ds);
	pebs = (pebs_arg_t *) ds->pebs_buffer_base;
	end = (pebs_arg_t *)ds->pebs_index;
	for (; pebs < end; pebs = (pebs_arg_t *)((char *)pebs + PEBS_STRUCT_SIZE)) {
		memcpy(&(nbuf[write_index]), pebs, sizeof(struct pebs_user));
		samp++;
		write_index++;
		if(read_cycle < write_cycle){
			unsigned long new_read_index = write_index%nRecords_module;
			if(new_read_index < read_index){ 
				read_cycle++;
			}
			read_index = new_read_index;
		}
		if(write_index == nRecords_module){
			write_index = 0;
			write_cycle++;
		}
		if(samp >= samples) break;
	}
	ds->pebs_index = (pebs_arg_t *) ds->pebs_buffer_base;
	return samp * sizeof(struct pebs_user);
}// ime_read
