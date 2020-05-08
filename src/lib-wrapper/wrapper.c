/**
* @file lib-wrapper/wrapper.c
*
* @brief stdlib wrapper
*
* Wrappers of standard library functions which produce side effects on
* the memory map of the process.
*
* @copyright
* Copyright (C) 2008-2019 HPDCS Group
* https://hpdcs.github.io
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @author Alessandro Pellegrini
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <core/timer.h>
#include <core/init.h>
#include <scheduler/scheduler.h>

// Definitions to functions which will be wrapped by the linker
char *__real_strcpy(char *, const char *);
char *__real_strncpy(char *, const char *, size_t);
char *__real_strcat(char *, const char *);
char *__real_strncat(char *, const char *, size_t);
void *__real_memcpy(void *, const void *, size_t);
void *__real_memmove(void *, const void *, size_t);
void *__real_memset(void *, int, size_t);
size_t __real_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int __real_fputc(int c, FILE *stream);
int __real_fputs(const char *s, FILE *stream);
int __real_vfprintf(FILE *stream, const char *format, va_list ap);


/* memory-related wrappers */

char *__wrap_strcpy(char *s, const char *ct)
{
	dirty_mem(s, -1);
	return __real_strcpy(s, ct);
}

char *__wrap_strncpy(char *s, const char *ct, size_t n)
{
	dirty_mem((void *)s, n);
	return __real_strncpy(s, ct, n);
}

char *__wrap_strcat(char *s, const char *ct)
{
	dirty_mem(s, -1);
	return __real_strcat(s, ct);
}

char *__wrap_strncat(char *s, const char *ct, size_t n)
{
	dirty_mem(s, n);
	return __real_strncat(s, ct, n);
}

void *__wrap_memcpy(void *s, const void *ct, size_t n)
{
	dirty_mem(s, n);
	return __real_memcpy(s, ct, n);
}

void *__wrap_memmove(void *s, const void *ct, size_t n)
{
	dirty_mem(s, n);
	return __real_memmove(s, ct, n);
}

void *__wrap_memset(void *s, int c, size_t n)
{
	dirty_mem(s, n);
	return __real_memset(s, c, n);
}

void __wrap_bzero(void *s, size_t n) {
	__wrap_memset(s, 0, n);
}

char *__wrap_strdup(const char *s)
{
	char *ret = (char *)__wrap_malloc(strlen(s) + 1);
	__real_strcpy(ret, s);
	dirty_mem(ret, strlen(s));
	return ret;
}

char *__wrap_strndup(const char *s, size_t n)
{
	char *ret = (char *)__wrap_malloc(n);
	__real_strncpy(ret, s, n);
	dirty_mem(ret, n);
	return ret;
}

/* stdio.h wrappers */

inline int __wrap_vfprintf(FILE *stream, const char *format, va_list ap)
{
	int ret = 0;
	
	if(!rootsim_config.silent_output || stream != stdout)
		ret = __real_vfprintf(stream, format, ap);

	return ret;	
}

int __wrap_vprintf(const char *format, va_list ap)
{
	return __wrap_vfprintf(stdout, format, ap);
}

int __wrap_printf(const char *format, ...)
{
	int ret = 0;
	va_list args;
	va_start(args, format);

	ret = __wrap_vfprintf(stdout, format, args);

	va_end(args);
	return ret;
}

int __wrap_fprintf(FILE *stream, const char *format, ...)
{
	int ret = 0;
	va_list args;
	va_start(args, format);

	ret = __wrap_vfprintf(stream, format, args);

	va_end(args);
	return ret;
}

inline int __wrap_fputs(const char *s, FILE *stream)
{
	int ret = 0;

	if(!rootsim_config.silent_output || stream != stdout)
		ret = __real_fputs(s, stream);

	return ret;
}

int __wrap_puts(const char *s)
{
	int ret = 0;
	
	ret += __wrap_fputs(s, stdout);
	ret += __wrap_fputs("\n", stdout); // puts() writes the string s and a trailing newline to stdout.
	
	return ret;
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if(stream == stdout && rootsim_config.silent_output)
		return 0;
	return __real_fwrite(ptr, size, nmemb, stream);
}

inline int __wrap_fputc(int c, FILE *stream)
{
	int ret = 0;
	
	if(!rootsim_config.silent_output || stream != stdout)
		ret = __real_fputc(c, stream);

	return ret;
}

int __wrap_putc(int c, FILE *stream)
{
	return __wrap_fputc(c, stream);
}

int __wrap_putchar(int c)
{
	return __wrap_fputc(c, stdout);
}
