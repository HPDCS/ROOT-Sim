#define NMI_NAME	"ime"
#define MAX_ID_PMC 4


int enable_nmi(void);

void disable_nmi(void);

void cleanup_pmc(void);

void print_reg(void);
