#include <stdlib.h>     /* exit func */
#include <stdio.h>      /* printf func */
#include <getopt.h>     /* args utility */
#include<pthread.h>	/* pthread utility */

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
#include <time.h>

/**
 * Returns the current time in microseconds.
 */
long getMicrotime(void)
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

/* used for open syscall */
// #ifdef  __cplusplus
// #include <fcntl.h>   compiled by g++ 
// #else
// #include <stdlib.h> /* compiled by gcc*/
// #endif

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


static inline unsigned int lfsr_random(int *lfsr_value)
{
	unsigned int bit;

	/* Compute next bit to shift in */
	bit = ((*lfsr_value >> 0) ^
		(*lfsr_value >> 2) ^
		(*lfsr_value >> 3) ^
		(*lfsr_value >> 5)) & 0x0001;

	/* Advance to next register value */
	*lfsr_value = (*lfsr_value >> 1) | (bit << 15);

	return *lfsr_value;
}// lfsr_random

int size;
int cycles;
int nr_thds;
int **matrix;
unsigned active = 1;

void swap(int *array, int i, int j)
{
	int t = array[i];
	array[i] = array[j];
	array[j] = t;
}// swap

void sort(int *array)
{
	int i;
	int ck = 1;

	while(ck)
		for (ck = 0, i = 0; i < size - 1; ++i)
			if (array[i] > array[i + 1]) {
				swap(array, i, i + 1);
				ck = 1;
			}
}// sort

void shuffle(int *array, int *rnd)
{
	int i, r, s; 
	for (i = 0, r = lfsr_random(rnd), s = lfsr_random(rnd);
		i < cycles; ++i, r = lfsr_random(rnd), s = lfsr_random(rnd)) {
		swap(array, r % size, s % size);
	}
}// shuffle

void *worker_func(void *ptr)
{
	/* HOP check in */
	struct hop_requester req;

	long time = getMicrotime();
	if (active) request_pt(&req);

	int id = *((int *) ptr);
	int times = size / nr_thds;
	int i;
	int rnd = 0xF00D;
	for (i = id * times; i < (id + 1) * times; ++i) {
		shuffle(matrix[i], &rnd);
		sort(matrix[i]);
	}

	/* Hop check out */
	if (active) free_pt(&req);
	printf("Execution Time: %lu msec\n", (getMicrotime() - time) / 1000);
}// worker_func


#define ARGS "on:i:c:"

int main(int argc, char **argv)
{

	/* nothing to do */
	if (argc < 7) {
		printf("Need -n #num_threads -i size -c #cycles \n");
		goto end;	
	}

	int i, j, val, option;

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
                case 'i':
	        	val = atoi(optarg);
        		if (val > 0) size = val;
			break;
                case 'c':
	        	val = atoi(optarg);
        		if (val > 0) cycles = val;
			break;
		default:
			goto end;
	       	}
	       	/* get next arg */
	       	option = getopt(argc, argv, ARGS);
        }

        if (size > 16) goto end;
        size = 1U << size; 
        if (size < nr_thds) goto end;

        matrix = malloc(sizeof(int*) * size);
        if (!matrix) goto failure;

        for (i = 0; i < size; ++i) {
        	matrix[i] = malloc(sizeof(int) * size);
        	if (!matrix[i]) goto mem_err;
        }

        for (i = 0; i < size; ++i)
        	for (j = 0; j < size; ++j)
        		matrix[i][j] = (i + j);

        pthread_t *thx = malloc(sizeof(pthread_t) * nr_thds);

        int *y = malloc (sizeof(int) * nr_thds);

        for (i = 0; i < nr_thds; ++i) {
        	y[i] = i;
        	(void) pthread_create(&(thx[i]), NULL, worker_func, &(y[i]));
        }

	for (i = 0; i < nr_thds; ++i) {
		pthread_join(thx[i], NULL);
        }

        for (i = 0; i < size; ++i)
        	for (j = 0; j < size; ++j)
        		if (matrix[i][j] != (i + j))
        			goto wrong;
        goto end;

wrong:
	printf("SORTING MATRIX ERROR\n");
end:
	free(y);
	free(thx);
	for (i = 0; i < size; ++i)
		free (matrix[i]);
	free(matrix);
        exit(EXIT_SUCCESS);
mem_err:
	for (j = 0; j < i; ++j)
		free (matrix[i]);
	free(matrix);
failure:
	exit(EXIT_FAILURE);
}// main