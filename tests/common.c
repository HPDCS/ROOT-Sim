#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#define actual_malloc(siz) malloc(siz)
#define actual_free(ptr) free(ptr)

#include "common.h"

struct lp_struct **lps_blocks;
__thread struct lp_struct *current;
__thread struct lp_struct context;
simulation_configuration rootsim_config = { 0 };
unsigned int n_prc_tot;
unsigned int n_prc;
__thread unsigned int __lp_counter = 0;

void _mkdir(const char *path) {
	(void)path;
}	

void _rootsim_error(bool fatal, const char *msg, ...)
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
	actual_free(ptr);
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
