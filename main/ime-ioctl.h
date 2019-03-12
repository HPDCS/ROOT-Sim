#ifndef _IME_IOCTL_H_
#define _IME_IOCTL_H_

#define MAX_ID_PMC 4
#define MAX_ID_CPU 4
#define MAX_ID_EVENT 7
#define numCPU sysconf(_SC_NPROCESSORS_ONLN)
#define MAX_CPU 256
#define MAX_BUFFER_SIZE 64

struct pmc_stats {
	int pmc_id;
    unsigned long percpu_value[MAX_CPU];
};

struct sampling_spec {
	int pmc_id;
    int event_id;
	int cpu_id[MAX_ID_CPU]; 
	uint64_t start_value; 
	int enable_PEBS[MAX_ID_CPU];
	int user[MAX_ID_CPU];
	int kernel[MAX_ID_CPU];
	int buffer_module_length;
    int buffer_pebs_length;
};

struct pebs_user{
	uint64_t eflags;	// 0x00
	uint64_t eip;	// 0x08
	uint64_t eax;	// 0x10
	uint64_t ebx;	// 0x18
	uint64_t ecx;	// 0x20
	uint64_t edx;	// 0x28
	uint64_t esi;	// 0x30
	uint64_t edi;	// 0x38
	uint64_t ebp;	// 0x40
	uint64_t esp;	// 0x48
	uint64_t r8;	// 0x50
	uint64_t r9;	// 0x58
	uint64_t r10;	// 0x60
	uint64_t r11;	// 0x68
	uint64_t r12;	// 0x70
	uint64_t r13;	// 0x78
	uint64_t r14;	// 0x80
	uint64_t r15;	// 0x88
	uint64_t stat;	// 0x90 IA32_PERF_GLOBAL_STATUS
	uint64_t add;	// 0x98 Data Linear Address
	uint64_t enc;	// 0xa0 Data Source Encoding
	uint64_t lat;	// 0xa8 Latency value (core cycles)
	uint64_t eventing_ip;	//0xb0 EventingIP
	uint64_t tsx;	// 0xb8 tx Abort Information
	uint64_t tsc;	// 0xc0	TSC
				// 0xc8
};

struct buffer_struct {
	struct pebs_user buffer_sample[MAX_BUFFER_SIZE];
	int last_index;
};

/* Use 'j' as magic number */
#define IME_IOC_MAGIC			'q'

#define _IO_NB	2

#define IME_PROFILER_ON						_IO(IME_IOC_MAGIC, _IO_NB)
#define IME_PROFILER_OFF					_IO(IME_IOC_MAGIC, _IO_NB+1)
#define IME_PMC_STATS						_IO(IME_IOC_MAGIC, _IO_NB+2)
#define IME_READ_BUFFER						_IO(IME_IOC_MAGIC, _IO_NB+3)
#define IME_RESET_BUFFER					_IO(IME_IOC_MAGIC, _IO_NB+4)

#endif /* _IME_IOCTL_H_ */