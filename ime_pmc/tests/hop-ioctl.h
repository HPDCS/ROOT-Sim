#ifndef _HOP_IOCTL_H_
#define _HOP_IOCTL_H_

struct cpu_stats {
	int cpu;
	unsigned long denied;
	unsigned long no_ibs;
	unsigned long spurious;
	unsigned long requests;
};

struct ctl_stats {
	/* specific cpu stats */
	int nr_cpu;
	struct cpu_stats *cpus;

	/* gather all the cpus stats */
	unsigned long denied;
	unsigned long no_ibs;
	unsigned long spurious;
	unsigned long requests;
};

struct tid_stats {
	pid_t tid;
	unsigned long busy;
	unsigned long kernel;
	unsigned long memory;
	unsigned long samples;
};


/* Use 'j' as magic number */
#define HOP_IOC_MAGIC			'j'

#define _IO_NB	1
#define _IOR_NB	(_IO_NB << 0x3)
#define _IOW_NB	(_IO_NB << 0x4)

#define HOP_PROFILER_ON						_IO(HOP_IOC_MAGIC,  _IO_NB)
#define HOP_PROFILER_OFF					_IO(HOP_IOC_MAGIC,  _IO_NB + 1)
#define HOP_DEBUGGER_ON						_IO(HOP_IOC_MAGIC,  _IO_NB + 2)
#define HOP_DEBUGGER_OFF					_IO(HOP_IOC_MAGIC,  _IO_NB + 3)
#define HOP_CLEAN_TIDS						_IO(HOP_IOC_MAGIC,  _IO_NB + 4)

#define HOP_CTL_STATS						_IOR(HOP_IOC_MAGIC, _IOR_NB, struct ctl_stats)
#define HOP_TID_STATS						_IOR(HOP_IOC_MAGIC, _IOR_NB + 1, struct tid_stats)

#define HOP_ADD_TID						_IOW(HOP_IOC_MAGIC, _IOW_NB, pid_t)
#define HOP_DEL_TID						_IOW(HOP_IOC_MAGIC, _IOW_NB + 1, pid_t)
#define HOP_START_TID						_IOW(HOP_IOC_MAGIC, _IOW_NB + 2, pid_t)
#define HOP_STOP_TID						_IOW(HOP_IOC_MAGIC, _IOW_NB + 3, pid_t)
#define HOP_SET_BUF_SIZE					_IOW(HOP_IOC_MAGIC, _IOW_NB + 4, pid_t)
#define HOP_SET_SAMPLING					_IOW(HOP_IOC_MAGIC, _IOW_NB + 5, pid_t)

#endif /* _HOP_IOCTL_H_ */