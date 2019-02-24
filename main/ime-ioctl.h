#ifndef _IME_IOCTL_H_
#define _IME_IOCTL_H_

struct pmc_stats {
	int pmc_id;
    unsigned long value;
};

struct sampling_spec {
	int pmc_id;
    int event_id;
    unsigned long value;
};

/* Use 'j' as magic number */
#define IME_IOC_MAGIC			'q'

#define _IO_NB	1

#define IME_PROFILER_ON						_IO(IME_IOC_MAGIC, _IO_NB)
#define IME_PROFILER_OFF					_IO(IME_IOC_MAGIC, _IO_NB+1)
#define IME_PMC_STATS						_IO(IME_IOC_MAGIC, _IO_NB+2)

#endif /* _IME_IOCTL_H_ */