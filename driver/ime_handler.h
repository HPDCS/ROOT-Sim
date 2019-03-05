
#define MAX_SAMPLE 256

int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs);

typedef struct
{
    int pmc_id;
	struct pt_regs regs;
    struct fpu fpu_reg;
} pmc_sample_t;
