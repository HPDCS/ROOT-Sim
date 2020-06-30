#include <linux/types.h>

/* CPUID info */
#define CPUID_INTEL_FAM7	0x6
#define CPUID_INTEL_MODEL7	0x8E
#define CPUID_INTEL_MODEL8	0x9E
#define IA32_PERF_CAPABILITIES	0x345

extern unsigned available_pmcs(void);
#define NR_PMCS available_pmcs()

/* Query the system via CPUID and see what the hardware provides */
extern int check_pmu_support(void); //DONE


extern struct pmc_cfg *get_pmc_config(unsigned pmc_id, unsigned cpu); 


extern void sync_system_pmu_state(void); // DONE

extern void enable_all_pmc(void); // DONE

extern void enable_all_pmc_system(void); // DONE



extern void reset_core_msr_pmc(void); // DONE

extern void disable_all_pmc(void); // DONE

extern void disable_all_pmc_system(void); // DONE


extern int pmc_init(void); // DONE

extern void pmc_fini(void);


extern int pebs_init(void); // DONE

extern void pebs_fini(void); // DONE

/* PEBS utility methods */
extern void flush_pebs_buffer(unsigned check);

extern void flush_pebs_buffer_on_cpu(unsigned cpu, unsigned check);


#define set_pmc_event(p, v)	p.evt = v
#define set_pmc_umask(p, v)	p.umask = v
#define set_pmc_pebs(p, v)	p.pebs = v
#define set_pmc_start(p, v)	p.counter = v
#define set_pmc_reset(p, v)	p.reset = v
#define set_pmc_pmi(p, v)	p.pmi = v
#define set_pmc_en(p, v)	p.en = v
#define set_pmc_usr(p, v)	p.usr = v
#define set_pmc_os(p, v)	p.os = v

struct pmc_cfg {
	union {
		u64 perf_evt_sel;
		struct {
			u64 evt: 8, umask: 8, usr: 1, os: 1, edge: 1, pc: 1, pmi: 1, 
			any: 1, en: 1, inv: 1, cmask: 8, reserved: 32;
		};
	};
	u32 counter;
	u64 reset;
	unsigned pebs;
	// u64 cpu_mask;
} __attribute__((packed));

#define PEBS_SAMPLE_SIZE sizeof(struct pebs_sample)
#define MEM_SAMPLE_SIZE sizeof(u64)

struct pebs_sample {
	u64 eflags;		// 0x00
	u64 eip;		// 0x08
	u64 eax;		// 0x10
	u64 ebx;		// 0x18
	u64 ecx;		// 0x20
	u64 edx;		// 0x28
	u64 esi;		// 0x30
	u64 edi;		// 0x38
	u64 ebp;		// 0x40
	u64 esp;		// 0x48
	u64 r8;			// 0x50
	u64 r9;			// 0x58
	u64 r10;		// 0x60
	u64 r11;		// 0x68
	u64 r12;		// 0x70
	u64 r13;		// 0x78
	u64 r14;		// 0x80
	u64 r15;		// 0x88
	u64 stat;		// 0x90 IA32_PERF_GLOBAL_STATUS
	u64 add;		// 0x98 Data Linear Address
	u64 enc;		// 0xa0 Data Source Encoding
	u64 lat;		// 0xa8 Latency value (core cycles)
	u64 eventing_ip;	//0xb0 EventingIP
	u64 tsx;		// 0xb8 tx Abort Information
	u64 tsc;		// 0xc0	TSC
				// 0xc8
};

struct debug_store {
	u64 bts_buffer_base;				// 0x00 
	u64 bts_index;					// 0x08
	u64 bts_absolute_maximum;			// 0x10
	u64 bts_interrupt_threshold;			// 0x18
	// pebs_arg_t *pebs_buffer_base;		
	// pebs_arg_t *pebs_index;			
	// pebs_arg_t *pebs_absolute_maximum;		
	// pebs_arg_t *pebs_interrupt_threshold;	
	u64 pebs_buffer_base;				// 0x20
	u64 pebs_index;					// 0x28
	u64 pebs_absolute_maximum;			// 0x30
	u64 pebs_interrupt_threshold;			// 0x38
	u64 pebs_counter0_reset;			// 0x40
	u64 pebs_counter1_reset;			// 0x48
	u64 pebs_counter2_reset;			// 0x50
	u64 pebs_counter3_reset;			// 0x58
	u64 reserved;					// 0x60
};