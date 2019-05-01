#pragma once

#define ROOTSIM_IOC_MAGIC 'R'

typedef struct _ioctl_info {
	int ds;
	void *addr;
	int id;
	unsigned int count;
	unsigned int *objects;
	unsigned mapped_processes;

	ulong callback;

	void *base_address;
	int page_count;
	int write_mode;
} ioctl_info;

#define ECS_MAJOR_FAULT	0
#define ECS_MINOR_FAULT	1
#define ECS_CHANGE_PAGE_PRIVILEGE	2

typedef struct _fault_info_t {
	long long rcx;
	long long rip;
	long long target_address;
	unsigned long target_gid;
	unsigned char fault_type;
} fault_info_t;

typedef struct _memory_trace_t {
	// TODO place u64, at the moment it doesn't compile
	unsigned long long *addresses;
	size_t length;
	unsigned cpu;
	// unsigned index;
} memory_trace_t;

#define _IO_NB		1
#define _IOR_NB		(_IO_NB << 0x3) // Reading data from the module
#define _IOW_NB		(_IO_NB << 0x4) // Wrinting data into the module
#define _IOWR_NB	(_IO_NB << 0x5) // Bidirectional operations


// Cross-State Events
#define IOCTL_SET_ANCESTOR_PGD 		_IO(ROOTSIM_IOC_MAGIC,	_IO_NB)

#define IOCTL_GET_PGD 			_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB, void *)
#define IOCTL_SET_VM_RANGE 		_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 1, ioctl_info *)
#define IOCTL_SCHEDULE_ON_PGD 		_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 2, ioctl_info *)
#define IOCTL_UNSCHEDULE_ON_PGD		_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 3, int)
#define IOCTL_SET_PAGE_PRIVILEGE	_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 4, int)
#define IOCTL_PROTECT_REMOTE_LP 	_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 5, int)

// Preëmption
#define IOCTL_DEREGISTER_THREAD 	_IO(ROOTSIM_IOC_MAGIC,	_IO_NB + 1)

#define IOCTL_REGISTER_THREAD 		_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 6, void *) 
#define IOCTL_SETUP_CALLBACK 		_IOW(ROOTSIM_IOC_MAGIC, _IOW_NB + 7, void *) 

// IME
#define IOCTL_GET_MEM_TRACE		_IOWR(ROOTSIM_IOC_MAGIC,_IOWR_NB, memory_trace_t *)
#define IOCTL_ADD_THREAD		_IOW(ROOTSIM_IOC_MAGIC,	_IOW_NB + 8, pid_t)
#define IOCTL_DEL_THREAD		_IOW(ROOTSIM_IOC_MAGIC,	_IOW_NB + 9, pid_t)
