/**
 * @file queues/xxhash.h
 *
 * @brief Fast Hash Algorithm
 *
 * xxHash - Fast Hash algorithm
 *
 * This functions are used to compute hashes of the payload of events
 * when the `--enable-extra-checks` option is specified. This allows
 * to check whether an event handler has modified the content of an
 * event during processing. This is a serious bug in models which is
 * very hard to spot. Indeed, since events are replayed in optimistic
 * simulation, altering the content of an event will lead to undefined
 * simulation results when an event is executed again due to a rollback,
 * silent execution, or state reconstruction fo CCGS.
 *
 * @copyright
 * Copyright (C) 2012-2014, Yann Collet.
 *
 * BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions iqueues/n binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Yann Collet
 *
 * You can contact the author at :
 * - xxHash source repository : http://code.google.com/p/xxhash/
 * - public discussion board : https://groups.google.com/forum/#!forum/lz4c
 */

#pragma once

#ifdef EXTRA_CHECKS

#include <stddef.h>		/* size_t */

typedef enum { XXH_OK = 0, XXH_ERROR } XXH_errorcode;
typedef struct {
	long long ll[6];
} XXH32_state_t;
typedef struct {
	long long ll[11];
} XXH64_state_t;

unsigned int XXH32(const void *input, size_t length, unsigned seed);
unsigned long long XXH64(const void *input, size_t length, unsigned long long seed);

#endif
