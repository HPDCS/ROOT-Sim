#include <stdlib.h>     /* exit func */
#include <stdio.h>      /* printf func */
#include <fcntl.h>      /* open syscall */
#include <getopt.h>     /* args utility */
#include <sys/ioctl.h>  /* ioctl syscall*/
#include <unistd.h>	/* close syscall */
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "ime-ioctl.h"
#include "intel_pmc_events.h"

const char * device = "/dev/ime/pmc";
int exit_ = 0;
//pmc mask, cpu mask, event mask, start values, pebs mask, user mask, kernel mask, reset value
#define ARGS "p:c:e:s:b:u:k:d:m:r:"
#define MAX_LEN 256
typedef struct{
	int pmc_id[MAX_ID_PMC];
	int event_id[MAX_ID_PMC];
	int cpu_id[MAX_ID_PMC][MAX_ID_CPU]; 
	uint64_t start_value[MAX_ID_PMC];
	uint64_t reset_value[MAX_ID_PMC]; 
	int enable_PEBS[MAX_ID_PMC][MAX_ID_CPU];
    int user[MAX_ID_PMC][MAX_ID_CPU];
    int kernel[MAX_ID_PMC][MAX_ID_CPU];
    int buffer_module_length;
    int buffer_pebs_length;
}configuration_t;

char binary[16][5] = {"0000", "0001", "0010", "0011", "0100",
 "0101", "0110", "0111", "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"};

char* convert_binary(char c){
    char* res;
    if(c >= '0' && c <= '9') return binary[atoi(&c)];
    if(c >= 'A' && c <= 'F') return binary[c-'A'+10];
    return NULL;
}

int convert_decimal(char c){
    int res;
    if(c >= '0' && c <= '9') return atoi(&c);
    if(c >= 'A' && c <= 'F') return (c-'A'+10);
    return -1;
}

int check_len(char* input){
    int len = strnlen(input, MAX_LEN);
    if(len != MAX_ID_PMC){
        return 0;
    }
    return 1;
}

int int_from_stdin()
{
	char buffer[12];
	int i = -1;
	if (fgets(buffer, sizeof(buffer), stdin)) {
		sscanf(buffer, "%d", &i);
	}
	return i;
}// int_from_stdin

configuration_t current_config;

int ioctl_cmd(int fd)
{
	printf("\n%d: EXIT\n", 0);
	printf("%d: LIST_EVENT\n", 1);
	printf("%d: READ_CONFIGURATION\n", 2);
	printf("%d: IME_SETUP_PMC\n", _IOC_NR(IME_SETUP_PMC));
	printf("%d: IME_PROFILER_ON\n", _IOC_NR(IME_PROFILER_ON));
	printf("%d: IME_PROFILER_OFF\n", _IOC_NR(IME_PROFILER_OFF));
	printf("%d: IME_RESET_PMC\n", _IOC_NR(IME_RESET_PMC));
	printf("%d: IME_PMC_STATS\n", _IOC_NR(IME_PMC_STATS));
	printf("%d: IME_READ_BUFFER\n", _IOC_NR(IME_READ_BUFFER));
	printf("%d: IME_RESET_BUFFER\n", _IOC_NR(IME_RESET_BUFFER));


	printf("Put cmd >> ");
	int cmd = int_from_stdin();

	int err = 0;
	int var = 0;
	unsigned long uvar = -1;

	if(cmd == 0){
		exit_ = 1;
		return err;
	}

	if(cmd == 1){
		printf("%d: EVT_INSTRUCTIONS_RETIRED\n", EVT_INSTRUCTIONS_RETIRED);
		printf("%d: EVT_UNHALTED_CORE_CYCLES\n", EVT_UNHALTED_CORE_CYCLES);
		printf("%d: EVT_UNHALTED_REFERENCE_CYCLES\n", EVT_UNHALTED_REFERENCE_CYCLES);
		printf("%d: EVT_BR_INST_RETIRED_ALL_BRANCHES\n", EVT_BR_INST_RETIRED_ALL_BRANCHES);
		printf("%d: EVT_MEM_INST_RETIRED_ALL_LOADS\n", EVT_MEM_INST_RETIRED_ALL_LOADS);
		printf("%d: EVT_MEM_INST_RETIRED_ALL_STORES\n", EVT_MEM_INST_RETIRED_ALL_STORES);
		printf("%d: EVT_MEM_LOAD_RETIRED_L3_HIT\n", EVT_MEM_LOAD_RETIRED_L3_HIT);
		return err;
	}

	if(cmd == 2){
		int i;
		for (i = 0; i < MAX_ID_PMC; i++){
			if(current_config.pmc_id[i] == 1){
				printf("PMC%d | EVENT:%d", i, current_config.event_id[i]);
				int k;
                printf(" | CPU MASK: ");
				for(k = 0; k < MAX_ID_CPU; k++){
					printf("%d", current_config.cpu_id[i][k]);
				}
                printf(" | PEBS MASK: ");
				for(k = 0; k < MAX_ID_CPU; k++){
					printf("%d", current_config.enable_PEBS[i][k]);
				}
                printf(" | USER MASK: ");
				for(k = 0; k < MAX_ID_CPU; k++){
					printf("%d", current_config.user[i][k]);
				}
                printf(" | KERNEL MASK: ");
				for(k = 0; k < MAX_ID_CPU; k++){
					printf("%d", current_config.kernel[i][k]);
				}
				printf(" | START VALUE: %lx", current_config.start_value[i]);
				printf(" | RESET VALUE: %lx", current_config.reset_value[i]);
                printf(" | PEBS DIM: %d", current_config.buffer_pebs_length);
                printf(" | MODULE DIM: %d\n", current_config.buffer_module_length);
			}
		}
		return 0;
	}

	if(cmd == _IOC_NR(IME_PROFILER_ON)) {
		ioctl(fd, IME_PROFILER_ON);
		printf("IOCTL: IME_PROFILER_ON success\n");
	}

	if(cmd == _IOC_NR(IME_PROFILER_OFF))  {
		ioctl(fd, IME_PROFILER_OFF);
		printf("IOCTL: IME_PROFILER_OFF success\n");
	}
	if(cmd == _IOC_NR(IME_SETUP_PMC) || cmd == _IOC_NR(IME_RESET_PMC)){
		int on = 0;
		if(cmd == _IOC_NR(IME_SETUP_PMC)) on = 1;
		struct sampling_spec* output = (struct sampling_spec*) malloc (sizeof(struct sampling_spec));
		int i, k;
		for (i = 0; i < MAX_ID_PMC; i++){
			if(current_config.pmc_id[i] == 0) continue;
			output->pmc_id = i;
			output->event_id = current_config.event_id[i];
            output->buffer_module_length = current_config.buffer_module_length;
            output->buffer_pebs_length = current_config.buffer_pebs_length;
			for(k = 0; k < MAX_ID_CPU; k++){
				output->enable_PEBS[k] = current_config.enable_PEBS[i][k];
			}
			output->start_value = current_config.start_value[i];
			output->reset_value = current_config.reset_value[i];
			for(k = 0; k < MAX_ID_CPU; k++){
				output->cpu_id[k] = current_config.cpu_id[i][k];
			}
            for(k = 0; k < MAX_ID_CPU; k++){
				output->user[k] = current_config.user[i][k];
			}
            for(k = 0; k < MAX_ID_CPU; k++){
				output->kernel[k] = current_config.kernel[i][k];
			}
			if(on == 1){	
				if ((err = ioctl(fd, IME_SETUP_PMC, output)) < 0){
					printf("IOCTL: IME_SETUP_PMC failed\n");
					return err;
				}
				printf("IOCTL: IME_SETUP_PMC success\n");
			}
			else{
				if ((err = ioctl(fd, IME_RESET_PMC, output)) < 0){
					printf("IOCTL: IME_RESET_PMC failed\n");
					return err;
				}
				printf("IOCTL: IME_RESET_PMC success\n");
			}
		}
		free(output);
		return 0;
	}

   if(cmd == _IOC_NR(IME_PMC_STATS)){
		int i, k;
		for (i = 0; i < MAX_ID_PMC; i++){
			if(current_config.pmc_id[i] == 0) continue;
			struct pmc_stats* args = (struct pmc_stats*) malloc (sizeof(struct pmc_stats));
			args->pmc_id = i;
			if ((err = ioctl(fd, IME_PMC_STATS, args)) < 0){
				printf("IOCTL: IME_PMC_STATS failed\n");
				return err;
			}
			for(k = 0; k < MAX_ID_CPU; k++){
				printf("The resulting value of PMC%d on CPU%d is: %lx\n",i, k, args->percpu_value[k]);
			}
			free(args);
		}
		return 0;
	}

	if(cmd == _IOC_NR(IME_READ_BUFFER)){
		int i;
		struct buffer_struct* args = (struct buffer_struct*) malloc (sizeof(struct buffer_struct));
		args->last_index = MAX_BUFFER_SIZE;
		if ((err = ioctl(fd, IME_READ_BUFFER, args)) < 0){
			printf("IOCTL: IME_READ_BUFFER failed\n");
			return err;
		}
		printf("IOCTL: IME_READ_BUFFER success\n");

		for(i = 0; i < args->last_index; i++){
			printf("The global value of index%d is: %lu\n", i, args->buffer_sample[i].stat);
		}
		free(args);
	}

	if(cmd == _IOC_NR(IME_RESET_BUFFER)){
		if ((err = ioctl(fd, IME_RESET_BUFFER)) < 0){
			printf("IOCTL: IME_RESET_BUFFER failed\n");
			return err;
		}
		printf("IOCTL: IME_RESET_BUFFER success\n");
	}
	return err;
}// ioctl_cmd

int main(int argc, char **argv)
{

	/* nothing to do */
	if (argc < 2) goto open_device;

	int fd, option, err, val;
	char path[128];
    int len, i, k;
    uint64_t sval;
	char delim[] = ",";
	char *ptr;
	
	fd = open(device, 0666);

	if (fd < 0) {
		printf("Error, cannot open %s\n", device);
		return -1;
	}

    err = 0;
    option = getopt(argc, argv, ARGS);


	for(i = 0; i < MAX_ID_PMC; i++){
		current_config.start_value[i] = -1;
		current_config.reset_value[i] = -1;
	}

    while(!err && option != -1) {
        switch(option) {
        case 'p':
            if(!check_len(optarg)) break;

            for(i = 0; i < MAX_ID_PMC; i++){
                if(optarg[i] == '1') current_config.pmc_id[i] = 1;
                else current_config.pmc_id[i] = 0;
            }
			ioctl(fd, IME_RESET_BUFFER);
            break;
        case 'c':
			if(!check_len(optarg)) break;

            for(i = 0; i < MAX_ID_PMC; i++){
                char c = toupper(optarg[i]);
                if(current_config.pmc_id[i] == 1 && convert_binary(c) != NULL){
                    char* cpu = convert_binary(c);
                    for(k = 0; k < MAX_ID_CPU; k++){
                        if(cpu[MAX_ID_CPU-1-k] == '1') current_config.cpu_id[i][k] = 1;
                        else current_config.cpu_id[i][k] = 0;
                    }
                }
            }
            break;
        case 'b':
            if(!check_len(optarg)) break;
            
            for(i = 0; i < MAX_ID_PMC; i++){
                char c = toupper(optarg[i]);
                if(current_config.pmc_id[i] == 1 && convert_binary(c) != NULL){
                    char* cpu = convert_binary(c);
                    for(k = 0; k < MAX_ID_CPU; k++){
                        if(cpu[MAX_ID_CPU-1-k] == '1' && current_config.cpu_id[i][k] == 1) current_config.enable_PEBS[i][k] = 1;
                        else current_config.enable_PEBS[i][k] = 0;
                    }
                }
            }
            break;
        case 'e':
            len = strnlen(optarg, MAX_LEN);
            if(len != (MAX_ID_PMC*2)) break;
            for(i = 0; i < (MAX_ID_PMC*2); i = i+2){
                char c1 = toupper(optarg[i]);
                char c2 = toupper(optarg[i+1]);
                if(current_config.pmc_id[i/2] == 0) continue;
                if((convert_decimal(c1) == -1) || (convert_decimal(c2) == -1)){
                    current_config.pmc_id[i/2] = 0;
                    continue;
                }
                val = convert_decimal(c1)*16 + convert_decimal(c2);
                if(val < 0 || val >= MAX_ID_EVENT){ 
                    current_config.pmc_id[i/2] = 0;
                    continue;
                }
                current_config.event_id[i/2] = val;
            }
            break;
        case 's':
			ptr = strtok(optarg, delim);
			i = 0;
			while(ptr != NULL && i < MAX_ID_PMC){
				sscanf(ptr, "%lx", &sval);
				current_config.start_value[i] = sval;
				ptr = strtok(NULL, delim);
				++i;
			}
			break;
		case 'r':
			printf("OPT: %s\n", optarg);
			ptr = strtok(optarg, delim);
			i = 0;
			while(ptr != NULL && i < MAX_ID_PMC){
				printf("STR: %s\n", ptr);
				sscanf(ptr, "%lx", &sval);
				current_config.reset_value[i] = sval;
				ptr = strtok(NULL, delim);
				++i;
			}
			break;
        case 'u':
            if(!check_len(optarg)) break;
            
            for(i = 0; i < MAX_ID_PMC; i++){
                char c = toupper(optarg[i]);
                if(current_config.pmc_id[i] == 1 && convert_binary(c) != NULL){
                    char* cpu = convert_binary(c);
                    for(k = 0; k < MAX_ID_CPU; k++){
                        if(cpu[MAX_ID_CPU-1-k] == '1' && current_config.cpu_id[i][k] == 1) current_config.user[i][k] = 1;
                        else current_config.user[i][k] = 0;
                    }
                }
            }
            break;
        case 'k':
            if(!check_len(optarg)) break;
            
            for(i = 0; i < MAX_ID_PMC; i++){
                char c = toupper(optarg[i]);
                if(current_config.pmc_id[i] == 1 && convert_binary(c) != NULL){
                    char* cpu = convert_binary(c);
                    for(k = 0; k < MAX_ID_CPU; k++){
                        if(cpu[MAX_ID_CPU-1-k] == '1' && current_config.cpu_id[i][k] == 1) current_config.kernel[i][k] = 1;
                        else current_config.kernel[i][k] = 0;
                    }
                }
            }
            break;
        case 'd':
            val = atoi(optarg);
            current_config.buffer_pebs_length = val;
            break;
        case 'm':
            val = atoi(optarg);
            current_config.buffer_module_length = val;
            break;    
        default:
            goto end;
        }
        /* get next arg */
        option = getopt(argc, argv, ARGS);
    }

open_device:
    fd = open(device, 0666);

	if (fd < 0) {
		printf("Error, cannot open %s\n", device);
		return -1;
	}
end:
	while (1)	{
		if (ioctl_cmd(fd)) printf("IOCTL ERROR\n");
		if(exit_ == 1) break;
	}

  	close(fd);
	return 0;
}// main
