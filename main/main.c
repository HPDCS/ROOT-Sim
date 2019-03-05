#include <sys/ioctl.h>
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

typedef struct{
	int pmc_id[MAX_ID_PMC];
	int event_id[MAX_ID_PMC];
	int cpu_id[MAX_ID_PMC][MAX_CPU]; 
	uint64_t start_value[MAX_ID_PMC]; 
	int enable_PEBS[MAX_ID_PMC];
}configuration_t;

configuration_t current_config;

int int_from_stdin()
{
	char buffer[12];
	int i = -1;
	if (fgets(buffer, sizeof(buffer), stdin)) {
		sscanf(buffer, "%d", &i);
	}
	return i;
}// int_from_stdin

uint64_t uint64_from_stdin()
{
	char buffer[24];
	uint64_t i = -1;
	if (fgets(buffer, sizeof(buffer), stdin)) {
		sscanf(buffer, "%lx", &i);
	}
	return i;
}


int ioctl_cmd(int fd)
{
	printf("%d: LIST_EVENT\n", 0);
	printf("%d: SET_CONFIGURATION\n", 1);
	printf("%d: READ_CONFIGURATION\n", 2);
	printf("%d: IME_PROFILER_ON\n", _IOC_NR(IME_PROFILER_ON));
	printf("%d: IME_PROFILER_OFF\n", _IOC_NR(IME_PROFILER_OFF));
	printf("%d: IME_PMC_STATS\n", _IOC_NR(IME_PMC_STATS));
	printf("%d: IME_READ_BUFFER\n", _IOC_NR(IME_READ_BUFFER));


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

	if(cmd == 1){
		
	}

	if(cmd == 2){
		int i;
		for (i = 0; i < MAX_ID_PMC; i++){
			printf("Put event id for PMC%d (-1 if you don't want use this PMC) >> ", i);
			int event = int_from_stdin();
			if(event != -1){
				current_config.pmc_id[i] = 0;
			}
			else{
				current_config.pmc_id[i] = 1;
				current_config.event_id[i] = event;
			}
		}
		for (i = 0; i < MAX_ID_PMC; i++){
			if(current_config.pmc_id[i] == 1){
				printf("Enable PEBS for PMC%d (1: yes, 0: no) >> ", i);
				int pebs = int_from_stdin();
				if(pebs == 1){
					current_config.enable_PEBS[i] = 1;
				}
				else{
					current_config.enable_PEBS[i] = 0;
				}
			}
		}

		for (i = 0; i < MAX_ID_PMC; i++){
			if(current_config.pmc_id[i] == 1){
				printf("Start value for PMC%d >> ", i);
				current_config.start_value[i] = uint64_from_stdin();
			}
		}
		for (i = 0; i < MAX_ID_PMC; i++){
			if(current_config.pmc_id[i] == 1){
				printf("How many CPU for PMC%d >> ", i);
				int cpu = int_from_stdin();
				if(cpu <= 0){
					current_config.pmc_id[i] = 0;
				}
				else if(cpu >= MAX_CPU){
					int k;
					for(k = 0; k < MAX_CPU; k++){
						current_config.cpu_id[i][k] = 1;
					}
				}
				else{
					int k;
					for(k = 0; k < cpu; k++){
						int cpu_id = -1;
						while(cpu_id < 0 || cpu_id >= MAX_CPU || current_config.cpu_id[i][cpu_id] == 1){
							printf("#%d CPU id >> ", i);
							cpu_id = int_from_stdin();
						}
						current_config.cpu_id[i][cpu_id] = 1;
					}
				}
			}
		}
	}

	/*printf("Insert PMC id >> ");
	int pmc_id = int_from_stdin();
	if(cmd == _IOC_NR(IME_PROFILER_ON) || cmd == _IOC_NR(IME_PROFILER_OFF)){
		struct sampling_spec* output = (struct sampling_spec*) malloc (sizeof(struct sampling_spec));
		output->pmc_id = pmc_id;

		if(output->pmc_id < 0 || output->pmc_id >= MAX_ID_PMC){
			printf("IOCTL: IME_PROFILER failed -- invalid PMC id\n");
			return -1;
		}

		if(cmd == _IOC_NR(IME_PROFILER_ON)){
			printf("Insert EVENT id >> ");
			int event_id = int_from_stdin();
			output->event_id = event_id;
			if(output->event_id < 0 || output->event_id >= MAX_ID_EVENT){
				printf("IOCTL: IME_PROFILER failed -- invalid EVENT id\n");
				return -1;
			}
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
			printf("IOCTL: IME_PROFILER_OFF success\n");
		}
		return 0;
	}

    if(cmd == _IOC_NR(IME_PMC_STATS)){
		int i;
		struct pmc_stats* args = (struct pmc_stats*) malloc (sizeof(struct pmc_stats));
		args->pmc_id = pmc_id;
		if ((err = ioctl(fd, IME_PMC_STATS, args)) < 0){
			printf("IOCTL: IME_PMC_STATS failed\n");
			return err;
		}
    	printf("IOCTL: IME_PMC_STATS success -- PMC%d\n", args->pmc_id);
		for(i = 0; i < numCPU; i++){
			printf("The resulting value of PMC%d on CPU%d is: %lx\n",args->pmc_id, i, args->percpu_value[i]);
		}
	}

	if(cmd == _IOC_NR(IME_READ_BUFFER)){
		int i;
		struct buffer_struct* args = (struct buffer_struct*) malloc (sizeof(struct buffer_struct));
		//args->percpu_value = (unsigned long*) malloc (sizeof(unsigned long)*numCPU);
		if ((err = ioctl(fd, IME_READ_BUFFER, args)) < 0){
			printf("IOCTL: IME_READ_BUFFER failed\n");
			return err;
		}
		printf("IOCTL: IME_READ_BUFFER success\n");

		for(i = 0; i < args->last_index; i++){
			printf("The latency value of index%d is: %lu\n", i, args->buffer_sample[i].lat);
		}
	}
	return err;*/
}// ioctl_cmd

const char * device = "/dev/ime/pmc";

int main (int argc, char* argv[])
{
	int err;
	int fd = open(device, 0666);

	if (fd < 0) {
		printf("Error, cannot open %s\n", device);
		return -1;
	}

	/*printf("What do you wanna do?\n");
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
	}*/

	struct sampling_spec* output = (struct sampling_spec*) malloc (sizeof(struct sampling_spec));
	output->pmc_id = 0;
	output->event_id = 0;
	
	if ((err = ioctl(fd, IME_PROFILER_ON, output)) < 0){
		printf("IOCTL: IME_PROFILER_ON failed\n");
		return err;
	}
	printf("IOCTL: IME_PROFILER_ON success\n");

	int i, k = 0;
	struct pmc_stats* args = (struct pmc_stats*) malloc (sizeof(struct pmc_stats));
	args->pmc_id = 0;
	
	while(k < 4){
		if ((err = ioctl(fd, IME_PMC_STATS, args)) < 0){
			printf("IOCTL: IME_PMC_STATS failed\n");
			return err;
		}
		printf("IOCTL: IME_PMC_STATS success -- PMC%d\n", args->pmc_id);
	
		for(i = 0; i < numCPU; i++){
			printf("The resulting value of PMC%d on CPU%d is: %lx\n",args->pmc_id, i, args->percpu_value[i]);
		}

		sleep(10);
		k++;
	}

	struct buffer_struct* args_buf = (struct buffer_struct*) malloc (sizeof(struct buffer_struct));
	//args_buf->percpu_value = (unsigned long*) malloc (sizeof(unsigned long)*numCPU);
	if ((err = ioctl(fd, IME_READ_BUFFER, args_buf)) < 0){
		printf("IOCTL: IME_READ_BUFFER failed\n");
		return err;
	}
	printf("IOCTL: IME_READ_BUFFER success\n");

	for(i = 0; i < args_buf->last_index; i++){
		printf("The address value of index%d is: %lu\n", i, args_buf->buffer_sample[i].add);
	}
	if ((err = ioctl(fd, IME_PROFILER_OFF, output)) < 0){
		printf("IOCTL: IME_PROFILER_OFF failed\n");
		return err;
	}
	printf("IOCTL: IME_PROFILER_OFF success\n");

  	close(fd);
	return 0;
}// main
