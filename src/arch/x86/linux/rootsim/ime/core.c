#include <asm/apic.h>
#include <linux/vmalloc.h>

#include "core.h"
#include "irq.h"
#include "pmu.h"
#include "intel_pmc_events.h"
#include "../rootsim.h" /* idt features */

DEFINE_PER_CPU(struct mem_data, pcpu_mem_data);

static int irq_init(void)
{
	gate_desc *idt = clone_current_idt();
	if (!idt) return -ENOMEM;

	// TODO the irq line choice must be done programmatically

	/* Install toggle handler*/
	install_hook(idt, (unsigned long) pmu_toggle_entry, 240, 3);
	
	/* Install PMI hanlder */
	install_hook(idt, (unsigned long) pmi_entry, 241, 0);

	/* Install the new IDT */
	patch_system_idt(idt, PAGE_SIZE - 1);
	return 0;
}

static void irq_fini(void)
{
	restore_system_idt();
}

static int hw_init(void)
{
	int err;
	unsigned cpu;
	struct pmc_cfg *cfg;

	err = pebs_init();
	if (err) goto out;

	pr_info("PEBS ON\n");

	err = pmc_init();
	if (err) goto pmc_failed;

	pr_info("PMC ON\n");

	/* setup the pmc 0 on each cpu*/
	for_each_present_cpu(cpu) {
		cfg = get_pmc_config(0, cpu);

		cfg->perf_evt_sel = EVT_MEM_INST_RETIRED_ALL_STORES;
		
		cfg->usr = 1;
		cfg->pmi = 0;
		cfg->en = 1; 

		cfg->pebs = 1;
		cfg->counter = ~0x10000;
		cfg->reset = ~0x10000ULL;
	}

	sync_system_pmu_state	();
	pr_info("SYSTEM ON\n");

	goto out;

pmc_failed:
	pebs_fini();
out:
	return err;
}

static void hw_fini(void)
{
	pmc_fini();

	pebs_fini();
}

	// void **buf_poll;
	// unsigned nr_buf;
	// unsigned buf_size;

	// unsigned current;
	// unsigned index;

static int res_init(void)
{
	int err = 0;
	unsigned i;
	unsigned cpu;
	struct mem_data *md;
	for_each_present_cpu(cpu) {
		md = per_cpu_ptr(&pcpu_mem_data, cpu);

		md->buf_poll = (u64 **)vmalloc(NR_BUFFERS);
		if (!md->buf_poll) goto no_buf;

		for (i = 0; i < NR_BUFFERS; ++i) {
			md->buf_poll[i] = (u64 *)vmalloc(BUFFER_SIZE);
			if (!md->buf_poll[i]) goto no_mem;
		}

		md->nr_buf = NR_BUFFERS;
		md->buf_size = BUFFER_SIZE / sizeof(u64);
		md->pos = 0;
		md->index = 0;
		md->read = 0;
	}

	goto out;

no_mem:
	/* Free current cpu residual buffers */
	while (--i > -1) vfree(md->buf_poll[i]);
	vfree(md->buf_poll);
	/* Free previous allocated buffer */
	while(--cpu > -1) {
		md = per_cpu_ptr(&pcpu_mem_data, cpu);
		i = md->nr_buf;
		while (--i > -1) vfree(md->buf_poll[i]);
		vfree(md->buf_poll);
	}
no_buf:	
	err = -ENOMEM;
out: 
	return err;
}

static void res_fini(void)
{
	unsigned i;
	unsigned cpu;
	struct mem_data *md;
	for_each_present_cpu(cpu) {
		md = per_cpu_ptr(&pcpu_mem_data, cpu);
		for (i = 0; i < md->nr_buf; ++i) {
			vfree(md->buf_poll[i]);
		}
	}
}

// TODO manage
int ime_init(void)
{
	int err = 0;

	/* make things clear, save PMU state (future feature) */
	// backup_pmu_system(...);

	/* check PMU support, waiting for implementation */
	err = check_pmu_support();
	if (err) goto out;

	pr_info("NR_PMCS: %u\n", NR_PMCS);

	/* Someone may have configured PMU, reset the configuration */
	disable_all_pmc_system(); // TODO fix call

	/* Create a modified version of the IDT and patch the system */
	err = irq_init();
	if (err) goto out;

	err = hw_init();
	if (err) goto hw_failed;

	err = res_init();
	if (err) goto res_failed;

	pr_info("IME on\n");

	goto out;

res_failed:
	hw_fini();
hw_failed:
	irq_fini();
out:
	return err;
}// ime_init

// TODO manage
void ime_fini(void)
{

	res_fini();

	hw_fini();
	
	irq_fini();
	
	pr_info("IME off\n");
}// ime_exit
