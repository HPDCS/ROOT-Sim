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
#include <core/timer.h>
#include <scheduler/scheduler.h>
#include <mm/dymelor.h>

// Definitions to functions which will be wrapped by the linker
char *__real_strcpy(char *, const char *);
char *__real_strncpy(char *, const char *, size_t);
char *__real_strcat(char *, const char *);
char *__real_strncat(char *, const char *, size_t);
void *__real_memcpy(void *, const void *, size_t);
void *__real_memmove(void *, const void *, size_t);
void *__real_memset(void *, int, size_t);

// Actual wrappers
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
