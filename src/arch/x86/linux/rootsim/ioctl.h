#pragma once

#define ROOTSIM_MODULE_MAGIC 'R'

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


// Cross-State Events
#define IOCTL_GET_PGD _IOW(ROOTSIM_MODULE_MAGIC, 1, void *)
#define IOCTL_SET_ANCESTOR_PGD _IO(ROOTSIM_MODULE_MAGIC, 2)
#define IOCTL_SET_VM_RANGE _IOW(ROOTSIM_MODULE_MAGIC, 3, ioctl_info *)
#define IOCTL_SCHEDULE_ON_PGD _IOW(ROOTSIM_MODULE_MAGIC, 4, ioctl_info *)
#define IOCTL_UNSCHEDULE_ON_PGD _IOW(ROOTSIM_MODULE_MAGIC, 5, int)
#define IOCTL_SET_PAGE_PRIVILEGE _IOW(ROOTSIM_MODULE_MAGIC, 6, int)
#define IOCTL_PROTECT_REMOTE_LP _IOW(ROOTSIM_MODULE_MAGIC, 7, int)

// Preëmption
#define IOCTL_REGISTER_THREAD _IOW(ROOTSIM_MODULE_MAGIC, 0, void *) 
#define IOCTL_DEREGISTER_THREAD _IO(ROOTSIM_MODULE_MAGIC, 1) 
#define IOCTL_SETUP_CALLBACK _IOW(ROOTSIM_MODULE_MAGIC, 3, void *) 
