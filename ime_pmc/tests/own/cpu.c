#include <stdlib.h>     /* exit func */
#include <stdio.h>      /* printf func */
#include <getopt.h>     /* args utility */
#include <pthread.h>	/* pthread utility */

/* HOP CODE START */
#include "hop-ioctl.h"  /* used for hop utility */
#include <unistd.h>
#include <asm-generic/errno-base.h>
#include <sys/ioctl.h>  /* ioctl utility */
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <sys/time.h>

#include <fcntl.h>

/**
 * Returns the current time in microseconds.
 */
long getMicrotime(void)
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}


#define HOP_CTL_DEVICE "/dev/hop/ctl"
#define HOP_DEV_O_MODE 0666
#define HOP_DEV_PATH "/dev/hop"

struct hop_requester {
    int fd;
    int active;
    int tid;
};

int request_pt(struct hop_requester *req)
{
    if (!req) return -EINVAL;

    req->active = 0;
    req->tid = syscall(__NR_gettid);

    printf("Start profiling ThreadID: %u\n", req->tid);

        req->fd = open(HOP_CTL_DEVICE, HOP_DEV_O_MODE);

        if (req->fd < 0) {
                printf("ERROR, cannot open %s, exit\n", HOP_CTL_DEVICE);
                return -ENOENT;
        }

        if (ioctl(req->fd, HOP_ADD_TID, &req->tid)) {
                printf("ERROR, cannot add TID %u for profiling, exit\n", req->tid);
                return -EINTR;
        }

        req->active = 1;
        return 0;
}// request_pt

void free_pt(struct hop_requester *req)
{
    if (!req) {
        printf("WARNING[free_pt], got a null argument, exit\n");
        return;
    }

    if (!req->active) {
        printf("WARNING[free_pt], profiler not active, exit\n");
        return;     
    }

    // if (ioctl(req->fd, HOP_DEL_TID, &req->tid)) {
    //     printf("ERROR, cannot remove TID %u, try manually\n", req->tid);
    // }

        if (req->fd > 0) close(req->fd);

    printf("Stop profiling ThreadID: %u\n", req->tid);
}// free_pt
/* HOP CODE END */

#define PARTITION   0
#define SYNCHRO     1

unsigned active = 1;
unsigned jobType = PARTITION;

int nr_thds;
unsigned long cycles;

static void f_partition(int id, unsigned long tm)
{
    int ab = 5, bc = 7, i = 0;
    printf("%lu\n", tm);
	printf("Working on [%llx]\n", ((unsigned long long) &ab) >> 12);	
    while (--tm > 0 && ++i) {
	    // 5 times
	    ab *= bc;
	    bc *= bc;
	    // bc /= ab;
	    ab *= bc;
	    ab *= ab;
	}
    printf("b ? %u\n", i);
}

int lock = 0;
unsigned long progress;

static void f_synchro(int id)
{
    // work untill progress stops
	while (1) {
		while(!__sync_bool_compare_and_swap(&lock, 0, 1));
		if (progress) {
			progress --;
			// run 'cycles' times
            f_partition(id, cycles);
		}
		while(!__sync_bool_compare_and_swap(&lock, 1, 0));
		if (!progress) break;
	}
}

void *worker_func(void *ptr)
{
	/* HOP check in */
	struct hop_requester req;

	long time = getMicrotime();
	
	if (active && request_pt(&req)) {
		printf("Cannot open the HOP CTL \n");
		exit(1);
	}


	/* job */
	int id = *((int *) ptr);
    
	if (jobType == PARTITION) {
		f_partition(id, cycles);
		printf("1\n");
	} else {
		f_synchro(id);       
		printf("2\n");
	}
	/* HOP check out */
	if (active) free_pt(&req);
	printf("%u %lu\n", id, (getMicrotime() - time) / 1000);	
}// worker_func


#define ARGS "kon:c:p:"

int main(int argc, char **argv)
{

	int i, j, val, option;

    cycles = 10000;
    nr_thds = 1;
    progress = 1;

	option = getopt(argc, argv, ARGS);
        while(option != -1) {
        	switch(option) {
        	case 'o':
        		active = 0;
        		break;
        	case 'n':
	        	val = atoi(optarg);
        		if (val > 0) nr_thds = val;
    			break;
            case 'c':
	        	val = atoi(optarg);
        		if (val > 0) cycles = val;
                break;
            case 'p':
	        	val = atoi(optarg);
        		if (val > 0) progress = val;
                break;
            case 'k' :
                jobType = SYNCHRO;
                break;
    		default:
                goto end;
	       	}
	       	/* get next arg */
	       	option = getopt(argc, argv, ARGS);
        }

        if (jobType == PARTITION)
            cycles = (cycles * progress) / nr_thds;

        printf("#Thds: %u, #Cycs: %u, #Prog: %u | Job: %s - Act: %c\n", 
        	nr_thds, cycles, progress,
            (jobType == PARTITION) ? "PART" : "SYNC",
        	(active) ? 'Y' : 'N');

        pthread_t *thx = malloc(sizeof(pthread_t) * nr_thds);

        if (!thx) goto failure;

        int *y = malloc (sizeof(int) * nr_thds);

        if (!y) goto no_id;

        for (i = 0; i < nr_thds; ++i) {
        	y[i] = i;
        	(void) pthread_create(&(thx[i]), NULL, worker_func, &(y[i]));
        }

		for (i = 0; i < nr_thds; ++i) {
			pthread_join(thx[i], NULL);
		}

        goto end;

wrong:
	printf("SORTING MATRIX ERROR\n");
end:
	free(y);
	free(thx);
    exit(EXIT_SUCCESS);
no_id:
	free(thx);
failure:
	exit(EXIT_FAILURE);
}// main
