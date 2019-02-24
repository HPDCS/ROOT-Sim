#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "ime-ioctl.h"
#include "intel_pmc_events.h"

#define EXIT	0
#define IOCTL 	1
#define MAX_ID_PMC 7
#define MAX_ID_EVENT 5

int int_from_stdin()
{
	char buffer[12];
	int i = -1;

	if (fgets(buffer, sizeof(buffer), stdin)) {
		sscanf(buffer, "%d", &i);
	}

	return i;
}// int_from_stdin


int ioctl_cmd(int fd)
{
	printf("%d: set LIST_EVENT\n", 0);
	printf("%d: set IME_PROFILER_ON\n", _IOC_NR(IME_PROFILER_ON));
	printf("%d: set IME_PROFILER_OFF\n", _IOC_NR(IME_PROFILER_OFF));
	printf("%d: set IME_PMC_STATS\n", _IOC_NR(IME_PMC_STATS));

	printf("Put cmd >> ");
	int cmd = int_from_stdin();

	int err = 0;
	int var = 0;
	unsigned long uvar = -1;

	if(cmd == 0){
		printf("%d: EVT_INSTRUCTIONS_RETIRED\n", EVT_INSTRUCTIONS_RETIRED);
		printf("%d: EVT_UNHALTED_CORE_CYCLES\n", EVT_UNHALTED_CORE_CYCLES);
		printf("%d: EVT_UNHALTED_REFERENCE_CYCLES\n", EVT_UNHALTED_REFERENCE_CYCLES);
		printf("%d: EVT_BR_INST_RETIRED_ALL_BRANCHES\n", EVT_BR_INST_RETIRED_ALL_BRANCHES);
		printf("%d: EVT_MEM_INST_RETIRED_ALL_LOADS\n", EVT_MEM_INST_RETIRED_ALL_LOADS);
		printf("%d: EVT_MEM_INST_RETIRED_ALL_STORES\n", EVT_MEM_INST_RETIRED_ALL_STORES);
		printf("%d: EVT_MEM_LOAD_RETIRED_L3_HIT\n", EVT_MEM_LOAD_RETIRED_L3_HIT);
		return err;
	}
	printf("Insert PMC id >> ");
	int pmc_id = int_from_stdin();
	if(cmd == _IOC_NR(IME_PROFILER_ON) || cmd == _IOC_NR(IME_PROFILER_OFF)){
		printf("Insert EVENT id >> ");
		int event_id = int_from_stdin();
		struct sampling_spec* output = (struct sampling_spec*) malloc (sizeof(struct sampling_spec));
		output->pmc_id = pmc_id;
		output->event_id = event_id;

		if(output->pmc_id < 0 || output->pmc_id > MAX_ID_PMC){
			printf("IOCTL: IME_PROFILER failed -- invalid PMC id\n");
			return -1;
		}
		if(output->event_id < 0 || output->event_id > MAX_ID_EVENT){
			printf("IOCTL: IME_PROFILER failed -- invalid EVENT id\n");
			return -1;
		}

		if(cmd == _IOC_NR(IME_PROFILER_ON)){
			if ((err = ioctl(fd, IME_PROFILER_ON, output)) < 0){
				printf("IOCTL: IME_PROFILER_ON failed\n");
				return err;
			}
			printf("IOCTL: IME_PROFILER_ON success\n");
		}

		else{
			if ((err = ioctl(fd, IME_PROFILER_OFF, output)) < 0){
				printf("IOCTL: IME_PROFILER_OFF failed\n");
				return err;
			}
			printf("IOCTL: IME_PROFILER_OFF success\nThe resulting value of PMC%d is: %lx", output->pmc_id, output->value);
		}
		return 0;
	}

  if(cmd == _IOC_NR(IME_PMC_STATS)){
		struct pmc_stats* args = (struct pmc_stats*) malloc (sizeof(struct pmc_stats));
		args->pmc_id = pmc_id;
		if ((err = ioctl(fd, IME_PMC_STATS, args)) < 0){
			printf("IOCTL: IME_PMC_STATS failed\n");
			return err;
		}
    printf("IOCTL: IME_PMC_STATS success\nThe resulting value of PMC%d is: %lx", args->pmc_id, args->value);
	}
	return err;
}// ioctl_cmd

const char * device = "/dev/ime/pmc";

int main (int argc, char* argv[])
{

	int fd = open(device, 0666);

	if (fd < 0) {
		printf("Error, cannot open %s\n", device);
		return -1;
	}

	printf("What do you wanna do?\n");
	printf("0) EXIT\n");
	printf("1) IOCTL\n");

	int cmd = int_from_stdin();

	while (cmd)	{
		switch (cmd) {
		case IOCTL :
			if (ioctl_cmd(fd))
				printf("IOCTL ERROR\n");
			break;
		default : 
			fprintf(stderr, "bad cmd\n");
		}

		printf("\n\n NEW REQ \n\n\n");
		printf("0) EXIT\n");
		printf("1) IOCTL\n");
		cmd = int_from_stdin();
	}

    close(fd);
	return 0;
}// main
