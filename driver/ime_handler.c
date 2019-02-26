#include <asm/apic.h>
#include <asm/apicdef.h>
#include <asm/nmi.h> 		// nmi stuff
#include <asm/cmpxchg.h>	// cmpxchg function
#include <linux/sched.h>	// current macro
#include <linux/percpu.h>	// this_cpu_ptr
#include <linux/slab.h>

#include "ime_handler.h"
extern unsigned long audit_counter;

static inline int handle_ime_event(struct pt_regs *regs)
{
	/* NMI_DONE means that this interrupt is not for this handler */
	audit_counter++;

}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ibs_nmi