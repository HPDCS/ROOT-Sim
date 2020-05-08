#pragma once

/// this must be the same as 1 << B_TOTAL_EXP
#define STATE_SIZE 65536

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stdbool.h>
extern void asm_store(uintptr_t *addr, uintptr_t val);
extern uintptr_t asm_load(uintptr_t *addr, bool is_clean);
#endif
