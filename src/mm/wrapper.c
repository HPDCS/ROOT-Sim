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
#include <core/init.h>
#include <mm/dymelor.h>

// Actual wrappers
__visible char *__wrap_strcpy(char *s, const char *ct)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, -1);
	return __real_strcpy(s, ct);
}

__visible char *__wrap_strncpy(char *s, const char *ct, size_t n)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, n);
	return __real_strncpy(s, ct, n);
}

__visible char *__wrap_strcat(char *s, const char *ct)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, -1);
	return __real_strcat(s, ct);
}

__visible char *__wrap_strncat(char *s, const char *ct, size_t n)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, n);
	return __real_strncat(s, ct, n);
}

__visible void *__wrap_memcpy(void *s, const void *ct, size_t n)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, n);
	return __real_memcpy(s, ct, n);
}

__visible void *__wrap_memmove(void *s, const void *ct, size_t n)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, n);
	return __real_memmove(s, ct, n);
}

__visible void *__wrap_memset(void *s, int c, size_t n)
{
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)s, n);
	return __real_memset(s, c, n);
}

__visible void __wrap_bzero(void *s, size_t n) {
	__wrap_memset(s, 0, n);
}

__visible char *__wrap_strdup(const char *s)
{
	char *ret = (char *)__wrap_malloc(strlen(s) + 1);
	__real_strcpy(ret, s);
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)ret, strlen(s));
	return ret;
}

__visible char *__wrap_strndup(const char *s, size_t n)
{
	char *ret = (char *)__wrap_malloc(n);
	__real_strncpy(ret, s, n);
	if(rootsim_config.snapshot == SNAPSHOT_INCREMENTAL)
		__write_mem((unsigned char *)ret, n);
	return ret;
}
