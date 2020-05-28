#pragma once

#include <datatypes/bitmap.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define B_TOTAL_EXP 16U
#define B_BLOCK_EXP 6U
#define B_LOGS_COUNT 32U

struct memory_map {
	void *base_mem;
	uint_fast8_t current;
	uint_fast32_t current_offset;
	rootsim_bitmap clean[bitmap_required_size(1 << (B_TOTAL_EXP - B_BLOCK_EXP))];
	rootsim_bitmap aggr_written[bitmap_required_size(1 << (B_TOTAL_EXP - B_BLOCK_EXP))];
	uint_least8_t longest[(1 << (B_TOTAL_EXP - B_BLOCK_EXP + 1)) - 1];
};

extern void *__wrap_malloc(size_t req_size);
extern void *__wrap_calloc(size_t nmemb, size_t size);
extern void __wrap_free(void *ptr);

uintptr_t read_mem(void *ptr);
void dirty_mem(void *ptr, uintptr_t value);
