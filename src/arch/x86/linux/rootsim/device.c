#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include "rootsim.h"

#define MODULE_MINORS		1
#define MODULE_DEV_MINOR 	0

int module_major = 0;
static struct cdev module_cdev;
static struct class *module_class = NULL;

static int module_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}// module_uevent

static int setup_module_resources(struct file_operations *fops)
{
	int err = 0;
	dev_t dev_nb;
	struct device *device;
	struct cdev *cdev = &module_cdev;

	if (!fops) {
		pr_err("MAKE DEVICE ERROR: invalid fops address %lx\n", (unsigned long)fops);
		return -1;
	}

	// dev_t should keep the numbers associated to the device (check it)
	dev_nb = MKDEV(module_major, MODULE_DEV_MINOR);

	cdev_init(cdev, fops);
	cdev->owner = THIS_MODULE;

	err = cdev_add(cdev, dev_nb, 1);
	if (err) {
		pr_err("MAKE DEVICE ERROR: %d while trying to add %s%d\n", err, KBUILD_MODNAME, MODULE_DEV_MINOR);
		return err;
	}

	device = device_create(module_class, NULL, /* no parent device */ 
	dev_nb, NULL, /* no additional data */
	KBUILD_MODNAME);

	pr_devel("MAKE DEVICE Device [%d %d] has been built\n", module_major, MODULE_DEV_MINOR);

	if (IS_ERR(device)) {
		pr_err("MAKE DEVICE ERROR: %d while trying to create %s%d\n", err, KBUILD_MODNAME, MODULE_DEV_MINOR);
		return PTR_ERR(device);
	}

	return err;
}// setup_module_resources

static int setup_chdevs_resources(void)
{
	int err = 0;
	dev_t dev = 0;

	/* Get a range of minor numbers (starting with 0) to work with */
	err = alloc_chrdev_region(&dev, module_major, MODULE_MINORS, KBUILD_MODNAME);
	if (err < 0)
	{
		pr_err("alloc_chrdev_region() failed\n");        
		goto out;
	}

	module_major = MAJOR(dev);

	/* Create device class (before allocation of the array of devices) */
	module_class = class_create(THIS_MODULE, KBUILD_MODNAME);
	module_class->dev_uevent = module_uevent;

	/**
	 * The IS_ERR macro encodes a negative error number into a pointer, 
	 * while the PTR_ERR macro retrieves the error number from the pointer. 
	 * Both macros are defined in include/linux/err.h
	 */
	if (IS_ERR(module_class))
	{
		pr_err("Class creation failed\n");
		err = PTR_ERR(module_class);
		goto out_unregister_chrdev;
	}

	goto out;
// out_class:
	class_destroy(module_class);
out_unregister_chrdev:
	unregister_chrdev_region(MKDEV(module_major, 0), MODULE_MINORS);
out:
	return err;
}// setup_chdevs_resources

static void cleanup_chdevs_resources(void)
{
	class_destroy(module_class);
	unregister_chrdev_region(MKDEV(module_major, 0), MODULE_MINORS);
}// cleanup_chdevs_resources

int device_init(struct file_operations *fops)
{
	int err = 0;
	err = setup_chdevs_resources();
	if (err) goto out;
	err = setup_module_resources(fops);
	if (err) goto no_module;
	goto out;
no_module:
	cleanup_chdevs_resources();
out:
	return err;
}// setup_resources


static void cleanup_module_resources(void)
{
	// Even no all devices have been allocated we do not accidentlly delete other devices
	device_destroy(module_class, MKDEV(module_major, MODULE_DEV_MINOR));
	// If null it doesn't matter
	cdev_del(&module_cdev);
}// cleanup_module_resources

void device_fini(void)
{
	/* cleanup the control device */
	cleanup_module_resources();
	cleanup_chdevs_resources();
}// cleanup_resources