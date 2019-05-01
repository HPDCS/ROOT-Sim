#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/version.h>

#include "rootsim.h"
#include "ime/core.h"

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
	if (!mutex_trylock(&rootsim_mutex)) {
		printk(KERN_INFO "%s: Trying to open an already-opened special device file\n", KBUILD_MODNAME);
		return -EBUSY;
	}

	return 0;
}


int rootsim_release(struct inode *inode, struct file *filp)
{
//	release_pagetable();

	mutex_unlock(&rootsim_mutex);

	return 0;
}


static int __init rootsim_init(void)
{
	int err;
	
	major = register_chrdev(0, KBUILD_MODNAME, &fops);

	// Dynamically allocate a major for the device
	if (major < 0) {
		printk(KERN_ERR "%s: Failed registering char device\n", KBUILD_MODNAME);
		err = major;
		goto failed_chrdevreg;
	}
		
	// Create a class for the device
	dev_cl = class_create(THIS_MODULE, "rootsim");
	if (IS_ERR(dev_cl)) {
		printk(KERN_ERR "%s: failed to register device class\n", KBUILD_MODNAME);
		err = PTR_ERR(dev_cl);
		goto failed_classreg;
	}
		
	// Create a device in the previously created class
	device = device_create(dev_cl, NULL, MKDEV(major, 0), NULL, KBUILD_MODNAME);
	if (IS_ERR(device)) {
		printk(KERN_ERR "%s: failed to create device\n", KBUILD_MODNAME);
		err = PTR_ERR(device);
		goto failed_devreg;
	}

	printk(KERN_INFO "%s: ROOT-Sim device registered with major number %d\n", KBUILD_MODNAME, major);

	scheduler_init();
	timer_init();
	fault_init();

	err = ime_init();
	if (err) goto failed_ime;
	
	return 0;
failed_ime:

failed_devreg:
	class_unregister(dev_cl);
	class_destroy(dev_cl);
failed_classreg:
	unregister_chrdev(major, KBUILD_MODNAME);
failed_chrdevreg:
	return err;
}

static void __exit rootsim_exit(void)
{
	// restore_idt();
	ime_fini();
	fault_fini();
	timer_fini();
	scheduler_fini();
	
	device_destroy(dev_cl, MKDEV(major, 0));
	class_unregister(dev_cl);
	class_destroy(dev_cl);
	unregister_chrdev(major, KBUILD_MODNAME);
}


module_init(rootsim_init)
module_exit(rootsim_exit)

