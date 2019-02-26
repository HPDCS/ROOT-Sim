#ifndef _IME_IOCTL_H_
#define _IME_IOCTL_H_

#define MAX_ID_PMC 3
#define MAX_ID_EVENT 6
#define numCPU sysconf(_SC_NPROCESSORS_ONLN)
#define MAX_CPU 256

struct pmc_stats {
	int pmc_id;
    unsigned long percpu_value[MAX_CPU];
};

struct sampling_spec {
	int pmc_id;
    int event_id;
};

/* Use 'j' as magic number */
#define IME_IOC_MAGIC			'q'

#define _IO_NB	1

#define IME_PROFILER_ON						_IO(IME_IOC_MAGIC, _IO_NB)
#define IME_PROFILER_OFF					_IO(IME_IOC_MAGIC, _IO_NB+1)
#define IME_PMC_STATS						_IO(IME_IOC_MAGIC, _IO_NB+2)

#endif /* _IME_IOCTL_H_ */