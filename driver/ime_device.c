#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/cdev.h>

#include "ime_device.h"
#include "ime_fops.h"


#define IME_DEV_MINOR 0
#define IME_MODULE_NAME "ime"
#define IME_DEVICE_NAME	"pmc"
#define MAX_ID_PMC 4

int ime_major = 0;
static spinlock_t list_lock;
static struct cdev ime_cdev;
static struct class *ime_class = NULL;
struct minor *ms;

#define LCK_LIST spin_lock(&list_lock)
#define UCK_LIST spin_unlock(&list_lock)

LIST_HEAD(free_minors);

#define put_minor(mn) \
	LCK_LIST; \
	if (mn) list_add_tail(&(mn->node), &free_minors); \
	UCK_LIST


static const struct file_operations ime_ctl_fops = {
	.owner 			= THIS_MODULE,
	.unlocked_ioctl = ime_ctl_ioctl //if-list function: one for each command
};

static int ime_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}// ime_uevent

static int setup_ime_resources(void)
{
	int err = 0;
	dev_t dev_nb;
	struct device *device;
	struct cdev *cdev = &ime_cdev;

	// dev_t should keep the numbers associated to the device (check it)
	dev_nb = MKDEV(ime_major, IME_DEV_MINOR);

	cdev_init(cdev, &ime_ctl_fops);
	cdev->owner = THIS_MODULE;

	err = cdev_add(cdev, dev_nb, 1);
	if (err) {
		pr_err("MAKE DEVICE ERROR: %d while trying to add %s%d\n", err, IME_DEVICE_NAME, IME_DEV_MINOR);
		return err;
	}

	device = device_create(ime_class, NULL, /* no parent device */ 
	dev_nb, NULL, /* no additional data */
	IME_MODULE_NAME "/%s", IME_DEVICE_NAME);

	pr_devel("MAKE DEVICE Device [%d %d] has been built\n", ime_major, IME_DEV_MINOR);

	if (IS_ERR(device)) {
		pr_err("MAKE DEVICE ERROR: %d while trying to create %s%d\n", err, IME_DEVICE_NAME, IME_DEV_MINOR);
		return PTR_ERR(device);
	}

	return err;
}// setup_ime_resources

static int setup_chdevs_resources(void)
{
	int err = 0;
	dev_t dev = 0;

	int i;

	/* Get a range of minor numbers (starting with 0) to work with */
	err = alloc_chrdev_region(&dev, ime_major, IME_MINORS, IME_DEVICE_NAME);
	if (err < 0)
	{
		pr_err("alloc_chrdev_region() failed\n");        
		goto out;
	}

	ime_major = MAJOR(dev);

	/* Create device class (before allocation of the array of devices) */
	ime_class = class_create(THIS_MODULE, IME_DEVICE_NAME);
	ime_class->dev_uevent = ime_uevent;

	/**
	 * The IS_ERR macro encodes a negative error number into a pointer, 
	 * while the PTR_ERR macro retrieves the error number from the pointer. 
	 * Both macros are defined in include/linux/err.h
	 */
	if (IS_ERR(ime_class))
	{
		pr_err("Class creation failed\n");
		err = PTR_ERR(ime_class);
		goto out_unregister_chrdev;
	}

	ms = vmalloc(NR_ALLOWED_TIDS * sizeof(struct minor));
	if (!ms) {
		err = -ENOMEM;
		goto out_class;
	}

	/* fill the minor list */
	for (i = 1; i < NR_ALLOWED_TIDS; ++i) {
		ms[i].min = i;
		put_minor((&ms[i]));
	}
	goto out;
out_class:
	class_destroy(ime_class);
out_unregister_chrdev:
	unregister_chrdev_region(MKDEV(ime_major, 0), IME_MINORS);
out:
	return err;
}// setup_chdevs_resources

static void cleanup_chdevs_resources(void)
{
	vfree(ms);	/* free minors*/
	class_destroy(ime_class);
	unregister_chrdev_region(MKDEV(ime_major, 0), IME_MINORS);
}// cleanup_chdevs_resources

int setup_resources(void)
{
	int err = 0;
	err = setup_chdevs_resources();
	if (err) goto out;
	err = setup_ime_resources();
	if (err) goto no_ime;
	goto out;
no_ime:
	cleanup_chdevs_resources();
out:
	return err;
}// setup_resources


static void cleanup_ime_resources(void)
{
	// Even no all devices have been allocated we do not accidentlly delete other devices
	device_destroy(ime_class, MKDEV(ime_major, IME_DEV_MINOR));
	// If null it doesn't matter
	cdev_del(&ime_cdev);
}// cleanup_ime_resources

void cleanup_resources(void)
{
	/* cleanup the control device */
	cleanup_ime_resources();
	cleanup_chdevs_resources();
}// cleanup_resources