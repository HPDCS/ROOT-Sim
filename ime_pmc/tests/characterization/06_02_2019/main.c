#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <linux/version.h>

#include "dict.h"

// #define DUMP_ORACLE

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




#define PAGESIZE	4096
#define MEM_PER_ITER	100*PAGESIZE
#define __memory_addr	(1ULL << 46)//(unsigned long long) ((0xbeccaULL << 12) << 12)
#define MIN_ACCESSES	500
#define MAX_ACCESSES	600
#define HOTNESS_PROB	0.1
#define HOTNESS_FACTOR  5.0
#define PAGES 		(MEM_PER_ITER / PAGESIZE)
#define SEED		0xbadf00d;


#define PAGE(addr)	(void *)((unsigned long long)addr & ~(PAGESIZE - 1))

#ifdef DUMP_ORACLE
struct oracle {
	void *page;
	unsigned long count;
};

__thread dict_t *page_count;

static void oracle(unsigned char *addr)
{
	char address[512];
	void *page = PAGE(addr);

	snprintf(address, 512, "%p", page);
	
	struct oracle *or = dict_get(page_count, address);
	if(or == NULL) {
		or = malloc(sizeof(struct oracle));
		or->page = page;
		or->count = 0;
		dict_add(page_count, address, or);
	}

	or->count++;
}

void do_dump_oracle(void *_or)
{
	struct oracle *or = (struct oracle *)_or;
	printf("%p\t%lu\n", or->page, or->count);
}

void dump_oracle(void)
{
	dict_iter(page_count, do_dump_oracle);	
}

#define ORACLE oracle


#else
#define ORACLE(...)
#endif

static __thread unsigned char *memory;
static __thread unsigned long long rnd_seed;

static inline unsigned rng(void)
{
	unsigned long long c = 7319936632422683443ULL;
	unsigned long long x = (rnd_seed += c);

	x ^= x >> 32;
	x *= c;
	x ^= x >> 32;
	x *= c;
	x ^= x >> 32;

	/* Return lower 32bits */
	return x;
}

static void mem_write(unsigned char *ptr, size_t size, int opcount)
{
	size_t i, j, span;

	// start = rng() % size;
	span = 100 + (rng() % (PAGESIZE - 1));


	//if (!size)
	//	return;
	for (i = 0; i < size; i += span) {
		j = (size_t)ptr ^ i;
		ptr[i] = j ^ (j >> 8);
		ORACLE(&ptr[0]);
	}
}

int main(int argc, char **argv)
{
	int i, j, k, iterations, opcount, hot_pages;
	unsigned char *base;
	size_t buflen;
	int flags = MAP_ANONYMOUS | MAP_PRIVATE;
	
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
	flags |= MAP_FIXED_NOREPLACE;
	#else
	flags |= MAP_FIXED;
	#endif

	if(argc != 3) {
		printf("Usage: %s <iterations> <opcount>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	iterations = atoi(argv[1]);	
	opcount = atoi(argv[2]);	
	buflen = iterations * MEM_PER_ITER;

	// TODO: spawn threads here for multithreaded execution

	memory = mmap(
		(void*) __memory_addr, // Change to NULL for multithreaded execution
		buflen, 
		PROT_READ | PROT_WRITE,
		flags,
		-1,
		0);
	
	if (memory == MAP_FAILED) { 
		perror("mmap"); 
		exit(EXIT_FAILURE);
	}

	rnd_seed = SEED;

	#ifdef DUMP_ORACLE
	page_count = dict_new();
	#endif

	int v[MEM_PER_ITER/PAGESIZE];	
	hot_pages = 0;
	for(i = 0; i < PAGES; i++) {
		if (rng() < ((1UL << 32) * HOTNESS_PROB)) {
			v[i] = (rng() % (int)((MAX_ACCESSES - MIN_ACCESSES) * HOTNESS_FACTOR)) + 10 *MIN_ACCESSES * HOTNESS_FACTOR;
		} else {
			v[i] = (rng() % (int)(MAX_ACCESSES - MIN_ACCESSES)) + MIN_ACCESSES;
		}
		//if(rng() % (unsigned long)((1UL << 31) * HOTNESS_PROB)) {
		//	v[hot_pages++] = i;
		//}
	}
	v[hot_pages] = -1;


	/* HOP check in */
	#ifndef DUMP_ORACLE
	struct hop_requester req;

	long time = getMicrotime();
	
	if (request_pt(&req)) {
		printf("Cannot open the HOP CTL \n");
		exit(1);
	}
	#endif
	/* Resume code */	

	base = memory; // + (i * MEM_PER_ITER);

	for(i = 0; i < PAGES; i++) {
		// base = memory + (i * MEM_PER_ITER);

		for(j = 0; j < v[i]; j++) {
			for(k = 0; k < opcount; ++k) {

				asm volatile ("xchgq %rax, %rbx\n"
					      "xchgq %rbx, %rcx\n"
					      "xchgq %rcx, %rcx\n"
					      "xchgq %rbx, %rcx\n"
					      "xchgq %rax, %rbx");

				mem_write(base + (i * PAGESIZE), PAGESIZE, opcount);
			}
		}
	}	

	/* HOP check out */
	#ifndef DUMP_ORACLE
	free_pt(&req);
	printf("ID %u - Execution Time: %lu msec\n", getpid(), (getMicrotime() - time) / 1000);
	#endif
	/* Resume code */

	munmap(memory, buflen);

	#ifdef DUMP_ORACLE
	dump_oracle();
	dict_free(page_count, true);
	#endif
}

