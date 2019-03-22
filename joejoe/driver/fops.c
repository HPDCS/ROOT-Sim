#include <linux/kernel.h>
#include <linux/sched.h>

#include "core.h"
#include "fops.h"
#include "iso_ioctl.h"
#include "thread.h"



int iso_open(struct inode *inode, struct file *file)
{
	pr_info("Open function\n");
	return 0;
}


int iso_release(struct inode *inode, struct file *file)
{
	pr_info("Release function\n");
	return 0;
}


long iso_ioctl(struct file *filep, unsigned int ioctl, unsigned long arg)
{
	u64 val;
	long err = -EFAULT;


	// if (!access_ok(VERIFY_frame, sizeof(*frame)))
	// 	goto out;

	switch (ioctl) {
	case ISO_GLOBAL_ON:
		enable_monitor();
		pr_info("ISO_GLOBAL_ON\n");
		break;
	case ISO_GLOBAL_OFF:
		disable_monitor();
		pr_info("ISO_GLOBAL_OFF\n");
		break;
	case ISO_CPU_MASK_SET:
		// if (!access_ok(VERIFY_READ, arg, sizeof(u64))) 
			// goto out;
		// if(__get_user(val, (u64 __user *)arg))
			// goto out;
		set_cpu_mask((u64) arg);
		pr_info("ISO_MASK_SET: %lx\n", arg);
		break;
	case ISO_ADD_TID:
		// if (!access_ok(VERIFY_READ, arg, sizeof(u64))) 
		// 	goto out;
		// if(__get_user(val, (u64 __user *)arg))
		// 	goto out;
		register_thread((pid_t) arg);
		pr_info("ISO_ADD_TID: %lx\n", arg);
		break;
	case ISO_DEL_TID:
		if (!access_ok(VERIFY_READ, arg, sizeof(u64))) 
			goto out;
		if(__get_user(val, (u64 __user *)arg))
			goto out;
		unregister_thread(val);
		pr_info("ISO_DEL_TID: %lx\n", arg);
		break;
	case ISO_PAUSE_TID:
	case ISO_RESUME_TID:
		break;
	default: 
		goto out;
	}



	err = 0;
out:
	return err;
}