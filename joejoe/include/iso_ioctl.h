#ifndef _ISO_IOCTL_H
#define _ISO_IOCTL_H

/* Use 'j' as magic number */
#define ISO_IOC_MAGIC	'I'

#define _IO_NB	1
#define _IOR_NB	(_IO_NB << 0x3)
#define _IOW_NB	(_IO_NB << 0x4)

						/* Enable the logic on all cpu */
#define ISO_GLOBAL_ON				_IO(ISO_IOC_MAGIC,  _IO_NB)
						/* Disable the logic on all cpu */
#define ISO_GLOBAL_OFF				_IO(ISO_IOC_MAGIC,  _IO_NB + 1)
// #define ISO_CPU_MASK_SET			_IO(ISO_IOC_MAGIC,  _IO_NB + 3)
// #define ISO_CPU_MASK_ON				_IO(ISO_IOC_MAGIC,  _IO_NB + 2)
// #define ISO_CPU_MASK_OFF			_IO(ISO_IOC_MAGIC,  _IO_NB + 3)

// #define HOP_CTL_STATS				_IOR(ISO_IOC_MAGIC, _IOR_NB, struct ctl_stats)
// #define HOP_TID_STATS				_IOR(ISO_IOC_MAGIC, _IOR_NB + 1, struct tid_stats)
// #define HOP_TID_PAGES				_IOR(ISO_IOC_MAGIC, _IOR_NB + 2, struct tid_page*)

						/* Set activity on the cpu mask (1 ON, 0 OFF) */
#define ISO_CPU_MASK_SET			_IOW(ISO_IOC_MAGIC, _IOW_NB, unsigned long long)
						/* Register a new thread, it is enabled by default */
#define ISO_ADD_TID				_IOW(ISO_IOC_MAGIC, _IOW_NB + 1, pid_t)
						/* Remove a thread */
#define ISO_DEL_TID				_IOW(ISO_IOC_MAGIC, _IOW_NB + 2, pid_t)
						/* Temporarily disable a thread  */
#define ISO_PAUSE_TID				_IOW(ISO_IOC_MAGIC, _IOW_NB + 3, pid_t)
						/* Reactive a disabled thread  */
#define ISO_RESUME_TID				_IOW(ISO_IOC_MAGIC, _IOW_NB + 4, pid_t)

// #define HOP_SET_BUF_SIZE			_IOW(ISO_IOC_MAGIC, _IOW_NB + 4, pid_t)
// #define HOP_SET_SAMPLING			_IOW(ISO_IOC_MAGIC, _IOW_NB + 5, pid_t)

#endif /* _ISO_IOCTL_H */