#define NR_ALLOWED_TIDS		255
#define IME_MINORS		(NR_ALLOWED_TIDS + 1)

struct minor {
	int min;
	struct list_head node;
};

int enable_pebs_on_system(void);

void disable_pebs_on_system(void);

int setup_resources(void);

void cleanup_resources(void);

void cleanup_pmc(void);
