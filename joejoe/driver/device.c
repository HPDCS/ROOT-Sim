#include <linux/device.h>
#include <linux/cdev.h>

#include "device.h"
#include "fops.h"

struct iso_device {
	struct class *class;
	struct cdev cdev;
	dev_t dev;
	struct device *device;
} iso_device;

static const struct file_operations iso_fops = {
	.open 		= iso_open,
	.owner 		= THIS_MODULE,
	.release 	= iso_release,
	.unlocked_ioctl = iso_ioctl
};

static int iso_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}// ime_uevent

int setup_devices(void)
{
	int err;

	iso_device.class = class_create(THIS_MODULE, ISO_DEVICE_NAME);
	if (IS_ERR(iso_device.class)) {
		pr_err("class_create() failed\n");
		err = PTR_ERR(iso_device.class);
		goto err_class;
	}

	iso_device.class->dev_uevent = iso_uevent;

	err = alloc_chrdev_region(&iso_device.dev, 0, ISO_MAX_MINOR, ISO_DEVICE_NAME);
	if (err < 0) {
		pr_err("alloc_chrdev_region() failed\n");
		goto err_dev;
	}

	cdev_init(&iso_device.cdev, &iso_fops);
	iso_device.cdev.owner = THIS_MODULE;

	err = cdev_add(&iso_device.cdev, iso_device.dev, 1);
	if (err) {
		pr_err("cdev_add() failed\n");
		goto err_add;
	}

	iso_device.device = device_create(iso_device.class, NULL,
		MKDEV(MAJOR(iso_device.dev), 0), NULL,
		ISO_DEVICE_NAME);

	if (IS_ERR(iso_device.device))
		goto err_create;
		
	return 0;

err_create:
	cdev_del(&iso_device.cdev);
err_add:
	unregister_chrdev_region(iso_device.dev, ISO_MAX_MINOR);
err_dev:
	class_destroy(iso_device.class);
err_class:
	return err;
}// setup_resources

void cleanup_devices(void)
{
	device_destroy(iso_device.class, MKDEV(MAJOR(iso_device.dev), 0));
	cdev_del(&iso_device.cdev);
	unregister_chrdev_region(iso_device.dev, ISO_MAX_MINOR);
	class_destroy(iso_device.class);
}// cleanup_resources
