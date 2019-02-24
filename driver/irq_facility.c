#include <asm/apicdef.h> 	// Intel apic constants
#include <asm/desc.h>
#include <linux/kallsyms.h>
#include <linux/smp.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>		/* current macro */
#include <linux/percpu.h>		/* this_cpu_ptr */
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/cdev.h>

#include "msr_config.h"
#include "irq_facility.h"
#include "intel_pmc_events.h"
#include "ime_fops.h"


#define valid_vector(vec)	(vec > 0 && vec < NR_VECTORS)
#define IME_DEV_MINOR 0
#define IME_MODULE_NAME "ime"
#define IME_DEVICE_NAME	"pmc"
#define MAX_ID_PMC 7

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
 
static void debugPMU(u64 pmu)
{
	u64 msr;
	rdmsrl(pmu, msr);
	pr_info("PMU %llx: %llx\n", pmu, msr);
}// debugPMU


/**
 * On my machine system_irqs is a global symbol but it is not exported (EXPORT_SYMBOL).
 * This is a problem for the system whne trying to mount the module because it cannot find
 * the symbol and ends up aborting the operation. To bypass this flaw, we use the 
 * kallsyms_lookup_name utility function which does the grunt work in place of us.
 */
static long unsigned int *system_irqs;

static unsigned irq_vector = 0;

static DEFINE_PER_CPU(u64, lvtpc_bkp);
static gate_desc entry_bkp;

extern unsigned long audit_counter;

extern void pebs_entry(void);

static int acquire_vector(void)
{
	// unsigned i = 0;
	if (valid_vector(irq_vector)) goto taken;

	pr_info("system_irqs found at 0x%lx\n", kallsyms_lookup_name("system_vectors"));
	system_irqs = (long unsigned int *) kallsyms_lookup_name("system_vectors");

	// Print all vectors
	// while (i < NR_VECTORS) {
	// 	pr_info("[%u] vector: %lu\n", i, test_bit(i++, system_irqs));	
	// }	
	while (test_and_set_bit(irq_vector, system_irqs)) {
		irq_vector ++;
	}
	if (!valid_vector(irq_vector)) goto busy;
	pr_info("Acquired vector: %u\n", irq_vector);	
	return 0;

busy:
	pr_info("There are no free vectors\n");
	return -1;
taken:
	pr_info("You already taken a vector: %u\n", irq_vector);
	return -2;
}// acquire_vector

static void release_vector(void)
{
	if (!valid_vector(irq_vector)) goto empty;
	if (!test_and_clear_bit(irq_vector, system_irqs)) goto wrong;
	pr_info("released vector: %u\n", irq_vector);
	return;
empty:
	pr_info("No vector has been acquired\n");
	return;
wrong:
	pr_info("Vector %u was selected but not checked as taken\n", irq_vector);
	return;
}// release_vector

/**
 * PEBS works only on PMC0. We need to check that it is not used before enabling the
 * PEBS support in order to avoid conflicts with other softwares.
 */
static int enable_on_apic(void)
{
	u64 msr;
	preempt_disable();

	/* Check if someone else is using the PMU */
	rdmsrl(MSR_IA32_PERFEVTSEL0, msr);
	if (msr & MASK_PERFEVT_EN) goto busy;

	__this_cpu_write(lvtpc_bkp, apic_read(APIC_LVTPC));
	apic_write(APIC_LVTPC, irq_vector);
	
	preempt_enable();	
	return 0;

busy:
	pr_info("[CPU %u] Cannot proceed beacuse the PMC0 is busy\n", smp_processor_id());
	preempt_enable();
	return -1;

}// setup_apic

static void disable_on_apic(void)
{
	preempt_disable();
	wrmsrl(MSR_IA32_PERFEVTSEL0, 0ULL);
	apic_write(APIC_LVTPC, __this_cpu_read(lvtpc_bkp));
	
	pr_info("[CPU %u] Restored PMU state\n", smp_processor_id());
	preempt_enable();
}// disable_on_apic

/**
 * The way we patch the idt is not the best. We need to tackle the SMP system
 * and find a way to patch the idt without locking the whole system for a while.
 * Furthemore, we have to be sure that each cpu idtr is pointing to the 
 * patched idt
 */
static int setup_idt_entry(void)
{
	
	struct desc_ptr idtr;
	gate_desc irq_desc;
	unsigned long cr0;

	/* read the idtr register */
	store_idt(&idtr);

	/* copy the old entry before overwritting it */
	memcpy(&entry_bkp, (void*)(idtr.address + irq_vector * sizeof(gate_desc)), sizeof(gate_desc));
	
	pack_gate(&irq_desc, GATE_INTERRUPT, (unsigned long)pebs_entry, 0, 0, 0);
	
	/* the IDT id read only */
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);

	write_idt_entry((gate_desc*)idtr.address, irq_vector, &irq_desc);
	
	/* restore the Write Protection BIT */
	write_cr0(cr0);	

	return 0;
}// setup_idt_entry

static void restore_idt_entry(void)
{
	struct desc_ptr idtr;
	unsigned long cr0;
	/* read the idtr register */
	store_idt(&idtr);
	/* the IDT id read only */
	cr0 = read_cr0();
	write_cr0(cr0 & ~X86_CR0_WP);
	write_idt_entry((gate_desc*)idtr.address, irq_vector, &entry_bkp);
	/* restore the Write Protection BIT */
	write_cr0(cr0);	
}// restore_idt_entry

void handle_pebs_irq(void)
{
	preempt_disable();
	audit_counter ++;
	preempt_enable();
}// handle_pebs_irq

asm("    .globl pebs_entry\n"
    "pebs_entry:\n"
    "    cld\n"
    "    testq $3,8(%rsp)\n"
    "    jz    1f\n"
    "    swapgs\n"
    "1:\n"
    "    pushq $0\n" /* error code */
    "    pushq %rdi\n"
    "    pushq %rsi\n"
    "    pushq %rdx\n"
    "    pushq %rcx\n"
    "    pushq %rax\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %rbx\n"
    "    pushq %rbp\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "1:  call handle_pebs_irq\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbp\n"
    "    popq %rbx\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rax\n"
    "    popq %rcx\n"
    "    popq %rdx\n"
    "    popq %rsi\n"
    "    popq %rdi\n"
    "    addq $8,%rsp\n" /* error code */
    "    testq $3,8(%rsp)\n"
    "    jz 2f\n"
    "    swapgs\n"
    "2:\n"
    "    iretq");

int enabledPMU(void *dummy)
{
	preempt_disable();
	debugPMU(MSR_IA32_PERF_GLOBAL_CTRL);
	wrmsrl(MSR_IA32_PMC0, 0ULL);
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, BIT(0));
	wrmsrl(MSR_IA32_PERFEVTSEL0, BIT(22) | BIT(16) | EVT_INSTRUCTIONS_RETIRED);
	debugPMU(MSR_IA32_PERFEVTSEL0);
	pr_info("[CPU %u] enabledPMU\n", smp_processor_id());
	preempt_enable();
	return 0;
}

int disablePMU(void *dummy)
{
	preempt_disable();
	debugPMU(MSR_IA32_PMC0);
	wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	wrmsrl(MSR_IA32_PERFEVTSEL0, 0ULL);
	pr_info("[CPU %u] disablePMU\n", smp_processor_id());
	preempt_enable();
	return 0;
}


int enable_pebs_on_system(void)
{
	if (acquire_vector()) goto err;
	if (enable_on_apic()) goto err_apic;
	if (setup_idt_entry()) goto err_entry;
	// preempt_disable();
	// pr_info("[CPU %u] PEBS enabled\n", smp_processor_id());
	// preempt_enable();
	smp_call_on_cpu(0, enabledPMU, NULL, 0);
	return 0;
err_apic:
	release_vector();
err_entry:
	disable_on_apic();
err:
	return -1;
}// enable_pebs_on_system



void disable_pebs_on_system(void)
{
	smp_call_on_cpu(0, disablePMU, NULL, 0);;
	restore_idt_entry();
	disable_on_apic();
	release_vector();
	// preempt_disable();
	// pr_info("[CPU %u] PEBS disabled\n", smp_processor_id());
	// preempt_enable();
}// disable_pebs_on_system

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

void cleanup_pmc(void){
	int pmc_id;
	preempt_disable();
	for(pmc_id = 0; pmc_id < MAX_ID_PMC; pmc_id++){
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
	}
	preempt_enable();
}