// Based on t-test1.c by Wolfram Gloger

#define _GNU_SOURCE

#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define actual_malloc(size) malloc(size)
#define actual_free(ptr) free(ptr)

#define OS_LINUX
#include <mm/mm.h>
#include <core/init.h>

#define N_TOTAL		500
#define N_THREADS	1
#define N_TOTAL_PRINT	50
#define MEMORY		(1ULL << 26)

#define RANDOM(s)	(rng() % (s))

#define MSIZE		10000
#define I_MAX		10000
#define ACTIONS_MAX	30

/* For large allocation sizes, the time required by copying in
   realloc() can dwarf all other execution times.  Avoid this with a
   size threshold. */
#define REALLOC_MAX	2000

struct bin {
	unsigned char *ptr;
	size_t size;
};

struct bin_info {
	struct bin *m;
	size_t size, bins;
};

struct thread_st {
	int bins, max, flags;
	size_t size;
	pthread_t id;
	char *sp;
	size_t seed;
};

pthread_cond_t finish_cond;
pthread_mutex_t finish_mutex;
int n_total = 0;
int n_total_max = N_TOTAL;
int n_running;

simulation_configuration rootsim_config = { 0 };

__thread struct lp_struct *current;
__thread struct lp_struct context;
unsigned int n_prc_tot = N_THREADS;

void rootsim_error(bool fatal, const char *msg, ...)
{
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);
	fflush(stderr);

	if (fatal) {
		exit(EXIT_FAILURE);
	}
}

void *__real_malloc(size_t size)
{
	return actual_malloc(size);
}

void __real_free(void *ptr)
{
	(void)ptr;
	abort();
}

void *__real_realloc(void *ptr, size_t size)
{
	(void)ptr;
	(void)size;
	abort();
}

void *__real_calloc(size_t nmemb, size_t size)
{
	(void)nmemb;
	(void)size;
	abort();
}

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

/*
 * Allocate a bin with malloc(), realloc() or memalign().
 * r must be a random number >= 1024.
 */
static void bin_alloc(struct bin *m, size_t size, unsigned r)
{
	if (mem_check(m->ptr, m->size)) {
		printf("memory corrupt!\n");
		exit(1);
	}
	r %= 1024;

/*	if (r < 4)
	{
		// memalign
		if (m->size > 0) free(m->ptr);
		m->ptr = memalign(sizeof(int) << r, size);
	}
	else*/ if (r < 20) {
		// calloc
		if (m->size > 0)
			__wrap_free(m->ptr);
		m->ptr = __wrap_calloc(size, 1);

		if (zero_check(m->ptr, size)) {
			size_t i;
			for (i = 0; i < size; i++) {
				if (m->ptr[i])
					break;
			}
			printf
			    ("calloc'ed memory non-zero (ptr=%p, i=%ld, nmemb=%zu)!\n", m->ptr, i, size);
			exit(1);
		}

	} else if ((r < 100) && (m->size < REALLOC_MAX)) {
		// realloc
		if (!m->size)
			m->ptr = NULL;
		m->ptr = __wrap_realloc(m->ptr, size);
	} else {
		// malloc
		if (m->size > 0)
			__wrap_free(m->ptr);
		m->ptr = __wrap_malloc(size);
	}
	if (!m->ptr) {
		printf("out of memory (r=%d, size=%ld)!\n", r, (unsigned long)size);
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
		printf("memory corrupt!\n");
		exit(1);
	}

	__wrap_free(m->ptr);
	m->size = 0;
}

static void bin_test(struct bin_info *p)
{
	size_t b;

	for (b = 0; b < p->bins; b++) {
		if (mem_check(p->m[b].ptr, p->m[b].size)) {
			printf("memory corrupt!\n");
			exit(1);
		}
	}
}

static void *malloc_test(void *ptr)
{
	struct thread_st *st = ptr;
	int i, pid = 1;
	unsigned b, j, actions;
	struct bin_info p;

	initialize_memory_map(&context);
	current = &context;

	rnd_seed = st->seed;

	p.m = actual_malloc(st->bins * sizeof(*p.m));
	p.bins = st->bins;
	p.size = st->size;
	for (b = 0; b < p.bins; b++) {
		p.m[b].size = 0;
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
			bin_alloc(&p.m[b], RANDOM(p.size) + 1, rng());
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

	st = __real_malloc(n_thr * sizeof(*st));
	if (!st)
		exit(-1);

	pthread_mutex_lock(&finish_mutex);

	/* Start all n_thr threads. */
	for (i = 0; i < n_thr; i++) {
		st[i].bins = bins;
		st[i].max = i_max;
		st[i].size = size;
		st[i].flags = 0;
		st[i].sp = 0;
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

	for (i = 0; i < n_thr; i++) {
		if (st[i].sp)
			__wrap_free(st[i].sp);
	}
	__wrap_free(st);
	printf("Done.\n");
	return 0;
}
