#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/version.h>

#include "rootsim.h"

MODULE_AUTHOR("Alessandro Pellegrini <pellegrini@dis.uniroma1.it>");
MODULE_AUTHOR("Francesco Quaglia <francesco.quaglia@uniroma2.it>");
MODULE_AUTHOR("Stefano Carnà <stefano.carna.dev@gmail.com>");
MODULE_AUTHOR("Matteo Principe <matteo.principe92@gmail.com>");

MODULE_DESCRIPTION("This module will execute a specified function just after the task switch completion");

MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.0");

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,25)
#error Unsupported Kernel Version
#endif

/// This variable keeps a copy of CR0, used to unprotect/protect memory
unsigned long cr0;

/// Only one process at a time can interact with this mutex
static DEFINE_MUTEX(rootsim_mutex);

/// File operations for the module
struct file_operations fops = {
	open: rootsim_open,
	unlocked_ioctl: rootsim_ioctl,
	compat_ioctl: rootsim_ioctl,
	release: rootsim_release
};

// Variables to correctly setup/shutdown the pseudo device file
static int major;
static struct class *dev_cl = NULL;
static struct device *device = NULL;


int rootsim_open(struct inode *inode, struct file *filp) {

	// It's meaningless to open this device in write mode
	// if (((filp->f_flags & O_ACCMODE) == O_WRONLY) || ((filp->f_flags & O_ACCMODE) == O_RDWR)) {
	// 	return -EACCES;
	// }

	// Only one access at a time
	// if (!mutex_trylock(&rootsim_mutex)) {
	// 	printk(KERN_INFO "%s: Trying to open an already-opened special device file\n", KBUILD_MODNAME);
	// 	return -EBUSY;
	// }

	return 0;
}


int rootsim_release(struct inode *inode, struct file *filp)
{
//	release_pagetable();

	// mutex_unlock(&rootsim_mutex);

	return 0;
}


static int __init rootsim_init(void)
{
	int err = device_init(&fops);
	if (err) {
		pr_err("%s: Error while device initialization\n", KBUILD_MODNAME);
		goto failed_device;
	}

	printk(KERN_INFO "%s: ROOT-Sim device registered\n", KBUILD_MODNAME);

	scheduler_init();
	timer_init();
	fault_init();

	err = ime_init();
	if (err) goto failed_ime;
	return 0;

failed_ime:
	device_fini();
failed_device:
	return err;
}

static void __exit rootsim_exit(void)
{
	ime_fini();
	fault_fini();
	timer_fini();
	scheduler_fini();
	device_fini();
}


module_init(rootsim_init)
module_exit(rootsim_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HPDCS Group");