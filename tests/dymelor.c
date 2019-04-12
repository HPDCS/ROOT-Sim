// Based on t-test1.c by Wolfram Gloger

#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>

#define actual_malloc(siz) malloc(siz)
#define actual_free(ptr) free(ptr)

#include <mm/mm.h>
#include <core/init.h>

#include "common.h"

#define N_TOTAL		50
#define N_THREADS	4
#define N_TOTAL_PRINT	5
#define MEMORY		(1ULL << 26)

#define RANDOM(s)	(rng() % (s))

#define MSIZE		10000
#define I_MAX		10000
#define ACTIONS_MAX	30

/* For large allocation sizes, the time required by copying in
   realloc() can dwarf all other execution times.  Avoid this with a
   size threshold. */
#define REALLOC_MAX	2000

enum subsystem {
	NEW,
	DYMELOR = 10,
	BUDDY,
	SLAB
};

struct bin {
	unsigned char *ptr;
	size_t size;
	enum subsystem subs;
};

struct bin_info {
	struct bin *m;
	size_t size, bins;
};

struct thread_st {
	int bins, max, flags;
	size_t size;
	pthread_t id;
	size_t seed;
	int counter;
};

pthread_cond_t finish_cond;
pthread_mutex_t finish_mutex;
int n_total = 0;
int n_total_max = N_TOTAL;
int n_running;


__thread struct thread_st *st;

	/*
 * Ultra-fast RNG: Use a fast hash of integers.
 * 2**64 Period.
 * Passes Diehard and TestU01 at maximum settings
 */
__thread unsigned long long rnd_seed;

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

static void mem_init(unsigned char *ptr, size_t size)
{
	size_t i, j;

	bzero(ptr, size);

	if (!size)
		return;
	for (i = 0; i < size; i += 2047) {
		j = (size_t)ptr ^ i;
		ptr[i] = j ^ (j >> 8);
	}
	j = (size_t)ptr ^ (size - 1);
	ptr[size - 1] = j ^ (j >> 8);
}

static int mem_check(unsigned char *ptr, size_t size)
{
	size_t i, j;

	if (!size)
		return 0;
	for (i = 0; i < size; i += 2047) {
		j = (size_t)ptr ^ i;
		if (ptr[i] != ((j ^ (j >> 8)) & 0xFF))
			return 1;
	}
	j = (size_t)ptr ^ (size - 1);
	if (ptr[size - 1] != ((j ^ (j >> 8)) & 0xFF))
		return 2;
	return 0;
}

static int zero_check(void *p, size_t size)
{
	unsigned *ptr = p;
	unsigned char *ptr2;

	while (size >= sizeof(*ptr)) {
		if (*ptr++)
			return -1;
		size -= sizeof(*ptr);
	}
	ptr2 = (unsigned char *)ptr;

	while (size > 0) {
		if (*ptr2++)
			return -1;
		--size;
	}
	return 0;
}

static void free_it(struct bin *m) {
	if(m->subs == DYMELOR)
		__wrap_free(m->ptr);
	if(m->subs == SLAB)
		slab_free(current->mm->slab, m->ptr);
	if(m->subs == BUDDY)
		free_lp_memory(current, m->ptr);
}

/*
 * Allocate a bin with malloc(), realloc() or memalign().
 * r must be a random number >= 1024.
 */
static void bin_alloc(struct bin *m, size_t size, unsigned r)
{
	if (mem_check(m->ptr, m->size)) {
		printf("[%d] memory corrupt at %p!\n", st->counter, m->ptr);
		abort();
	}

	r %= 1024;

	if (r < 120) {
		// calloc
		if (m->size > 0)
			free_it(m);
		m->ptr = __wrap_calloc(size, 1);
		m->subs = DYMELOR;

		if (zero_check(m->ptr, size)) {
			size_t i;
			for (i = 0; i < size; i++) {
				if (m->ptr[i])
					break;
			}
			printf("[%d] calloc'ed memory non-zero (ptr=%p, i=%ld, nmemb=%zu)!\n", st->counter, m->ptr, i, size);
			exit(1);
		}

	} else if ((r < 200) && (m->size < REALLOC_MAX)) {
		// realloc
		if (!m->size)
			m->ptr = NULL;
		if(m->subs != DYMELOR) {
			free_it(m);
			m->ptr = NULL;
		}
		m->ptr = __wrap_realloc(m->ptr, size);
		m->subs = DYMELOR;
	} /*else if(r < 474) {
		// buddy
		if (m->size > 0)
			free_it(m);
		m->ptr = allocate_lp_memory(current, size);
		m->subs = BUDDY;
	} */else if(r < 749 && size <= SLAB_MSG_SIZE) {
		// slab
		if (m->size > 0)
			free_it(m);
		m->ptr = slab_alloc(current->mm->slab);
		m->subs = SLAB;
	} else {
		// malloc
		if (m->size > 0)
			free_it(m);
		m->ptr = __wrap_malloc(size);
		m->subs = DYMELOR;
	}
	if (!m->ptr) {
		printf("[%d] out of memory (r=%d, size=%ld)!\n", st->counter, r, (unsigned long)size);
		exit(1);
	}

	m->size = size;

	mem_init(m->ptr, m->size);
}

/* Free a bin. */
static void bin_free(struct bin *m)
{
	if (!m->size)
		return;

	if (mem_check(m->ptr, m->size)) {
		printf("[%d] memory corrupt at %p!\n", st->counter, m->ptr);
		abort();
	}

	free_it(m);
	m->size = 0;
}

static void bin_test(struct bin_info *p)
{
	size_t b;

	for (b = 0; b < p->bins; b++) {
		if (mem_check(p->m[b].ptr, p->m[b].size)) {
			printf("[%d] memory corrupt at %p!\n", st->counter, p->m[b].ptr);
			abort();
		}
	}
}

static void *malloc_test(void *ptr)
{
	int i, pid = 1;
	unsigned b, j, actions, action;
	struct bin_info p;

	st = ptr;

	context.gid.to_int = st->counter;
	context.bound = actual_malloc(sizeof(msg_t));
	context.bound->timestamp = 0.0;
	initialize_memory_map(&context);
	current = &context;

	rnd_seed = st->seed;

	p.m = actual_malloc(st->bins * sizeof(*p.m));
	p.bins = st->bins;
	p.size = st->size;
	for (b = 0; b < p.bins; b++) {
		p.m[b].size = 0;
		p.m[b].subs = NEW;
		p.m[b].ptr = NULL;
		if (!RANDOM(2))
			bin_alloc(&p.m[b], RANDOM(p.size) + 1, rng());
	}

	for (i = 0; i <= st->max;) {
		bin_test(&p);
		actions = RANDOM(ACTIONS_MAX);

		for (j = 0; j < actions; j++) {
			b = RANDOM(p.bins);
			bin_free(&p.m[b]);
		}
		i += actions;
		actions = RANDOM(ACTIONS_MAX);

		for (j = 0; j < actions; j++) {
			b = RANDOM(p.bins);
			action = rng();
			bin_alloc(&p.m[b], RANDOM(p.size) + 1, action);
			bin_test(&p);
		}

		i += actions;
	}

	for (b = 0; b < p.bins; b++)
		bin_free(&p.m[b]);

	actual_free(p.m);

	if (pid > 0) {
		pthread_mutex_lock(&finish_mutex);
		st->flags = 1;
		pthread_cond_signal(&finish_cond);
		pthread_mutex_unlock(&finish_mutex);
	}

	finalize_memory_map(&context);
	current = NULL;

	return NULL;
}

static int my_start_thread(struct thread_st *st)
{
	pthread_create(&st->id, NULL, malloc_test, st);
	return 0;
}

static int my_end_thread(struct thread_st *st)
{
	/* Thread st has finished.  Start a new one. */
	if (n_total >= n_total_max) {
		n_running--;
	} else if (st->seed++, my_start_thread(st)) {
		printf("Creating thread #%d failed.\n", n_total);
	} else {
		n_total++;
		if (!(n_total % N_TOTAL_PRINT)) printf("n_total = %d\n", n_total);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int i, bins;
	int n_thr = N_THREADS;
	int i_max = I_MAX;
	size_t size = MSIZE;
	struct thread_st *st;

	n_prc_tot = n_thr;

	segment_init();

	if (argc > 1)
		n_total_max = atoi(argv[1]);
	if (n_total_max < 1)
		n_thr = 1;
	if (argc > 2)
		n_thr = atoi(argv[2]);
	if (n_thr < 1)
		n_thr = 1;
	if (n_thr > 100)
		n_thr = 100;
	if (argc > 3)
		i_max = atoi(argv[3]);

	if (argc > 4)
		size = atol(argv[4]);
	if (size < 2)
		size = 2;

	bins = MEMORY / (size * n_thr);
	if (argc > 5)
		bins = atoi(argv[5]);
	if (bins < 4)
		bins = 4;

	pthread_cond_init(&finish_cond, NULL);
	pthread_mutex_init(&finish_mutex, NULL);

	printf("total=%d threads=%d i_max=%d size=%ld bins=%d\n", n_total_max, n_thr, i_max, size, bins);

	st = actual_malloc(n_thr * sizeof(*st));
	if (!st)
		exit(-1);

	pthread_mutex_lock(&finish_mutex);

	/* Start all n_thr threads. */
	for (i = 0; i < n_thr; i++) {
		st[i].bins = bins;
		st[i].max = i_max;
		st[i].size = size;
		st[i].flags = 0;
		//st[i].sp = 0;
		st[i].counter = i;
		st[i].seed = (i_max * size + i) ^ bins;
		if (my_start_thread(&st[i])) {
			printf("Creating thread #%d failed.\n", i);
			n_thr = i;
			break;
		}
		printf("Created thread %lx.\n", (long)st[i].id);
	}

	for (n_running = n_total = n_thr; n_running > 0;) {

		/* Wait for subthreads to finish. */
		pthread_cond_wait(&finish_cond, &finish_mutex);
		for (i = 0; i < n_thr; i++) {
			if (st[i].flags) {
				pthread_join(st[i].id, NULL);
				st[i].flags = 0;
				my_end_thread(&st[i]);
			}
		}
	}
	pthread_mutex_unlock(&finish_mutex);

	actual_free(st);
	printf("Done.\n");
	return 0;
}
