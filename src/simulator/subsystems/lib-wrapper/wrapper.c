#include <stdlib.h>
#include <string.h>
#include <mm/dymelor.h>
#include <core/timer.h>

// Dummy definitions to make the compiler happy
char *__real_strcpy(char *, const char *);
char *__real_strncpy(char *, const char *, size_t);
char *__real_strcat(char *, const char *);
char *__real_strncat(char *, const char *, size_t);
void *__real_memcpy(void *, const void *, size_t);
void *__real_memmove(void *, const void *, size_t);
void *__real_memset (void *, int, size_t);

/// This variable is used to count the overhead to mark a memory area as updated
extern unsigned long long ticks_counter;


// Actual wrappers
char *__wrap_strcpy (char *s, const char *ct) {
	ticks_counter = CLOCK_READ();
	dirty_mem(s, -1);
	return __real_strcpy(s, ct);
}

char *__wrap_strncpy (char *s, const char *ct, size_t n) {
	ticks_counter = CLOCK_READ();
	dirty_mem((void *)s, n);
	return __real_strncpy(s, ct, n);
}

char *__wrap_strcat (char *s, const char *ct) {
	ticks_counter = CLOCK_READ();
	dirty_mem(s, -1);
	return __real_strcat(s, ct);
}

char *__wrap_strncat (char *s, const char *ct, size_t n) {
	ticks_counter = CLOCK_READ();
	dirty_mem(s, n);
	return __real_strncat(s, ct, n);
}

void *__wrap_memcpy (void *s, const void *ct, size_t n) {
	ticks_counter = CLOCK_READ();
	dirty_mem(s, n);
	return __real_memcpy(s, ct, n);
}

void *__wrap_memmove (void *s, const void *ct, size_t n) {
	ticks_counter = CLOCK_READ();
	dirty_mem(s, n);
	return __real_memmove(s, ct, n);
}

void *__wrap_memset (void *s, int c, size_t n) {
	ticks_counter = CLOCK_READ();
	dirty_mem(s, n);
	return __real_memset(s, c, n);
}

char *__wrap_strdup(const char *s) {
	char *ret = (char *)__wrap_malloc(strlen(s));
	__real_strcpy(ret, s);
	ticks_counter = CLOCK_READ();
	dirty_mem(ret, strlen(s));
	return ret;
}

char *__wrap_strndup(const char *s, size_t n) {
	char *ret = (char *)__wrap_malloc(n);
	__real_strncpy(ret, s, n);
	ticks_counter = CLOCK_READ();
	dirty_mem(ret, n);
	return ret;
}

