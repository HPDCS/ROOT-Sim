#define valid_vector(vec)	(vec > 0 && vec < NR_VECTORS)
#define NMI_NAME	"ime"
#define MAX_ID_PMC 3

int enable_pebs_on_system(void);

void disable_pebs_on_system(void);

void cleanup_pmc(void);

void disablePMC0(void* arg);

void enablePMC0(void* arg);

int enable_on_apic(void);

void disable_on_apic(void);
