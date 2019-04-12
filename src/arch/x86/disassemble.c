/**
 * @file arch/x86/disassemble.c
 *
 * @brief x86 ISA disassembler
 *
 * This is an x86 ISA disassembler. The disassembly (which is table-based)
 * extracts every possible information from an instruction, given a
 * pointer to it.
 *
 * This is a complete disassembler until SSE2 instructions. Newer
 * instructions support is far from complete. Although it has been
 * extensively tested (it has correctly disassembled the Linux kernel and
 * Photoshop), it is extremely possible that some bugs are hidden somewhere.
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
 * @author Davide Cingolani
 * @author Simone Economo
 * @author Fernando Visca
 * @author Alice Porfirio
 *
 * @date September 19, 2008
 */

#if defined(__x86_64__) && defined(HAVE_ECS)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <arch/x86/disassemble.h>

/* Prototipi delle funzioni */

/* 0fxx - escape a due byte */
void esc_0f_opcode(struct disassembly_state *state);

/* Escape al coprocessore */
void d8_opcode(struct disassembly_state *state);
void d9_opcode(struct disassembly_state *state);
void da_opcode(struct disassembly_state *state);
void db_opcode(struct disassembly_state *state);
void dc_opcode(struct disassembly_state *state);
void dd_opcode(struct disassembly_state *state);
void de_opcode(struct disassembly_state *state);
void df_opcode(struct disassembly_state *state);

/* Vari gruppi */
void immed_grp_1(struct disassembly_state *state); /* opcodes 80-83 */
void shift_grp_2(struct disassembly_state *state); /* opcodes C0-C1, D0-D3 */
void unary_grp_3(struct disassembly_state *state); /* opcodes F6-F7 */
void grp_4(struct disassembly_state *state);       /* opcode FE */
void grp_5(struct disassembly_state *state);       /* opcode FF */
void grp_6(struct disassembly_state *state);       /* opcode 0F00 */
void grp_7(struct disassembly_state *state);       /* opcode 0F01 */
void grp_8(struct disassembly_state *state);       /* opcode 0FBA */
void grp_9(struct disassembly_state *state);       /* opcode 0FC7 */
void grp_10(struct disassembly_state *state);      /* opcode 0FB9 */
void grp_11(struct disassembly_state *state);      /* opcodes C6-C7 */
void grp_12(struct disassembly_state *state);      /* opcode 0F71 */
void grp_13(struct disassembly_state *state);      /* opcode 0F72 */
void grp_14(struct disassembly_state *state);      /* opcode 0F73 */
void grp_15(struct disassembly_state *state);      /* opcode 0FAE */
void grp_16(struct disassembly_state *state);      /* opcode OF18 */

/* escape MMX/SSE/SSE2 */
void esc_0f10_17 (struct disassembly_state *state);
void esc_0f28_2f (struct disassembly_state *state);
void esc_0f50_70 (struct disassembly_state *state);
void esc_0f74_76 (struct disassembly_state *state);
void esc_0f7e_7f (struct disassembly_state *state);
void esc_0fc2 (struct disassembly_state *state);
void esc_0fc4_c6 (struct disassembly_state *state);
void esc_0fd1_ef (struct disassembly_state *state);
void esc_0ff1_fe (struct disassembly_state *state);


/* Un array di opcode, dove l'indice è il primo byte dell'opcode */
insn one_byte_opcode_table[] = {
  /* 00 */
  { "add", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* 01 */
  { "add", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* 02 */
  { "add", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 03 */
  { "add", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 04 */
  { "add", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 05 */
  { "add", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 06 */
  { "push", { R_ES, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 07 */
  { "pop", { R_ES, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 08 */
  { "or", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* 09 */
  { "or", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* 0A */
  { "or", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 0B */
  { "or", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 0C */
  { "or", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 0D */
  { "or", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 0E */
  { "push", { R_CS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 0F */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f_opcode, 0 },
  /* 10 */
  { "adc", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 11 */
  { "adc", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 12 */
  { "adc", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 13 */
  { "adc", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 14 */
  { "adc", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 15 */
  { "adc", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 16 */
  { "push", { R_SS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 17 */
  { "pop", { R_SS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 18 */
  { "sbb", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 19 */
  { "sbb", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 1A */
  { "sbb", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 1B */
  { "sbb", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 1C */
  { "sbb", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 1D */
  { "sbb", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 1E */
  { "push", { R_DS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 1F */
  { "pop", { R_DS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 20 */
  { "and", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 21 */
  { "and", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 22 */
  { "and", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 23 */
  { "and", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 24 */
  { "and", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 25 */
  { "and", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 26 */
  { "es", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 27 */
  { "daa", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_ALU },
  /* 28 */
  { "sub", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 29 */
  { "sub", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 2A */
  { "sub", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 2B */
  { "sub", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 2C */
  { "sub", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 2D */
  { "sub", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 2E */
  { "cs", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 2F */
  { "das", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_ALU },
  /* 30 */
  { "xor", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 31 */
  { "xor", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_ALU },
  /* 32 */
  { "xor", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 33 */
  { "xor", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* 34 */
  { "xor", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU },
  /* 35 */
  { "xor", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU },
  /* 36 */
  { "ss", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 37 */
  { "aaa", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_ALU },
  /* 38 */
  { "cmp", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU | I_CTRL },
  /* 39 */
  { "cmp", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU | I_CTRL },
  /* 3A */
  { "cmp", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_ALU | I_CTRL },
  /* 3B */
  { "cmp", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU | I_CTRL },
  /* 3C */
  { "cmp", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_ALU | I_CTRL },
  /* 3D */
  { "cmp", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_ALU | I_CTRL },
  /* 3E */
  { "ds", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3F */
  { "aas", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_ALU },
  /* 40 - La riga 40-4f è per il byte REX */
  { "inc", { R_AX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 41 */
  { "inc", { R_CX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 42 */
  { "inc", { R_DX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 43 */
  { "inc", { R_BX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 44 */
  { "inc", { R_SP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 45 */
  { "inc", { R_BP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 46 */
  { "inc", { R_SI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 47 */
  { "inc", { R_DI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 48 */
  { "dec", { R_AX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 49 */
  { "dec", { R_CX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 4A */
  { "dec", { R_DX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 4B */
  { "dec", { R_BX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 4C */
  { "dec", { R_SP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 4D */
  { "dec", { R_BP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 4E */
  { "dec", { R_SI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 4F */
  { "dec", { R_DI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_ALU },
  /* 50 */
  { "push", { R_AX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 51 */
  { "push", { R_CX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 52 */
  { "push", { R_DX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 53 */
  { "push", { R_BX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 54 */
  { "push", { R_SP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 55 */
  { "push", { R_BP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 56 */
  { "push", { R_SI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 57 */
  { "push", { R_DI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 58 */
  { "pop", { R_AX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 59 */
  { "pop", { R_CX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 5A */
  { "pop", { R_DX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 5B */
  { "pop", { R_BX, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 5C */
  { "pop", { R_SP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 5D */
  { "pop", { R_BP, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 5E */
  { "pop", { R_SI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 5F */
  { "pop", { R_DI, ADDR_0, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 60 */
  { "pusha", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 61 */
  { "popa", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 62 */
  { "bound", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_A, OP_0 }, NULL, I_MEMRD | I_CTRL | I_ALU },
  /* 63 */
  { "arpl", { ADDR_E, ADDR_G, ADDR_0 }, { OP_W, OP_W, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_CTRL | I_ALU },
  /* 64 */
  { "fs", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 65 */
  { "gs", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 66 - cambia la dimensione dell'operando */
  { "opd-size", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 67 - cambia la dimensione dell'operando */
  { "addr-size", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 68 */
  { "push", { ADDR_I, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 69 */
  { "imul", { ADDR_G, ADDR_E, ADDR_I }, { OP_V, OP_V, OP_V }, NULL, I_MEMRD | I_ALU },
  /* 6A */
  { "push", { ADDR_I, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* 6B */
  { "imul", { ADDR_G, ADDR_E, ADDR_I }, { OP_V, OP_V, OP_B }, NULL, I_MEMRD | I_ALU },
  /* 6C - SDM pgg 3-328-330 e 3-519-521 */
  { "insb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_MEMWR | I_STRING },
  /* 6D */
  { "insw", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_MEMWR | I_STRING },
  /* 6E */
  { "outsb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_MEMRD | I_STRING },
  /* 6F */
  { "outsw", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_MEMRD | I_STRING },
  /* 70 */
  { "jo", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 71 */
  { "jno", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 72 - quest'istruzione è jb/jnae/jc ed equivale a jump if carry set */
  { "jc", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 73 - quest'istruzione è jnb/jnae/jnc ed equivale a jump if carry clear */
  { "jnc", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 74 */
  { "jz", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 75 */
  { "jnz", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 76 */
  { "jbe", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 77 */
  { "ja", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 78 */
  { "js", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 79 */
  { "jns", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 7A */
  { "jp", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 7B */
  { "jnp", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 7C */
  { "jl", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 7D */
  { "jge", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 7E */
  { "jle", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 7F */
  { "jg", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* 80 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_B, OP_B, OP_0 }, immed_grp_1, 0 },
  /* 81 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_V, OP_V, OP_0 }, immed_grp_1, 0 },
  /* 82 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_B, OP_B, OP_0 }, immed_grp_1, 0 },
  /* 83 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_V, OP_B, OP_0 }, immed_grp_1, 0 },
  /* 84 */
  { "test", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_CTRL | I_ALU | I_MEMRD },
  /* 85 */
  { "test", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CTRL | I_ALU | I_MEMRD },
  /* 86 */
  { "xchg", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_MEMWR },
  /* 87 */
  { "xchg", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_MEMWR },
  /* 88 */
  { "mov", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMWR },
  /* 89 */
  { "mov", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR },
  /* 8A */
  { "mov", { ADDR_G, ADDR_E, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD },
  /* 8B */
  { "mov", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD },
  /* 8C */
  { "mov", { ADDR_E, ADDR_S, ADDR_0 }, { OP_W, OP_W, OP_0 }, NULL, I_MEMWR },
  /* 8D */
  { "lea", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_MEMIND },
  /* 8E */
  { "mov", { ADDR_S, ADDR_E, ADDR_0 }, { OP_W, OP_W, OP_0 }, NULL, I_MEMRD },
  /* 8F */
  { "pop", { ADDR_E, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_PUSHPOP | I_MEMWR },
  /* 90 */
  { "nop", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 91 */
  { "xchg", { R_AX, R_CX, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 92 */
  { "xchg", { R_AX, R_DX, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 93 */
  { "xchg", { R_AX, R_BX, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 94 */
  { "xchg", { R_AX, R_SP, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 95 */
  { "xchg", { R_AX, R_BP, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 96 */
  { "xchg", { R_AX, R_SI, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 97 */
  { "xchg", { R_AX, R_DI, ADDR_0 }, { OP_E, OP_E, OP_0 }, NULL, 0 },
  /* 98 - cbw/cwde dipendono dall'opsize */
  { "cbw", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 99 - cwd/cdq dipendono dall'opsize */
  { "cwd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 9A */
  { "callf", { ADDR_A, ADDR_0, ADDR_0 }, { OP_P, OP_0, OP_0 }, NULL, I_CALL},
  /* 9B */
  { "fwait", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_FPU | I_CTRL },
  /* 9C */
  { "pushf", { ADDR_F, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_PUSHPOP | I_CTRL },
  /* 9D */
  { "popf", { ADDR_F, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_PUSHPOP | I_CTRL },
  /* 9E */
  { "sahf", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* 9F */
  { "lahf", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* A0 */
  { "mov", { R_AL, ADDR_O, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_MEMRD },
  /* A1 */
  { "mov", { R_AX, ADDR_O, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_MEMRD },
  /* A2 */
  { "mov", { ADDR_O, R_AL, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_MEMWR },
  /* A3 */
  { "mov", { ADDR_O, R_AX, ADDR_0 }, { OP_V, OP_E, OP_0 }, NULL, I_MEMWR },
  /* A4 */
  { "movs", { ADDR_X, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_STRING | I_MEMRD | I_MEMWR },
  /* A5 */
  { "movs", { ADDR_Y, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_STRING | I_MEMRD | I_MEMWR },
  /* A6 */
  { "cmps", { ADDR_X, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_STRING | I_CTRL | I_ALU | I_MEMRD },
  /* A7 */
  { "cmps", { ADDR_Y, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_STRING | I_CTRL | I_ALU | I_MEMRD },
  /* A8 */
  { "test", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, I_CTRL | I_ALU },
  /* A9 */
  { "test", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, I_CTRL | I_ALU},
  /* AA - stosb e stosw sono etichette per stos m8,16,32 */
  { "stos", { ADDR_X, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_STRING | I_MEMWR },
  /* AB - viene presentato come Yv, eAX, ma entrambi sono opzionali */
  { "stos", { ADDR_Y, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_STRING | I_MEMWR },
/*   { "stosw", { ADDR_Y, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, false }, */
  /* AC */
  { "lods", { ADDR_X, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_STRING | I_MEMRD },
  /* AD */
  { "lods", { ADDR_Y, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_STRING | I_MEMRD },
  /* AE */
  { "scas", { ADDR_X, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_STRING | I_MEMRD | I_ALU | I_CTRL },
  /* AF */
  { "scas", { ADDR_Y, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_STRING | I_MEMRD | I_ALU | I_CTRL },
  /* B0 */
  { "mov", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B1 */
  { "mov", { R_CL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B2 */
  { "mov", { R_DL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B3 */
  { "mov", { R_BL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B4 */
  { "mov", { R_AH, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B5 */
  { "mov", { R_CH, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B6 */
  { "mov", { R_DH, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B7 */
  { "mov", { R_BH, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* B8 */
  { "mov", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* B9 */
  { "mov", { R_CX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* BA */
  { "mov", { R_DX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* BB */
  { "mov", { R_BX, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* BC */
  { "mov", { R_SP, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* BD */
  { "mov", { R_BP, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* BE */
  { "mov", { R_SI, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* BF */
  { "mov", { R_DI, ADDR_I, ADDR_0 }, { OP_E, OP_V, OP_0 }, NULL, 0 },
  /* C0 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_B, OP_B, OP_0 }, shift_grp_2, 0 },
  /* C1 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_V, OP_B, OP_0 }, shift_grp_2, 0 },
  /* C2 */
  { "ret", { ADDR_I, ADDR_0, ADDR_0 }, { OP_W, OP_0, OP_0 }, NULL, I_RET},
  /* C3 */
  { "ret", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_RET},
  /* C4 */
  { "les", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_P, OP_0 }, NULL, I_MEMRD },
  /* C5 */
  { "lds", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_P, OP_0 }, NULL, I_MEMRD },
  /* C6 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_B, OP_B, OP_0 }, grp_11, 0 },
  /* C7 */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_V, OP_V, OP_0 }, grp_11, 0 },
  /* C8 */
  { "enter", { ADDR_I, ADDR_I, ADDR_0 }, { OP_W, OP_B, OP_0 }, NULL, 0 },
  /* C9 */
  { "leave", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_RET },
  /* CA */
  { "retf", { ADDR_I, ADDR_0, ADDR_0 }, { OP_W, OP_0, OP_0 }, NULL, I_RET},
  /* CB */
  { "retf", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_RET},
  /* CC */
  { "int\t3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CD */
  { "int", { ADDR_I, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, 0 },
  /* CE */
  { "into", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CF */
  { "iret", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_RET },
  /* D0 */
  { NULL, { ADDR_E, IMMED_1, ADDR_0 }, { OP_B, OP_0, OP_0 }, shift_grp_2, I_MEMWR },
  /* D1 */
  { NULL, { ADDR_E, IMMED_1, ADDR_0 }, { OP_V, OP_0, OP_0 }, shift_grp_2, I_MEMWR },
  /* D2 */
  { NULL, { ADDR_E, R_CL, ADDR_0 }, { OP_B, OP_0, OP_0 }, shift_grp_2, I_MEMWR },
  /* D3 */
  { NULL, { ADDR_E, R_CL, ADDR_0 }, { OP_V, OP_0, OP_0 }, shift_grp_2, I_MEMWR },
  /* D4 */
  { "aam", { ADDR_I, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_ALU },
  /* D5 */
  { "aad", { ADDR_I, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_ALU },
  /* D6 - istruzione non legale */
  { "ill_d6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* D7 */
  { "xlatb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_MEMRD },
  /* D8 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, d8_opcode, 0 },
  /* D9 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, d9_opcode, 0 },
  /* DA */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, da_opcode, 0 },
  /* DB */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, db_opcode, 0 },
  /* DC */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, dc_opcode, 0 },
  /* DD */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, dd_opcode, 0 },
  /* DE */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, de_opcode, 0 },
  /* DF */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, df_opcode, 0 },
  /* E0 */
  { "loopne", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* E1 */
  { "loope", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* E2 */
  { "loop", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* E3 */
  { "jcxz", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP | I_CONDITIONAL },
  /* E4 */
  { "in", { R_AL, ADDR_I, ADDR_0 }, { OP_0, OP_B, OP_0 }, NULL, 0 },
  /* E5 */
  { "in", { R_AX, ADDR_I, ADDR_0 }, { OP_E, OP_B, OP_0 }, NULL, 0 },
  /* E6 */
  { "out", { ADDR_I, R_AL, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, 0 },
  /* E7 */
  { "out", { ADDR_I, R_AX, ADDR_0 }, { OP_B, OP_E, OP_0 }, NULL, 0 },
  /* E8 */
  { "call", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CALL },
  /* E9 - near jmp */
  { "jmp", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_JUMP },
  /* EA - far jmp */
  { "jmp", { ADDR_A, ADDR_0, ADDR_0 }, { OP_P, OP_0, OP_0 }, NULL, I_JUMP },
  /* EB - short jmp */
  { "jmp short", { ADDR_J, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_JUMP },
  /* EC */
  { "in", { R_AL, R_DX, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* ED */
  { "in", { R_AX, R_DX, ADDR_0 }, { OP_E, OP_0, OP_0 }, NULL, 0 },
  /* EE */
  { "out", { R_DX, R_AL, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* EF */
  { "out", { R_DX, R_AX, ADDR_0 }, { OP_0, OP_E, OP_0 }, NULL, 0 },
  /* F0 */
  { "lock", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* F1 */
  { "ill_f1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* F2 */
  { "repne", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* F3 */
  { "repe", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* F4 */
  { "hlt", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* F5 */
  { "cmc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* F6 */
  { NULL, { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, unary_grp_3, 0 },
  /* F7 */
  { NULL, { ADDR_E, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, unary_grp_3, 0 },
  /* F8 */
  { "clc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* F9 */
  { "stc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* FA */
  { "cli", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* FB */
  { "sti", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* FC */
  { "cld", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* FD */
  { "std", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL },
  /* FE - inc/dec */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_4, 0 },
  /* FF - inc/dec */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_5, 0 },
};


/* tabella di escape opcode 0F */
insn esc_0f_opcode_table[] = {
  /* 00 - grp 6 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_6, 0 },
  /* 01 - grp 7 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_7, 0 },
  /* 02 */
  { "lar", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_W, OP_0 }, NULL, I_MEMRD | I_CTRL },	// [FV] Load Access Rights Byte
  /* 03 */
  { "lsl", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_W, OP_0 }, NULL, I_MEMRD | I_CTRL },	// [FV] Load Segment Limit
  /* 04 */
  { "ill_0f04", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 05 */
  { "ill_0f05", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 06 */
  { "clts", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 07 */
  { "ill_0f07", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 08 */
  { "invd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 09 */
  { "wbinvd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 0A */
  { "ill_0f0a", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 0B */
  { "ud2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 0C */
  { "ill_0f0c", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 0D */
  { "ill_0f0d", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 0E */
  { "ill_0f0e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 0F */
  { "ill_0f0f", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 10 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 11 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 12 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 13 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 14 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 15 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 16 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 17 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f10_17, 0 },
  /* 18 - prefetch */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_16, 0 },
  /* 19 */
  { "ill_0f19", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 1A */
  { "ill_0f1a", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 1B */
  { "ill_0f1b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 1C */
  { "ill_0f1c", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 1D */
  { "ill_0f1d", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 1E */
  { "ill_0f1e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 1F */
  { "nop", { ADDR_E, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, 0 },
  /* 20 */
  { "mov", { ADDR_R, ADDR_C, ADDR_0 }, { OP_D, OP_D, OP_0 }, NULL, I_CTRL },
  /* 21 */
  { "mov", { ADDR_R, ADDR_D, ADDR_0 }, { OP_D, OP_D, OP_0 }, NULL, 0 },
  /* 22 */
  { "mov", { ADDR_C, ADDR_R, ADDR_0 }, { OP_D, OP_D, OP_0 }, NULL, I_CTRL },
  /* 23 */
  { "mov", { ADDR_D, ADDR_R, ADDR_0 }, { OP_D, OP_D, OP_0 }, NULL, 0 },
  /* 24 */
  { "mov", { ADDR_R, ADDR_T, ADDR_0 }, { OP_D, OP_D, OP_0 }, NULL, 0 },	// [FV] Viene riportata Illegal
  /* 25 */
  { "ill_0f25", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 26 */
  { "mov", { ADDR_T, ADDR_R, ADDR_0 }, { OP_D, OP_D, OP_0 }, NULL, 0 },	// [FV] Viene riportata Illegal
  /* 27 */
  { "ill_0f27", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 28 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 29 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 2A */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 2B */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 2C */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 2D */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 2E */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 2F */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f28_2f, 0 },
  /* 30 */
  { "wrmsr", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 31 */
  { "rdtsc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 32 */
  { "rdmsr", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 33 */
  { "rdpmc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 34 */
  { "sysenter", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 35 */
  { "sysexit", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 36 */
  { "ill_0f36", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 37 */
  { "ill_0f37", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 38 */
  { "ill_0f38", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 39 */
  { "ill_0f39", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3A */
  { "ill_0f3a", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3B */
  { "ill_0f3b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3C */
  { "ill_0f3c", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3D */
  { "ill_0f3d", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3E */
  { "ill_0f3e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 3F */
  { "ill_0f3f", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* 40 */
  { "cmovo", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 41 */
  { "cmovno", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 42 - cmovb/cmovc/cmovnae */
  { "cmovb", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 43 - cmovae/cmovnb/cmovnc */
  { "cmovnb", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 44 - cmove/cmovz */
  { "cmove", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 45 - cmovne/cmovnz */
  { "cmovne", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 46 - cmovbe/cmovna */
  { "cmovna", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 47 - cmova/cmovnbe */
  { "cmova", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 48 */
  { "cmovs", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 49 */
  { "cmovns", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 4A - cmovp/cmovpe */
  { "cmovp", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 4B - cmovnp/cmovpo */
  { "cmovnp", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 4C - cmovl/cmovnge */
  { "cmovl", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 4D - cmovnl/cmovge */
  { "cmovge", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 4E - cmovle/cmovng */
  { "cmovle", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 4F - cmovnle/cmovg */
  { "cmovg", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_CONDITIONAL | I_MEMRD },
  /* 50 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 51 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 52 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 53 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 54 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 55 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 56 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 57 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 58 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 59 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 5A */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 5B */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 5C */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 5D */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 5E */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 5F */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 60 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 61 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 62 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 63 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 64 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 65 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 66 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 67 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 68 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 69 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 6A */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 6B */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 6C */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 6D */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 6E */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 6F */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 70 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f50_70, 0 },
  /* 71 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_12, 0 },
  /* 72 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_13, 0 },
  /* 73 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_14, 0 },
  /* 74 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f74_76, 0 },
  /* 75 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f74_76, 0 },
  /* 76 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f74_76, 0 },
  /* 77 */
  { "emms", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_CTRL | I_FPU },	// [FV] Empty MMX Technology State
  /* 78 */
  { "mmxud", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },	// [FV] Non gestita
  /* 79 */
  { "mmxud", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },	// [FV] Non gestita
  /* 7A */
  { "mmxud", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },	// [FV] Non gestita
  /* 7B */
  { "mmxud", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },	// [FV] Non gestita
  /* 7C */
  { "mmxud", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },	// [FV] Non gestita
  /* 7D */
  { "mmxud", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },	// [FV] Non gestita
  /* 7E */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f7e_7f, 0 },
  /* 7F */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0f7e_7f, 0 },
  /* 80 */
  { "jo", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 81 */
  { "jno", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 82 - jnae/jb/jc */
  { "jb", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 83 - jae/jnb/jnc */
  { "jae", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 84 - je/jz */
  { "je", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 85 - jne/jnz */
  { "jne", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 86 - jbe/jna */
  { "jbe", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 87 - ja/jnbe */
  { "ja", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 88 */
  { "js", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 89 */
  { "jns", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 8A - jp/jpe */
  { "jp", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 8B - jnp/jpo */
  { "jnp", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 8C - jl/jnge */
  { "jl", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 8D - jnl/jge */
  { "jge", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 8E - jle/jng */
  { "jle", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 8F - jnle/jg */
  { "jg", { ADDR_J, ADDR_0, ADDR_0 }, { OP_V, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_JUMP },
  /* 90 */
  { "seto", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 91 */
  { "setno", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 92 - setb/setc/setnae */
  { "setb", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 93 - setae/setnb/setnc */
  { "setae", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 94 - sete/setz */
  { "sete", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 95 - setne/setnz */
  { "setne", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 96 - setbe/setna */
  { "setbe", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 97 - seta/setnbe */
  { "seta", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 98 */
  { "sets", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 99 */
  { "setns", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 9A - setp/setpe */
  { "setp", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 9B - setnp/setpo */
  { "setnp", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 9C - setl/setnge */
  { "setl", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 9D - setnl/setge */
  { "setge", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 9E - setle/setng */
  { "setle", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* 9F - setnle/setg */
  { "setg", { ADDR_E, ADDR_0, ADDR_0 }, { OP_B, OP_0, OP_0 }, NULL, I_CONDITIONAL | I_MEMWR },
  /* A0 */
  { "push", { R_FS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* A1 */
  { "pop", { R_FS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* A2 */
  { "cpuid", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* A3 */
  { "bt", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_CTRL },
  /* A4 */
  { "shld", { ADDR_E, ADDR_G, ADDR_I }, { OP_V, OP_V, OP_B }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* A5 */
  { "shld", { ADDR_E, ADDR_G, R_CL }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_MEMRD| I_ALU },
  /* A6 */
  { "ill_0fa6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* A7 */
  { "ill_0fa7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* A8 */
  { "push", { R_GS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* A9 */
  { "pop", { R_GS, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, I_PUSHPOP },
  /* AA */
  { "rsm", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* AB */
  { "bts", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_CTRL },
  /* AC */
  { "shrd", { ADDR_E, ADDR_G, ADDR_I }, { OP_V, OP_V, OP_B }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* AD */
  { "shrd", { ADDR_E, ADDR_G, R_CL }, { OP_V, OP_V, OP_0 }, NULL, I_MEMWR | I_MEMRD | I_ALU },
  /* AE */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_15, 0 },
  /* AF */
  { "imul", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_ALU },
  /* B0 */
  { "cmpxchg", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_CTRL | I_CONDITIONAL | I_ALU },
  /* B1 */
  { "cmpxchg", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_CTRL | I_CONDITIONAL | I_ALU },
  /* B2 */
  /* [FV] Mi pare fosse errata, riportava Mp, 00, 00 */
  { "lss", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_P, OP_0 }, NULL, I_MEMRD },
  /* B3 */
  { "btr", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_CTRL },
  /* B4 */
  /* [FV] Mi pare fosse errata, riportava Mp, 00, 00 */
  { "lfs", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_P, OP_0 }, NULL, I_MEMRD },
  /* B5 */
  /* [FV] Mi pare fosse errata, riportava Mp, 00, 00 */
  { "lgs", { ADDR_G, ADDR_M, ADDR_0 }, { OP_V, OP_P, OP_0 }, NULL, I_MEMRD },
  /* B6 */
  { "movzx", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_B, OP_0 }, NULL, I_MEMRD },
  /* B7 */
  { "movzx", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_W, OP_0 }, NULL, I_MEMRD },
  /* B8 */
  { "ill_0fb8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* B9 - è sia UD che group 10... boh! */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_10, 0 },
  /* BA */
  { NULL, { ADDR_E, ADDR_I, ADDR_0 }, { OP_V, OP_B, OP_0 }, grp_8, 0 },
  /* BB */
  { "btc", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_CTRL | I_ALU },
  /* BC */
  { "bsf", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_CTRL | I_ALU },
  /* BD */
  { "bsr", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_CTRL | I_ALU },
  /* BE */
  { "movsx", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_B, OP_0 }, NULL, I_MEMRD },
  /* BF */
  { "movsx", { ADDR_G, ADDR_E, ADDR_0 }, { OP_V, OP_W, OP_0 }, NULL, I_MEMRD },
  /* C0 */
  { "xadd", { ADDR_E, ADDR_G, ADDR_0 }, { OP_B, OP_B, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_ALU },
  /* C1 */
  { "xadd", { ADDR_E, ADDR_G, ADDR_0 }, { OP_V, OP_V, OP_0 }, NULL, I_MEMRD | I_MEMWR | I_ALU },
  /* C2 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fc2, 0 },
  /* C3 */
  /* [FV] Forse era sbagliato, riportava Ed, Gd, 00: inserita OP_Y nella select_operand_size e nella format_addr_g */
  { "movnti", { ADDR_M, ADDR_G, ADDR_0 }, { OP_Y, OP_Y, OP_0 }, NULL, I_MEMWR },
  /* C4 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fc4_c6, 0 },
  /* C5 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fc4_c6, 0 },
  /* C6 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fc4_c6, 0 },
  /* C7 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, grp_9, 0 },
  /* C8 */
  { "bswap", { R_EAX, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* C9 */
  { "bswap", { R_ECX, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CA */
  { "bswap", { R_EDX, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CB */
  { "bswap", { R_EBX, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CC */
  { "bswap", { R_ESP, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CD */
  { "bswap", { R_EBP, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CE */
  { "bswap", { R_ESI, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* CF */
  { "bswap", { R_EDI, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* D0 */
  { "ill_0fd0", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* D1 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D2 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D3 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D4 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D5 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D6 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D7 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D8 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* D9 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* DA */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* DB */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* DC */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* DD */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* DE */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* DF */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E0 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E1 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E2 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E3 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E4 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E5 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E6 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E7 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E8 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* E9 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* EA */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* EB */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* EC */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* ED */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* EE */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* EF */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0fd1_ef, 0 },
  /* F0 */
  { "ill_0ff0", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
  /* F1 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F2 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F3 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F4 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F5 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F6 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F7 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F8 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* F9 */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* FA */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* FB */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* FC */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* FD */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* FE */
  { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, esc_0ff1_fe, 0 },
  /* FF */
  { "ill_0fff", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
};


/* esc_0f_opcode
 * gestisce tutti gli opcode che iniziano con 0x0f
 */
void esc_0f_opcode(struct disassembly_state *state)
{
	unsigned char opcode;
	insn_table table = esc_0f_opcode_table;

	opcode = state->text[state->pos++];

	// Recupera gli operandi
	state->addr[0] = table[opcode].addr_method[0];
	state->addr[1] = table[opcode].addr_method[1];
	state->addr[2] = table[opcode].addr_method[2];

	state->op[0] = table[opcode].operand_type[0];
	state->op[1] = table[opcode].operand_type[1];
	state->op[2] = table[opcode].operand_type[2];

	state->opcode[1] = opcode;

	// Copia il mnemonico
	if(table[opcode].instruction != NULL) {
		strcpy(state->instrument->mnemonic, table[opcode].instruction);
	}

	// Preset some flags
	state->instrument->flags = table[opcode].flags;

	// Controlla se è un escape opcode
	if(table[opcode].instruction == NULL) { // È un escape opcode
		// Controllo di sicurezza
		if(table[opcode].esc_function == NULL) {
			fprintf(stderr, "%s:%d: Errore interno\n", __FILE__, __LINE__);
			abort();
		} else {
			table[opcode].esc_function(state);
		}
	} else { // Istruzione normale
	}
}


/* read_modrm
 * Salva il byte successivo in state->modrm ed incrementa state->pos
 */
inline void read_modrm(struct disassembly_state *state)
{
	state->modrm = state->text[state->pos];
	state->pos++;
	state->read_modrm = true;
}


/* d8_opcode
 * x87 escape.
 */
void d8_opcode(struct disassembly_state *state)
{
	// [FV] Dichiaro i seguenti campi
	char *instructions[4][2] = {{"fadd", "fmul"}, {"fcom", "fcomp"}, {"fsub", "fsubr"}, {"fdiv", "fdivr"}};
	int row, col;
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);

	// [FV] Sono tutte istruzioni I_FPU
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {
		row = (state->modrm >> 4) - 0xC;	// [FV] 0, 1, 2 o 3, a seconda (vedi tabelle manuale)
		col = ((state->modrm & 0x0F) < 0x8) ? 0 : 1;

		/* [FV]
		switch(state->modrm >> 4) {
			case 0xc: // (state->modrm & 0x0f) < 0x8 ? fadd : fmul
			case 0xd: // (state->modrm & 0x0f) < 0x8 ? fcom : fcomp
			case 0xe: // (state->modrm & 0x0f) < 0x8 ? fsub : fsubr
			case 0xf: // (state->modrm & 0x0f) < 0x8 ? fdiv : fdivr
				break;
		}
		*/

		state->addr[0] = floatingPointRegisters[0];
		state->addr[1] = floatingPointRegisters[state->modrm & 0x07]; // [FV] Errore: nell'originale c'era 0x03!

		/* [FV]
		// Selettore registri
		switch(state->modrm & 0x03) { // In una tabella di lookup?
			case 0x0:
				state->addr[1] = R_ST0;
				break;
			case 0x1:
				state->addr[1] = R_ST1;
				break;
			case 0x2:
				state->addr[1] = R_ST2;
				break;
			case 0x3:
				state->addr[1] = R_ST3;
				break;
			case 0x4:
				state->addr[1] = R_ST4;
				break;
			case 0x5:
				state->addr[1] = R_ST5;
				break;
			case 0x6:
				state->addr[1] = R_ST6;
				break;
			case 0x7:
				state->addr[1] = R_ST7;
		}*/
	} else {
		// Il campo nnn del byte ModR/M seleziona l'istruzione
		row = ((state->modrm >> 3) & 0x07) / 2;
		col = ((state->modrm >> 3) & 0x07) % 2;

		switch((state->modrm >> 3) & 0x07) {
			case 0x0: // fadd
				strcpy(state->instrument->mnemonic, "fadd");
				break;
			case 0x1: // fmul
				strcpy(state->instrument->mnemonic, "fmul");
				break;
			case 0x2: // fcom
				strcpy(state->instrument->mnemonic, "fcom");
				break;
			case 0x3: // fcomp
				strcpy(state->instrument->mnemonic, "fcomp");
				break;
			case 0x4: // fsub
				strcpy(state->instrument->mnemonic, "fsub");
				break;
			case 0x5: // fsubr
				strcpy(state->instrument->mnemonic, "fsubr");
				break;
			case 0x6: // fdiv
				strcpy(state->instrument->mnemonic, "fdiv");
				break;
			case 0x7: // fdivr
				strcpy(state->instrument->mnemonic, "fdivr");
				break;
		}

		// L'operando è un float a 32 bit in memoria
		state->addr[0] = ADDR_M;
		state->op[0] = OP_D;
		// [FV] Effettua lettura in memoria
		state->instrument->flags |= I_MEMRD;
	}

	// [FV] Copio il mnemonico
	strcpy(state->instrument->mnemonic, instructions[row][col]);

	if(row == 1) {	// [FV] L'istruzione e' una FCOM od una FCOMP
		state->instrument->flags |= I_CTRL;
		if(col == 1)	// [FV] L'istruzione e' una FCOMP
			state->instrument->flags |= I_PUSHPOP;
	}
}

/* d9_opcode
 * x87 escape.
 */
void d9_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {
		switch(state->modrm >> 4) {
			case 0xc:
				strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fld" : "fxch");
				state->addr[0] = floatingPointRegisters[state->modrm & 0x07];
				if(state->modrm < 0x8C)	// [FV] La FLD effettua push di un registro nello FPU register stack
					state->instrument->flags |= I_PUSHPOP;
				break;

			case 0xd:
				if(state->modrm == 0xd0) {
					strcpy(state->instrument->mnemonic, "fnop");
				} else {
					strcpy(state->instrument->mnemonic, "ill_d9");
					state->instrument->flags &= ~I_FPU;
				}
				break; // TODO: Do we need a break here?!

			case 0xe:
				switch(state->modrm & 0x0f) {
					case 0x0: // fchs
						strcpy(state->instrument->mnemonic, "fchs");
						state->addr[0] = R_ST0;
						break;
					case 0x1: // fabs
						strcpy(state->instrument->mnemonic, "fabs");
						state->addr[0] = R_ST0;
						break;
					case 0x4: // ftst
						strcpy(state->instrument->mnemonic, "fchs");
						state->addr[0] = R_ST0;
						state->instrument->flags |= I_CTRL;
						break;
					case 0x5: // fxam
						strcpy(state->instrument->mnemonic, "fxam");
						state->addr[0] = R_ST0;
						state->instrument->flags |= I_CTRL;
						break;
					case 0x8: // fld1
						strcpy(state->instrument->mnemonic, "fld1");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x9: // fldl2t
						strcpy(state->instrument->mnemonic, "fldl2t");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0xa: // fldl2e
						strcpy(state->instrument->mnemonic, "fldl2e");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0xb: // fldpi
						strcpy(state->instrument->mnemonic, "flpi");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0xc: // fldlg2
						strcpy(state->instrument->mnemonic, "fldlg2");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0xd: // fldln2
						strcpy(state->instrument->mnemonic, "fldln2");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0xe: // fldz
						strcpy(state->instrument->mnemonic, "fldz");
						state->instrument->flags |= I_PUSHPOP;
						break;
					default: // ill_d9
						strcpy(state->instrument->mnemonic, "ill_d9");
						state->instrument->flags &= ~I_FPU;
						break;
				}
				break;

			case 0xf:
				switch(state->modrm & 0X0f) {
					case 0x00: // f2xm1
						strcpy(state->instrument->mnemonic, "f2xm1");
						break;
					case 0x01: // fyl2x
						strcpy(state->instrument->mnemonic, "fyl2x");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x02: // fptan
						strcpy(state->instrument->mnemonic, "fptan");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x03: // fpatan
						strcpy(state->instrument->mnemonic, "fpatan");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x04: // fxtract
						strcpy(state->instrument->mnemonic, "fxtract");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x05: // fprem1
						strcpy(state->instrument->mnemonic, "fprem1");
						break;
					case 0x06: // fdecstp (Decrement Stack-Top Pointer)
						strcpy(state->instrument->mnemonic, "fdecstp");
						state->instrument->flags |= I_CTRL;
						break;
					case 0x07: // fincstp
						strcpy(state->instrument->mnemonic, "fincstp");
						state->instrument->flags |= I_CTRL;
						break;
					case 0x08: // fprem
						strcpy(state->instrument->mnemonic, "fprem");
						break;
					case 0x09: // fyl2xp1
						strcpy(state->instrument->mnemonic, "fyl2xp1");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x0a: // fsqrt
						strcpy(state->instrument->mnemonic, "fsqrt");
						break;
					case 0x0b: // fsincos
						strcpy(state->instrument->mnemonic, "fsincos");
						state->instrument->flags |= I_PUSHPOP;
						break;
					case 0x0c: // frndint
						strcpy(state->instrument->mnemonic, "frndint");
						break;
					case 0x0d: // fscale
						strcpy(state->instrument->mnemonic, "fscale");
						break;
					case 0x0e: // fsin
						strcpy(state->instrument->mnemonic, "fsin");
						break;
					case 0x0f: // fcos
						strcpy(state->instrument->mnemonic, "fcos");
						break;
				}
				break;
		}
	} else {
		state->addr[0] = ADDR_M;
		// [FV] state->op[0] = OP_B; // Non strettamente corretto, ma funziona!

		// Il campo nnn del byte ModR/M seleziona l'istruzione
		switch((state->modrm >> 3) & 0x07) {

			case 0x0: // fld
				strcpy(state->instrument->mnemonic, "fld");
				state->op[0] = OP_D;
				state->instrument->flags |= I_MEMRD | I_PUSHPOP;
				break;

			case 0x2: // fst
				strcpy(state->instrument->mnemonic, "fst");
				state->op[0] = OP_D;
				state->instrument->flags |= I_MEMWR;
				break;

			case 0x3: // fstp
				strcpy(state->instrument->mnemonic, "fstp");
				state->op[0] = OP_D;
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;

			case 0x4: // fldenv
				strcpy(state->instrument->mnemonic, "fldenv");
				state->op[0] = OP_FS;
				state->instrument->flags |= I_MEMRD;
				break;

			case 0x5: // fldcw
				strcpy(state->instrument->mnemonic, "fldcw");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMRD | I_CTRL;
				break;

			case 0x6: // fstenv
				strcpy(state->instrument->mnemonic, "fstenv");
				state->op[0] = OP_FS;
				state->instrument->flags |= I_MEMWR;
				break;

			case 0x7: // fstcw
				strcpy(state->instrument->mnemonic, "fstcw");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMWR | I_CTRL;
				break;

			default: // ill_d9
				state->addr[0] = ADDR_0;
				state->op[0] = OP_0;
				state->instrument->flags &= ~I_FPU;
		}
	}
}

/* da_opcode
 * x87 escape.
 */
void da_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {

		if(state->modrm == 0xe9) {
			strcpy(state->instrument->mnemonic, "fucompp");
			state->instrument->flags |= I_CTRL | I_PUSHPOP;
			return;
		}

		switch(state->modrm >> 4) {

			case 0xc:
				strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fcmovb" : "fcmove");
				break;

			case 0xd: // (state->modrm < 0xc8) ? fcmovbe : fcmovu
				strcpy(state->instrument->mnemonic, (state->modrm < 0xd8) ? "fcmovbe" : "fcmovu");
				break;

			default: // ill_da
				strcpy(state->instrument->mnemonic, "ill_da");
				state->instrument->flags &= ~I_FPU;
				return;
		}



		state->instrument->flags |= I_CONDITIONAL; // [FV] Testa EFLAGS
		state->addr[0] = R_ST0;
		state->addr[1] = floatingPointRegisters[state->modrm & 0x07];
	} else {
		// Md
		state->addr[0] = ADDR_M;
		state->op[0] = OP_D;
		state->instrument->flags |= I_MEMRD;

		switch((state->modrm >> 3) & 0x07) {

			case 0x0: // fiadd
				strcpy(state->instrument->mnemonic, "fiadd");
				break;

			case 0x1: // fimul
				strcpy(state->instrument->mnemonic, "fimul");
				break;

			case 0x2: // ficom
				strcpy(state->instrument->mnemonic, "ficom");
				state->instrument->flags |= I_CTRL;
				break;

			case 0x3: // ficomp
				strcpy(state->instrument->mnemonic, "ficomp");
				state->instrument->flags |= I_CTRL | I_PUSHPOP;
				break;

			case 0x4: // fisub
				strcpy(state->instrument->mnemonic, "fisub");
				break;

			case 0x5: // fisubr
				strcpy(state->instrument->mnemonic, "fisubr");
				break;

			case 0x6: // fidiv
				strcpy(state->instrument->mnemonic, "fidiv");
				break;

			case 0x7: // fidivr
				strcpy(state->instrument->mnemonic, "fidivr");
				break;
		}

	}
}

/* db_opcode
 * x87 escape.
 */
void db_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {
		if(state->modrm == 0xe2) {
			strcpy(state->instrument->mnemonic, "fclex");
			state->instrument->flags |= I_CTRL;
			return;
		} else if(state->modrm == 0xe3) {
			strcpy(state->instrument->mnemonic, "finit");
			state->instrument->flags |= I_CTRL;
			return;
		} else if(state->modrm > 0xf7 || (state->modrm > 0xdf && state->modrm < 0xe8)) {	// [FV] Errore! C'era scritto "< 0xf0"!
			strcpy(state->instrument->mnemonic, "ill_db");
			state->instrument->flags &= ~I_FPU;
		} else {
			state->addr[0] = R_ST0;
			state->addr[1] = floatingPointRegisters[state->modrm && 0x07];

			switch(state->modrm >> 4) {
				case 0xc:
					strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fcmovnb" : "fcmovne");
					state->instrument->flags |= I_CONDITIONAL;
					break;
				case 0xd:
					strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fcmovnbe" : "fcmovnu");
					state->instrument->flags |= I_CONDITIONAL;
					break;
				case 0xe:
					strcpy(state->instrument->mnemonic, "fucomi");
					state->instrument->flags |= I_CTRL;	// [FV] Affects EFLAGS register
					break;
				case 0xf: // fcomi
					strcpy(state->instrument->mnemonic, "fcomi");
					state->instrument->flags |= I_CTRL;	// [FV] Affects EFLAGS register
					break;
			}
		}
	} else {
		state->addr[0] = ADDR_M;

		switch((state->modrm >> 3) & 0x07) {
			case 0x0:
				strcpy(state->instrument->mnemonic, "fild");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMRD | I_PUSHPOP;
				break;
			case 0x1:	// [FV] Questo case mancava!!!
				strcpy(state->instrument->mnemonic, "fisttp");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
			case 0x2:
				strcpy(state->instrument->mnemonic, "fist");
				state->instrument->flags |= I_MEMWR;
				state->op[0] = OP_D;
				break;
			case 0x3:
				strcpy(state->instrument->mnemonic, "fistp");
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				state->op[0] = OP_D;
				break;
			case 0x5:
				strcpy(state->instrument->mnemonic, "fld");
				state->instrument->flags |= I_MEMRD | I_PUSHPOP;
				state->op[0] = OP_M80;	// [FV] Riportava OP_Q...
				break;
			case 0x7:
				strcpy(state->instrument->mnemonic, "fstp");
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				state->op[0] = OP_M80;	// [FV] Riportava OP_Q...
				break;
			default:
				state->addr[0] = ADDR_0;
				strcpy(state->instrument->mnemonic, "ill_db");
				state->instrument->flags &= ~I_FPU;
		}
	}
}

/* dc_opcode
 * x87 escape.
 */
void dc_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {
		if(state->modrm >> 4 == 0xd) {
			strcpy(state->instrument->mnemonic, "ill_dc");
			state->instrument->flags &= ~I_FPU;
		} else {
			state->addr[1] = R_ST0;
			state->addr[0] = floatingPointRegisters[state->modrm & 0x07];

			switch(state->modrm >> 4) {
				case 0xc:
					strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fadd" : "fmul");
					break;
				case 0xe:
					strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fsubr" : "fsub");
					break;
				case 0xf:
					strcpy(state->instrument->mnemonic, (state->modrm < 0xc8) ? "fdivr" : "fdiv");
					break;
			}
		}
	} else {
		state->addr[0] = ADDR_M;
		state->op[0] = OP_Q;
		state->instrument->flags |= I_MEMRD;

		switch((state->modrm >> 3) & 0x07) {
			case 0x00:
				strcpy(state->instrument->mnemonic, "fadd");
				break;
			case 0x01:
				strcpy(state->instrument->mnemonic, "fmul");
				break;
			case 0x02:
				strcpy(state->instrument->mnemonic, "fcom");
				state->instrument->flags |= I_CTRL;
				break;
			case 0x03:
				strcpy(state->instrument->mnemonic, "fcomp");
				state->instrument->flags |= I_CTRL | I_PUSHPOP;
				break;
			case 0x04:
				strcpy(state->instrument->mnemonic, "fsub");
				break;
			case 0x05:
				strcpy(state->instrument->mnemonic, "fsubr");
				break;
			case 0x06:
				strcpy(state->instrument->mnemonic, "fdiv");
				break;
			case 0x07:
				strcpy(state->instrument->mnemonic, "fdivr");
				break;
		}
	}
}

/* dd_opcode
 * x87 escape.
 */
void dd_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {
		if((state->modrm >= 0xc8 && state->modrm <= 0xcf) || state->modrm >= 0xf0) {	// [FV] if(state->modrm < 0xc8 || state->modrm > 0xef) ???
			strcpy(state->instrument->mnemonic, "ill_dd");
			state->instrument->flags &= ~I_FPU;
			return;
		}
		else {
			if(state->modrm > 0xdf && state->modrm < 0xe8)
				state->addr[1] = R_ST0;

			state->addr[0] = floatingPointRegisters[state->modrm & 0x07];

			switch(state->modrm >> 4) {
				case 0xc:
					strcpy(state->instrument->mnemonic, "ffree");
					break;
				case 0xd:
					if(state->modrm < 0xd8) {
						strcpy(state->instrument->mnemonic, "fst");
						// [FV] state->instrument->to_instrument = true; - Perche'?
					} else { // fstp
						strcpy(state->instrument->mnemonic, "fstp");
						// [FV] state->instrument->to_instrument = true; - Perché?
						state->instrument->flags |= I_PUSHPOP;
					}
					break;
				case 0xe:
					if(state->modrm < 0xe8) {
						strcpy(state->instrument->mnemonic, "fucom");
						state->instrument->flags |= I_CTRL;
					} else {
						strcpy(state->instrument->mnemonic, "fucomp");
						state->instrument->flags |= I_PUSHPOP | I_CTRL;
					}
					break;
			}
		}
	} else {
		state->addr[0] = ADDR_M;
		state->op[0] = OP_Q;	// [FV] C'era scritto OP_B...

		switch((state->modrm >> 3) & 0x07) {
			case 0x0: // fld
				strcpy(state->instrument->mnemonic, "fld");
				state->instrument->flags |= I_MEMRD | I_PUSHPOP;
				break;
			case 0x1: // [FV] Questo case mancava!!!
				strcpy(state->instrument->mnemonic, "fisttp");
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
			case 0x2: // fst
				strcpy(state->instrument->mnemonic, "fst");
				state->instrument->flags |= I_MEMWR;
				break;
			case 0x3: // fstp
				strcpy(state->instrument->mnemonic, "fstp");
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
			case 0x4: // frstor
				strcpy(state->instrument->mnemonic, "frstor");
				state->instrument->flags |= I_MEMRD | I_CTRL;
				state->op[0] = OP_FSR;
				break;
			case 0x6: // fsave
				strcpy(state->instrument->mnemonic, "fsave");
				state->instrument->flags |= I_MEMWR | I_CTRL;
				state->op[0] = OP_FSR;
				break;
			case 0x7: // fstsw
				strcpy(state->instrument->mnemonic, "fstsw");
				state->op[0] = OP_W;	// [FV]
				state->instrument->flags |= I_MEMWR | I_CTRL;
				break;
			default: // ill_dd
				strcpy(state->instrument->mnemonic, "ill_dd");
				state->addr[0] = ADDR_0;
				state->op[0] = OP_0;
				state->instrument->flags &= ~I_FPU;
		}
	}
}

/* de_opcode
 * x87 escape.
 */
void de_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	if(state->modrm > 0xbf) {
		state->instrument->flags |= I_PUSHPOP;
		if(state->modrm == 0xd9){ // fcompp
			strcpy(state->instrument->mnemonic, "fcompp");
			state->instrument->flags |= I_CTRL;
		}
		else if(state->modrm >> 4 == 0xd) {
			strcpy(state->instrument->mnemonic, "ill_de");
			state->instrument->flags &= ~I_PUSHPOP & ~I_FPU;
		}
		else {
			state->addr[1] = R_ST0;
			state->addr[0] = floatingPointRegisters[state->modrm & 0x07];

			switch(state->modrm & 0xf8) {
				case 0xc0:
					strcpy(state->instrument->mnemonic, "faddp");
					break;
				case 0xc8:
					strcpy(state->instrument->mnemonic, "fmulp");
					break;
				case 0xe0:
					strcpy(state->instrument->mnemonic, "fsubrp");
					break;
				case 0xe8:
					strcpy(state->instrument->mnemonic, "fsubp");
					break;
				case 0xf0:
					strcpy(state->instrument->mnemonic, "fdivrp");
					break;
				case 0xf8:
					strcpy(state->instrument->mnemonic, "fdivp");
			}
		}
	} else {
		state->addr[0] = ADDR_M;
		state->op[0] = OP_W;
		state->instrument->flags |= I_MEMRD;

		switch((state->modrm >> 3) & 0x07) {
			case 0x00:
				strcpy(state->instrument->mnemonic, "fiadd");
				break;
			case 0x01:
				strcpy(state->instrument->mnemonic, "fimul");
				break;
			case 0x02:
				strcpy(state->instrument->mnemonic, "ficom");
				break;
			case 0x03:
				strcpy(state->instrument->mnemonic, "ficomp");
				state->instrument->flags |= I_PUSHPOP;
				break;
			case 0x04:
				strcpy(state->instrument->mnemonic, "fisub");
				break;
			case 0x05:
				strcpy(state->instrument->mnemonic, "fisubr");
				break;
			case 0x06:
				strcpy(state->instrument->mnemonic, "fidiv");
				break;
			case 0x07:
				strcpy(state->instrument->mnemonic, "fidivr");
				break;
		}
	}
}

/* df_opcode
 * x87 escape.
 */
void df_opcode(struct disassembly_state *state)
{
	enum addr_method floatingPointRegisters[8] = {R_ST0, R_ST1, R_ST2, R_ST3, R_ST4, R_ST5, R_ST6, R_ST7};

	read_modrm(state);
	state->instrument->flags |= I_FPU;

	// TODO: da aggiungere che scrivono in memoria: fisttp
	// [FV] Fatto.

	if(state->modrm > 0xbf) {
		if(state->modrm == 0xe0) {
			strcpy(state->instrument->mnemonic, "fstsw");
			state->addr[0] = R_AX;
			state->instrument->flags |= I_CTRL;
		} else if(state->modrm < 0xe8 || state->modrm > 0xf7) {
			strcpy(state->instrument->mnemonic, "ill_df");
			state->instrument->flags &= ~I_FPU;
		}
		else {
			state->addr[0] = R_ST0;
			state->addr[1] = floatingPointRegisters[state->modrm & 0x07];
			state->instrument->flags |= I_CTRL | I_PUSHPOP;	// [FV] EFALGS modificati
			strcpy(state->instrument->mnemonic, (state->modrm > 0xef) ? "fcomip" : "fucomip");
		}
	} else {
		unsigned char enc = (state->modrm >> 3) & 0x07;
		state->addr[0] = ADDR_M;

		// Scrivono in memoria soltanto fist, fistp, fbstp
		switch(enc) {
			case 0x00:
				strcpy(state->instrument->mnemonic, "fild");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMRD | I_PUSHPOP;
				break;
			case 0x01:
				strcpy(state->instrument->mnemonic, "fisttp");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
			case 0x02: // fist
				strcpy(state->instrument->mnemonic, "fist");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMWR;
				break;
			case 0x03: // fistp
				strcpy(state->instrument->mnemonic, "fistp");
				state->op[0] = OP_W;
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
			case 0x04:
				strcpy(state->instrument->mnemonic, "fbld");
				state->op[0] = OP_M80;
				state->instrument->flags |= I_MEMRD;
				break;
			case 0x05:
				strcpy(state->instrument->mnemonic, "fild");
				state->op[0] = OP_Q;
				state->instrument->flags |= I_MEMRD | I_PUSHPOP;
				break;
			case 0x06: // fbstp
				strcpy(state->instrument->mnemonic, "fbstp");
				state->op[0] = OP_M80;
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
			case 0x07: // fistp
				strcpy(state->instrument->mnemonic, "fistp");
				state->op[0] = OP_Q;
				state->instrument->flags |= I_MEMWR | I_PUSHPOP;
				break;
		}
	}
}

/* Estensioni per gli opcode SSE/SSE2 - per queste istruzioni un terzo byte
 * opzionale specifica l'istruzione ed i suoi operandi. */

/* sse_prefix_to_index
 * Converte un prefisso all'opcode SSE/SSE2 in un indice della tabella degli escape
 * opcode.
 */
int sse_prefix_to_index (unsigned char sse_prefix)
{
	int idx = 0;

	switch(sse_prefix) {
		case 0x00:
			idx = 0;
			break;
		case 0x66:
			idx = 1;
			break;
		case 0xf2:
			idx = 2;
			break;
		case 0xf3:
			idx = 3;
			break;
		default:
			fprintf(stderr, "%s:%d: Unexpected SSE prefix: %d\n", __FILE__, __LINE__, sse_prefix);
	}

	return idx;
}


/* sse_esc
 * Processa gli opcode con gli escape SSE/SSE2. L'istruzione è in:
 * table[state->opcode[1] - base][sse_prefix_to_index (state->sse_prefix)]
 * Vengono riempite tutte le informazioni di interessa in state.
 */
void sse_esc(struct disassembly_state *state, insn table[][4], unsigned char base) {
	insn instruction;

	instruction = (table[state->opcode[1] - base][sse_prefix_to_index(state->sse_prefix)]);

	state->addr[0] = instruction.addr_method[0];
	state->addr[1] = instruction.addr_method[1];
	state->addr[2] = instruction.addr_method[2];

	state->op[0] = instruction.operand_type[0];
	state->op[1] = instruction.operand_type[1];
	state->op[2] = instruction.operand_type[2];

	state->instrument->flags = instruction.flags;

}


/* esc_0f10_17
 * Opcodes da 0f10 a 0f17.
 */
void esc_0f10_17 (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0F10 */
    {
      /* 00 */
      { "movups", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "movupd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "movsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F3 */
      { "movss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }
    },
    /* 0F11 */
    {
      /* 00 */
      { "movups", { ADDR_W, ADDR_V, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMWR | I_SSE | I_XMM },
      /* 66 */
      { "movupd", { ADDR_W, ADDR_V, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMWR | I_SSE2 | I_XMM },
      /* F2 */
      { "movsd", { ADDR_W, ADDR_V, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMWR | I_SSE2 | I_XMM },
      /* F3 */
      { "movss", { ADDR_W, ADDR_V, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMWR | I_SSE | I_XMM }
    },
    /* 0F12 */
    {
      /* 00 - situazione strana */
      { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* 66 */
      { "movlpd", { ADDR_V, ADDR_M, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },	// [FV] Era segnata Vq, Ws?!
      /* F2 */
      { "ill_f20f12", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f20f13", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F13 */
    {
      /* 00 */
      { "movlps", { ADDR_M, ADDR_V, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_SSE | I_XMM },	// [FV] Era segnata Vq, Wq?
      /* 66 */
      { "movlpd", { ADDR_M, ADDR_V, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_SSE2 | I_XMM },	// [FV] Era segnata Vq, Wq?
      /* F2 */
      { "ill_f20f13", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f13", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F14 */
    {
      /* 00 */
      { "unpcklps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "unpcklpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f14", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f14", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F15 */
    {
      /* 00 */
      { "unpckhps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "unpckhpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f15", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f15", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F16 */
    {
      /* 00 - situazione strana */
      { NULL, { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* 66 */
      { "movhpd", { ADDR_V, ADDR_M, ADDR_0 }, { OP_DQ, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE2 },	// [FV] Era riportato Vq, Wq!?
      /* F2 */
      { "ill_f20f16", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f16", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F17 */
    {
      /* 00 */
      { "movhps", { ADDR_M, ADDR_V, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_SSE },	// [FV] Riportava addr[0] = ADDR_W!
      /* 66 */
      { "movhpd", { ADDR_M, ADDR_V, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_SSE2 },	// [FV] Riportava addr[0] = ADDR_W!
      /* F2 */
      { "ill_f20f17", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f17", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    }
  };

	if((state->opcode[1] == 0x12 || state->opcode[1] == 0x16) && state->sse_prefix == 0) {
		unsigned char opcode = state->opcode[1];
		int idx = 0;
		insn tbl[] = {
		  /* 0f12 */
		  /* mem->reg only */
		  // [FV] Riportava Wq, Vq ed era segnata da non instrumentare !?
		  { "movlps", { ADDR_V, ADDR_M, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_SSE },
		  /* reg->reg only */
		  { "movhlps", { ADDR_V, ADDR_V, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, 0 /* [FV] I_SSE3 */ },
		  /* 0f16 */
		  /* mem->reg only */
		  // [FV] Riportava Vq, Wq
		  { "movhps", { ADDR_V, ADDR_M, ADDR_0 }, { OP_DQ, OP_Q, OP_0 }, NULL, I_MEMRD | I_SSE },
		  /* reg->reg only */
		  // [FV] Riportava Vq, Vq
		  { "movlhps", { ADDR_V, ADDR_V, ADDR_0 }, { OP_DQ, OP_Q, OP_0 }, NULL, I_SSE }
		};

		read_modrm(state);

		if(state->modrm >> 6 == 0x3) { // reg->reg
			if(opcode == 0x12)
				idx = 1;
			else
				idx = 3;
		} else { // mem->reg
			if(opcode == 0x12)
				idx = 0;
			else
				idx = 2;
		}

		state->addr[0] = tbl[idx].addr_method[0];
		state->addr[1] = tbl[idx].addr_method[1];
		state->addr[2] = tbl[idx].addr_method[2];

		state->op[0] = tbl[idx].operand_type[0];
		state->op[1] = tbl[idx].operand_type[1];
		state->op[2] = tbl[idx].operand_type[2];

		return;
	}

	sse_esc(state, table, 0x10);
}

void esc_0f28_2f (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0F28 */
    {
      /* 00 */
      { "movaps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "movapd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f28", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f28", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F29 */
    {
      /* 00 */
      { "movaps", { ADDR_W, ADDR_V, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMWR | I_SSE | I_XMM },
      /* 66 */
      { "movapd", { ADDR_W, ADDR_V, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMWR | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f29", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f29", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F2A */
    {
      /* 00 */
      { "cvtpi2ps", { ADDR_V, ADDR_Q, ADDR_0 }, { OP_PS, OP_PI, OP_0 }, NULL, I_MEMRD | I_MMX | I_XMM },	// [FV] Invece di OP_PI riportava OP_Q
      /* 66 */
      { "cvtpi2pd", { ADDR_V, ADDR_Q, ADDR_0 }, { OP_PD, OP_PI, OP_0 }, NULL, I_MEMRD | I_MMX | I_XMM },	// [FV] Riportava Vpd, Qdq!?
      /* F2 */
      { "cvtsi2sd", { ADDR_V, ADDR_E, ADDR_0 }, { OP_SD, OP_Y, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },	// [FV] Riportava Vsd, Ed
      /* F3 */
      { "cvtsi2ss", { ADDR_V, ADDR_E, ADDR_0 }, { OP_SS, OP_Y, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }	// [FV] Riportava Vss, Ed
    },
    /* 0F2B */
    {
      /* 00 */
      { "movntps", { ADDR_M, ADDR_V, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMWR | I_SSE | I_XMM },	// [FV] Era segnato Wps, Vps
      /* 66 */
      { "movntpd", { ADDR_M, ADDR_V, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMWR | I_SSE2 | I_XMM },	// [FV] Era segnato Wpd, Vpd
      /* F2 */
      { "ill_f20f2b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f2b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F2C */
    {
      /* 00 */
      { "cvttps2pi", { ADDR_P, ADDR_W, ADDR_0 }, { OP_PI, OP_PS, OP_0 }, NULL, I_MEMRD | I_MMX | I_XMM },	// [FV] Riportava Qq, Wps
      /* 66 */
      { "cvttpd2pi", { ADDR_P, ADDR_W, ADDR_0 }, { OP_PI, OP_PD, OP_0}, NULL, I_MEMRD | I_MMX | I_XMM },	// [FV] Riportava Qdq, Wpd
      /* F2 */
      { "cvttsd2si", { ADDR_G, ADDR_W, ADDR_0 }, { OP_Y, OP_SD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },	// [FV] Riportava Gd, Wsd
      /* F3 */
      { "cvttss2si", { ADDR_G, ADDR_W, ADDR_0 }, { OP_Y, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }	// [FV] Riportava Gd, Wss
    },
    /* 0F2D */
    {
      /* 00 */
      { "cvtps2pi", { ADDR_P, ADDR_W, ADDR_0 }, { OP_PI, OP_PS, OP_0 }, NULL, I_MEMRD | I_MMX | I_XMM },	// [FV] Riportava Qq, Wps
      /* 66 */
      { "cvtpd2pi", { ADDR_P, ADDR_W, ADDR_0 }, { OP_PI, OP_PD, OP_0}, NULL, I_MEMRD | I_MMX | I_XMM },	// [FV] Riportava Qdq, Wpd (Mi pare sia Ppi ma il manuale riporta Qpi ?!?)
      /* F2 */
      { "cvtsd2si", { ADDR_G, ADDR_W, ADDR_0 }, { OP_Y, OP_SD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },	// [FV] Riportava Gd, Wsd
      /* F3 */
      { "cvtss2si", { ADDR_G, ADDR_W, ADDR_0 }, { OP_Y, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }	// [FV] Riportava Gd, Wss
    },
    /* OF2E */
    {
      /* 00 */
      { "ucomiss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "ucomisd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f2e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f2e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F2F */
    {
      /* 00 */
      { "comiss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "comisd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f2f", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f2f", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    }
  };

  sse_esc(state, table, 0x28);
}

void esc_0f50_70 (struct disassembly_state *state)
{

	// TODO: 66 0f 3a 17: EXTRACTPS scrive in memoria
	//	 66 0f 3a 14: PEXTRB
	//	 66 0f 3a 16: PEXTRD
	//	 66 0f 3a 16: PEXTRQ
	//	 66 0f 3a 15: PEXTRW

  insn table[][4] = {
    /* 0F50 */
    {
      /* 00 */
      { "movmskps", { ADDR_G, ADDR_V, ADDR_0 }, { OP_Y, OP_PS, OP_0 }, NULL, I_SSE },	// [FV] Riportava Ed, Vps
      /* 66 */
      { "movmskpd", { ADDR_G, ADDR_V, ADDR_0 }, { OP_Y, OP_PD, OP_0 }, NULL, I_SSE2 },	// [FV] Riportava Ed, Vpd
      /* F2 */
      { "ill_f20f50", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f50", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F51 */
    {
      /* 00 */
      { "sqrtps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "sqrtpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "sqrtsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F3 */
      { "sqrtss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }
    },
    /* 0F52 */
    {
      /* 00 */
      { "rsqrtps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "ill_660f52", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F2 */
      { "ill_f20f52", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "rsqrtss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }
    },
    /* 0F53 */
    {
      /* 00 */
      { "rcpps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "ill_660f52", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F2 */
      { "ill_f20f52", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "rcpss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE | I_XMM }
    },
    /* 0F54 */
    {
      /* 00 */
      { "andps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "andpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },	// [FV] Il manuale riporta Wpd, Vpd
      /* F2 */
      { "ill_f20f54", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f54", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F55 */
    {
      /* 00 */
      { "andnps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "andnpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },	// [FV] Il manuale riporta Wpd, Vpd
      /* F2 */
      { "ill_f20f55", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f55", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F56 */
    {
      /* 00 */
      { "orps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "orpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },	// [FV] Il manuale riporta Wpd, Vpd
      /* F2 */
      { "ill_f20f56", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f56", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F57 */
    {
      /* 00 */
      { "xorps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "xorpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },	// [FV] Il manuale riporta Wpd, Vpd (controllare altre istanze, se presenti...)
      /* F2 */
      { "ill_f20f57", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f57", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F58 */
    {
      /* 00 */
      { "addps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "addpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F2 */
      { "addsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F3 */
      { "addss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM }
    },
    /* 0F59 */
    {
      /* 00 */
      { "mulps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "mulpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F2 */
      { "mulsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F3 */
      { "mulss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM }
    },
    /* 0F5A */
    {
      /* 00 */
      { "cvtps2pd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* 66 */
      { "cvtpd2ps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "cvtsd2ss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F3 */
      { "cvtss2sd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SS, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM }
    },
    /* 0F5B */
    {
      /* 00 */
      { "cvtdq2ps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_DQ, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* 66 */
      { "cvtps2dq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_PS, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f5b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "cvttps2dq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_PS, OP_0}, NULL, I_MEMRD | I_SSE2 | I_XMM }
    },
    /* 0F5C */
    {
      /* 00 */
      { "subps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "subpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F2 */
      { "subsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F3 */
      { "subss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM }
    },
    /* 0F5D */
    {
      /* 00 */
      { "minps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "minpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F2 */
      { "minsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F3 */
      { "minss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM }
    },
    /* 0F5E */
    {
      /* 00 */
      { "divps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "divpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F2 */
      { "divsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F3 */
      { "divss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM }
    },
    /* 0F5F */
    {
      /* 00 */
      { "maxps", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PS, OP_PS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM },
      /* 66 */
      { "maxpd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_PD, OP_PD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F2 */
      { "maxsd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SD, OP_SD, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE2 | I_XMM },
      /* F3 */
      { "maxss", { ADDR_V, ADDR_W, ADDR_0 }, { OP_SS, OP_SS, OP_0 }, NULL, I_MEMRD | I_ALU | I_SSE | I_XMM }
    },
    /* 0F60 */
    {
      /* 00 */
      { "punpcklbw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "punpcklbw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f60", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f60", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F61 */
    {
      /* 00 */
      { "punpcklwd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "punpcklwd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f61", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f61", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F62 */
    {
      /* 00 */
      { "punpckldq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "punpckldq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f62", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f62", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F63 */
    {
      /* 00 */
      { "packsswb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "packsswb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f63", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f63", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F64 */
    {
      /* 00 */
      { "pcmpgtb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_MMX },
      /* 66 */
      { "pcmpgtb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f64", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f64", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F65 */
    {
      /* 00 */
      { "pcmpgtw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_MMX },
      /* 66 */
      { "pcmpgtw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f65", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f65", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F66 */
    {
      /* 00 */
      { "pcmpgtd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_MMX },
      /* 66 */
      { "pcmpgtd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20f66", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f66", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F67 */
    {
      /* 00 */
      { "packuswb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, 0 },
      /* 66 */
      { "packuswb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, 0 },
      /* F2 */
      { "ill_f20f67", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f67", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F68 */
    {
      /* 00 */
      { "punpckhbw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "punpckhbw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },	// [FV] Riportava Pdq, Qdq!
      /* F2 */
      { "ill_f20f68", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f68", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F69 */
    {
      /* 00 */
      { "punpckhwd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "punpckhwd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },	// [FV] Riportava Pdq, Qdq!
      /* F2 */
      { "ill_f20f69", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f69", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F6A */
    {
      /* 00 */
      { "punpckhdq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "punpckhdq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },	// [FV] Riportava Pdq, Qdq!
      /* F2 */
      { "ill_f20f6a", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f6a", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F6B */
    {
      /* 00 */
      { "packssdw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_D, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "packssdw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f6b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f6b", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F6C */
    {
      /* 00 */
      { "ill_0f6c", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* 66 */
      { "punpcklqdq", { ADDR_V, ADDR_W, ADDR_0}, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f6c", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f6c", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F6D */
    {
      /* 00 */
      { "ill_0f6d", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* 66 */
      { "punpckhqdq", { ADDR_V, ADDR_W, ADDR_0}, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f6d", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f6d", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F6E */
    {
      /* 00 */
      { "movd", { ADDR_P, ADDR_E, ADDR_0 }, { OP_D, OP_Y, OP_0 }, NULL, I_MEMRD | I_MMX | I_SSE2 },	// [FV] Riportava Pd, Ed ed era segnata da instrumentare!?
      /* 66 */
      { "movd", { ADDR_V, ADDR_E, ADDR_0 }, { OP_Y, OP_Y, OP_0 }, NULL, I_MEMRD | I_XMM | I_SSE2 },	// [FV] Riportava Vdq, Ed
      /* F2 */
      { "ill_f20f6e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f6e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F6F */
    {
      /* 00 */
      { "movq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "movdqa", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f6f", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "movdqu", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_MEMRD | I_XMM | I_SSE2 },
    },
    /* 0F70 */
    {
      /* 00 */
      { "pshufw", { ADDR_P, ADDR_Q, ADDR_I }, { OP_Q, OP_Q, OP_B }, NULL, I_MEMRD | I_MMX },
      /* 66 */
      { "pshufd", { ADDR_V, ADDR_W, ADDR_I }, { OP_DQ, OP_DQ, OP_B }, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "pshuflw", { ADDR_V, ADDR_W, ADDR_I }, { OP_DQ, OP_DQ, OP_B }, NULL, I_MEMRD | I_XMM | I_SSE2 },
      /* F3 */
      { "pshufhw", { ADDR_V, ADDR_W, ADDR_I }, { OP_DQ, OP_DQ, OP_B }, NULL, I_MEMRD | I_XMM | I_SSE2 }
    }
  };

  sse_esc(state, table, 0x50);
}

void esc_0f74_76 (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0F74 */
    {
      /* 00 */
      { "pcmpeqb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_CTRL | I_MEMRD | I_MMX },
      /* 66 */
      { "pcmpeqb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_CTRL | I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f74", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f74", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F75 */
    {
      /* 00 */
      { "pcmpeqw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_CTRL | I_MEMRD | I_MMX },
      /* 66 */
      { "pcmpeqw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_CTRL | I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f75", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f75", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0F76 */
    {
      /* 00 */
      { "pcmpeqd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_CTRL | I_MEMRD | I_MMX },
      /* 66 */
      { "pcmpeqd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_CTRL | I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f76", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30f76", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    }
  };

  sse_esc(state, table, 0x74);
}

void esc_0f7e_7f (struct disassembly_state *state)
{
  insn table[][4] = {
    /* OF7E */
    {
      /* 00 */
      { "movd", { ADDR_E, ADDR_P, ADDR_0 }, { OP_Y, OP_D, OP_0 }, NULL, I_MEMWR | I_MMX | I_SSE2 },	// [FV] Riportava Ed, Pd
      /* 66 */
      { "movd", { ADDR_E, ADDR_V, ADDR_0 }, { OP_Y, OP_Y, OP_0 }, NULL, I_MEMWR | I_XMM | I_SSE2 },	// [FV] Riportava Ed, Vdq
      /* F2 */
      { "ill_f20f7e", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "movq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMRD | I_XMM }
    },
    /* 0F7F */
    {
      /* 00 */
      { "movq", { ADDR_Q, ADDR_P, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_MMX },
      /* 66 */
      { "movdqa", { ADDR_W, ADDR_V, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_MEMWR | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20f7f", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "movdqu", { ADDR_W, ADDR_V, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_MEMWR | I_XMM | I_SSE2 }
    }
  };

  sse_esc(state, table, 0x7e);
}

void esc_0fc2 (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0FC2 */
    {
      /* 00 */
      { "cmpps", { ADDR_V, ADDR_W, ADDR_I }, { OP_PS, OP_PS, OP_B }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_XMM },
      /* 66 */
      { "cmppd", { ADDR_V, ADDR_W, ADDR_I }, { OP_PD, OP_PD, OP_B }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "cmpsd", { ADDR_V, ADDR_W, ADDR_I }, { OP_SD, OP_SD, OP_B }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F3 */
      { "cmpss", { ADDR_V, ADDR_W, ADDR_I }, { OP_SS, OP_SS, OP_B }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_XMM },
    }
  };

  sse_esc(state, table, 0xc2);
}

void esc_0fc4_c6 (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0FC4 */
    {
      /* 00 */
      { "pinsrw", { ADDR_P, ADDR_E, ADDR_I }, { OP_Q, OP_D, OP_B }, NULL, I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pinsrw", { ADDR_V, ADDR_E, ADDR_I }, { OP_DQ, OP_D, OP_B }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fc4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fc4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FC5 */
    {
      /* 00 */
      { "pextrw", { ADDR_G, ADDR_N, ADDR_I }, { OP_D, OP_Q, OP_B }, NULL, I_SSE | I_MMX },	// [FV] Riportava Gd, Nq, Ib
      /* 66 */
      { "pextrw", { ADDR_G, ADDR_U, ADDR_I }, { OP_D, OP_DQ, OP_B }, NULL, I_SSE2 | I_XMM },	// [FV] Riportava Gd, Vdq, Ib
      /* F2 */
      { "ill_f20fc5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fc5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FC6 */
    {
      /* 00 */
      { "shufps", { ADDR_V, ADDR_W, ADDR_I }, { OP_PS, OP_PS, OP_B }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* 66 */
      { "shufpd", { ADDR_V, ADDR_W, ADDR_I }, { OP_PD, OP_PD, OP_B }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fc6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fc6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    }
  };

  sse_esc(state, table, 0xc4);
}

void esc_0fd1_ef (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0FD1 */
    {
      /* 00 */
      { "psrlw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psrlw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20fd1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD2 */
    {
      /* 00 */
      { "psrld", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psrld", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20fd2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD3 */
    {
      /* 00 */
      { "psrlq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psrlq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_XMM | I_SSE2 },
      /* F2 */
      { "ill_f20fd3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD4 */
    {
      /* 00 */
      { "paddq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_MMX },
      /* 66 */
      { "paddq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fd4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD5 */
    {
      /* 00 */
      { "pmulw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "pmulw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_MMX },
      /* F2 */
      { "ill_f20fd5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD6 */
    {
      /* 00 */
      { "ill_0fd6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* 66 */
      { "movq", { ADDR_W, ADDR_V, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_XMM },
      /* F2 */
      { "movdq2q", { ADDR_P, ADDR_U, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MMX | I_XMM },	// [FV] Riportava Pq, Wq ed era segnata da instrumentare !?
      /* F3 */
      { "movq2dq", { ADDR_V, ADDR_N, ADDR_0 }, { OP_DQ, OP_Q, OP_0 }, NULL, I_MMX | I_XMM }	// [FV] Riportava Vdq, Qq ed era segnata da instrumentare !?
    },
    /* 0FD7 */
    {
      /* 00 */
      { "pmovmskb", { ADDR_G, ADDR_N, ADDR_0 }, { OP_D, OP_Q, OP_0 }, NULL, I_MMX | I_SSE },	// [FV] Riportava Gd, Pq
      /* 66 */
      { "pmovmksb", { ADDR_G, ADDR_U, ADDR_0 }, { OP_D, OP_DQ, OP_0 }, NULL, I_XMM | I_SSE2 },	// [FV] Riportava Gd, Vdq
      /* F2 */
      { "ill_f20fd7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD8 */
    {
      /* 00 */
      { "psubusb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubusb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fd8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FD9 */
    {
      /* 00 */
      { "psubusw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubusw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fd9", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fd9", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FDA */
    {
      /* 00 */
      { "pminub", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pminub", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fda", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fda", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FDB */
    {
      /* 00 */
      { "pand", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "pand", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fdb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fdb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FDC */
    {
      /* 00 */
      { "paddusb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddusb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fdc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fdc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FDD */
    {
      /* 00 */
      { "paddusw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddusw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fdd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fdd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FDE */
    {
      /* 00 */
      { "pmaxub", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pmaxub", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fde", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fde", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FDF */
    {
      /* 00 */
      { "pandn", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "pandn", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fdf", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fdf", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE0 */
    {
      /* 00 */
      { "pavgb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pavgb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe0", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe0", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE1 */
    {
      /* 00 */
      { "praw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "praw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE2 */
    {
      /* 00 */
      { "prad", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "prad", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE3 */
    {
      /* 00 */
      { "pavgw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pavgw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE4 */
    {
      /* 00 */
      { "pmulhuw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pmulhuw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE5 */
    {
      /* 00 */
      { "pmulhw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "pmulhw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE6 */
    {
      /* 00 */
      { "ill_0fe6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* 66 */
      { "cvttpd2dq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_PD, OP_0}, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "cvtpd2dq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_PD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM },
      /* F3 */
      { "cvtdq2pd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_PD, OP_0 }, NULL, I_MEMRD | I_SSE2 | I_XMM }	// [FV] Riportava Vpd, Wdq
    },
    /* 0FE7 */
    {
      /* 00 */
      { "movntq", { ADDR_M, ADDR_P, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_MMX },	// [FV] Riportava Gd, Pq
      /* 66 */
      { "movntdq", { ADDR_M, ADDR_V, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_MEMWR | I_SSE2 | I_XMM },	// [FV] Riportava Wdq, Vq
      /* F2 */
      { "ill_f20fe7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE8 */
    {
      /* 00 */
      { "psubsb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubsb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FE9 */
    {
      /* 00 */
      { "psubsw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubsw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fe9", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fe9", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FEA */
    {
      /* 00 */
      { "pminusw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pminusw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fea", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fea", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FEB */
    {
      /* 00 */
      { "por", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "por", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20feb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30feb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FEC */
    {
      /* 00 */
      { "paddsb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddsb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fec", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fec", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FED */
    {
      /* 00 */
      { "paddsw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddsw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fed", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fed", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FEE */
    {
      /* 00 */
      { "pmaxsw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "pmaxsw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_CTRL | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fee", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fee", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FEF */
    {
      /* 00 */
      { "pxor", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "pxor", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20fef", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30fef", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    }
  };

  sse_esc(state, table, 0xd1);
}

void esc_0ff1_fe (struct disassembly_state *state)
{
  insn table[][4] = {
    /* 0FF1 */
    {
      /* 00 */
      { "psllw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psllw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff1", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF2 */
    {
      /* 00 */
      { "pslld", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "pslld", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff2", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF3 */
    {
      /* 00 */
      { "psllq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psllq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff3", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF4 */
    {
      /* 00 */
      { "pmuludq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_MMX },
      /* 66 */
      { "pmuludq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff4", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF5 */
    {
      /* 00 */
      { "pmaddwd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_MMX },
      /* 66 */
      { "pmaddwd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff5", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF6 */
    {
      /* 00 */
      { "psadbw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE | I_MMX },
      /* 66 */
      { "psadbw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff6", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF7 */
    {
      /* 00 */	// [FV] Scrive su DS:DI/EDI/RDI
      { "maskmovq", { ADDR_P, ADDR_N, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_MEMWR | I_MMX },	// [FV] Riportava Ppi, Qpi
      /* 66 */	// [FV] Scrive su DS:EDI/RDI
      { "maskmovdqu", { ADDR_V, ADDR_U, ADDR_0}, { OP_DQ, OP_DQ, OP_0}, NULL, I_MEMWR | I_SSE2 | I_XMM },	// [FV] Riportava Vdq, Wdq
      /* F2 */
      { "ill_f20ff7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff7", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF8 */
    {
      /* 00 */
      { "psubb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff8", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FF9 */
    {
      /* 00 */
      { "psubw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ff9", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ff9", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FFA */
    {
      /* 00 */
      { "psubd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "psubd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ffa", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ffa", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FFB */
    {
      /* 00 */
      { "psubq", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_MMX },
      /* 66 */
      { "psubq", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ffb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ffb", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FFC */
    {
      /* 00 */
      { "paddb", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddb", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ffc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ffc", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FFD */
    {
      /* 00 */
      { "paddw", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddw", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ffd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ffd", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    },
    /* 0FFE */
    {
      /* 00 */
      { "paddd", { ADDR_P, ADDR_Q, ADDR_0 }, { OP_Q, OP_Q, OP_0 }, NULL, I_ALU | I_MEMRD | I_MMX },
      /* 66 */
      { "paddd", { ADDR_V, ADDR_W, ADDR_0 }, { OP_DQ, OP_DQ, OP_0 }, NULL, I_ALU | I_MEMRD | I_SSE2 | I_XMM },
      /* F2 */
      { "ill_f20ffe", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 },
      /* F3 */
      { "ill_f30ffe", { ADDR_0, ADDR_0, ADDR_0 }, { OP_0, OP_0, OP_0 }, NULL, 0 }
    }
  };

  sse_esc(state, table, 0xf1);
}

/* Gruppi di estensione degli opcode: usano i bit 5-3 del byte ModR/M per l'istruzione */

/* immed_grp_1
 * L'opcode può andare da 0x80 a 0x83. L'istruzione è scelta in base ai bit 5-3 del byte
 * ModR/M.
 */
void immed_grp_1(struct disassembly_state *state)
{
	unsigned char encoding;
	char *instructions[] = { "add", "or", "adc", "sbb",
				 "and", "sub", "xor", "cmp" };

	// Tutte queste istruzioni, tranne cmp, possono scrivere in memoria

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;

	strcpy(state->instrument->mnemonic, instructions[encoding]);

	if (encoding == 0b000 || encoding == 0b001 || encoding == 0b010 || encoding == 0b011 || encoding == 0b100 || encoding == 0b101 || encoding == 0b110)
		state->instrument->flags |= I_MEMRD | I_MEMWR | I_ALU;

	else if (encoding== 0b111)
		state->instrument->flags |= I_MEMRD | I_ALU | I_CTRL;

	switch(encoding) {


		case 0b000 ... 0b110:	// ADD, ADC, SBB, AND, SUB, XOR ( ... e' una GNU extension)

			state->instrument->flags |= I_MEMRD | I_MEMWR | I_ALU;
			break;
		case 0b111:	// CMP
			state->instrument->flags |= I_MEMRD | I_ALU | I_CTRL;
	}
}

/* opcodes C0-C1, D0-D3 */
void shift_grp_2(struct disassembly_state *state)
{
	unsigned char encoding;

	// Queste istruzioni possono scrivere tutte in memoria

	char *instructions[] = { "rol", "ror", "rcl", "rcr",
				 "shl", "shr", "ill_grp_2", "sar" };

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;

	strcpy(state->instrument->mnemonic, instructions[encoding]);

	if(encoding != 0b110) {
		state->instrument->flags |= I_MEMRD | I_MEMWR | I_ALU;
	}
}

/* opcodes F6-F7 */
void unary_grp_3(struct disassembly_state *state)
{
	// Possono scrivere in memoria: not, neg

	unsigned char encoding, opcode;
	char *instructions[] = { "test", "ill_grp_3", "not", "neg",
				 "mul", "imul", "div", "idiv" };
	enum addr_method addr[8][2] = { { ADDR_I, ADDR_I }, { ADDR_0, ADDR_0 },
					{ ADDR_0, ADDR_0 }, { ADDR_0, ADDR_0 },
					{ R_AL, R_AX }, { R_AL, R_AX },
					{ R_AL, R_AX }, { R_AL, R_AX } };
	enum operand_type op[8][2] = { { OP_B, OP_V }, { OP_0, OP_0 },
				       { OP_0, OP_0 }, { OP_0, OP_0 },
				       { OP_0, OP_E }, { OP_0, OP_E },
				       { OP_0, OP_E }, { OP_0, OP_E } };

	unsigned long flags[8] =      {	I_ALU | I_CTRL | I_MEMRD,
					0,
					I_ALU | I_MEMRD | I_MEMWR,
					I_ALU | I_MEMRD | I_MEMWR,
					I_ALU | I_MEMRD,
					I_ALU | I_MEMRD,
					I_ALU | I_MEMRD, I_ALU | I_MEMRD};

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	opcode = state->opcode[0] - 0xf6;

	strcpy(state->instrument->mnemonic, instructions[encoding]);
	state->addr[1] = addr[encoding][opcode];
	state->op[1] = op[encoding][opcode];
	state->instrument->flags = flags[encoding];
}

/* opcode FE */
void grp_4(struct disassembly_state *state)
{
	unsigned char encoding;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	state->instrument->flags = I_MEMRD | I_MEMWR | I_ALU;

	switch(encoding) {
		case 0:	// inc
			strcpy(state->instrument->mnemonic, "inc");
			break;
		case 1: // dec
			strcpy(state->instrument->mnemonic, "dec");
			break;
		default: // ill_grp_4
			strcpy(state->instrument->mnemonic, "ill_grp_4");
			state->instrument->flags = 0;
			break;
	}

	if(encoding < 2) {
		state->addr[0] = ADDR_E;
		state->op[0] = OP_B;
	}
}

/* opcode FF */
void grp_5(struct disassembly_state *state)
{
	unsigned char encoding;
	char *instructions[] = { "inc", "dec", "call", "call far",
				 "jmp", "jmp far", "push", "ill_grp_5" };

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	strcpy(state->instrument->mnemonic, instructions[encoding]);

	switch(encoding) {
		case 0x00:
		case 0x01:
			state->instrument->flags = I_ALU | I_MEMRD | I_MEMWR;
			break;
		case 0x02:
		case 0x03:
			state->instrument->flags = I_CALLIND | I_CALL;
			break;
		case 0x04:
		case 0x05:
			state->instrument->flags = I_JUMPIND | I_JUMP;
			break;
		case 0x06:
			state->instrument->flags = I_PUSHPOP | I_MEMRD;
			break;
		case 0x07:
			return;
	}

	state->addr[0] = ADDR_E;

	if(encoding == 0x03 || encoding == 0x05)
		state->op[0] = OP_P;
	else
		state->op[0] = OP_V;
}

/* opcode 0F00 */
void grp_6(struct disassembly_state *state)
{
	unsigned char encoding;

	char *instructions[6] = { "sldt", "str", "lldt", "ltr", "verr", "verw" };

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	if(encoding > 0x05) {
		strcpy(state->instrument->mnemonic, "ill_grp_6");
		return;
	}

	strcpy(state->instrument->mnemonic, instructions[encoding]);
	state->addr[0] = ADDR_E;
	state->op[0] = OP_W;	// [FV] Non corretto con dimensione registri per SLDT e STR!!!

	switch(encoding) {
		case 0x00:	// SLDT
			state->instrument->flags = I_MEMWR;
			break;
		case 0x01:	// STR
			state->instrument->flags = I_MEMWR;
			break;
		case 0x02:	// LLDT
			state->instrument->flags = I_MEMRD;
			break;
		case 0x03:	// LTR
			state->instrument->flags = I_MEMRD;
			break;
		case 0x04:	// VERR
			state->instrument->flags = I_CTRL | I_MEMRD;
			break;
		case 0x05:	// VERW
			state->instrument->flags = I_CTRL | I_MEMRD;
			break;
	}

	// [FV] state->op[0] = (encoding < 2) ? OP_V : OP_W; - Non penso funzioni... Ricontrollare!
}

/* opcode 0F01 */
void grp_7(struct disassembly_state *state)
{
	unsigned char encoding, lower_bits, mod_76;
	char *instructions[] = { "sgdt", "sidt", "lgdt", "lidt",
				 "smsw", "ill_grp_7", "lmsw", "invlpg" };

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	lower_bits = state->modrm & 0x07;
	mod_76 = (state->modrm >> 6) & 0x03;

	// TODO: mod_76 dovrà essere reintrodotto prima o poi, quando questo modulo verrà esteso oltre le SSE2
	(void)mod_76;
	(void)lower_bits;

	/*if(mod_76 == 11b && encoding != 100b && encoding |= 110b) {	// [FV] In realtà queste potremmo non gestirle...
		switch(encoding) {
			case 000b:
				switch(lower_bits) {
					case 001b:
						strcpy(state->instrument->mnemonic, "vmcall");
						break;
					case 010b:
						strcpy(state->instrument->mnemonic, "vmlaunch");
						break;
					case 011b:
						strcpy(state->instrument->mnemonic, "vmresume");
						break;
					case 100b:
						strcpy(state->instrument->mnemonic, "vmxoff");
						break;
					default:
						strcpy(state->instrument->mnemonic, "ill_grp_7");
						break;
				}
				break;
			case 001b:
				switch(lower_bits) {
					case 000b:
						strcpy(state->instrument->mnemonic, "trampoline");
						break;
					case 001b:
						strcpy(state->instrument->mnemonic, "mwait");
						break;
					default:
						strcpy(state->instrument->mnemonic, "ill_grp_7");
						break;
				}
				break;
			case 010b:
				switch(lower_bits) {
					case 000b:
						strcpy(state->instrument->mnemonic, "xgetbv");
						break;
					case 001b:
						strcpy(state->instrument->mnemonic, "xsetbv");
						break;
					default:
						strcpy(state->instrument->mnemonic, "ill_grp_7");
						break;
				}
				break;
			case 111b:
				switch(lower_bits) {
					case 000b:
						strcpy(state->instrument->mnemonic, "swapgs");
						break;
					case 001b:
						strcpy(state->instrument->mnemonic, "rdtscp");
						break;
					default:
						strcpy(state->instrument->mnemonic, "ill_grp_7");
						break;
				}
				break;
			default:
				strcpy(state->instrument->mnemonic, "ill_grp_7");
				break;
			//}
		}
	}*/
	//else {
		strcpy(state->instrument->mnemonic, instructions[encoding]);

		if(encoding == 5)	// ill_grp_7
			return;

		if(encoding < 4) {
			state->addr[0] = ADDR_M;
			state->op[0] = OP_S;
			if(encoding < 2)	// SGDT, SIDT
				state->instrument->flags = I_MEMWR;
			else	// LGDT, LIDT
				state->instrument->flags = I_MEMRD;
		} else if(encoding == 0x07) {	// INVLPG
			state->addr[0] = ADDR_M;
			state->op[0] = OP_B;
		} else {
			state->addr[0] = ADDR_E;
			state->op[0] = OP_W;
			if(encoding == 4)	// SMSW
				state->instrument->flags = I_MEMWR;
			else	// LMSW
				state->instrument->flags = I_MEMRD;
		}
	//}
}

/* opcode 0FBA */
void grp_8(struct disassembly_state *state)
{
	unsigned char encoding;
	char *instructions[] = { "bt", "bts", "btr", "btc" };

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;

	if(encoding < 4) {
		// ill_grp_8
		strcpy(state->instrument->mnemonic, "ill_grp_8");
		state->addr[0] = state->addr[1] = ADDR_0;
		state->op[0] = state->op[1] = OP_0;
		return;
	}

	encoding -= 4; // I valori a 0 a 3 in realtà non sono usati

	strcpy(state->instrument->mnemonic, instructions[encoding]);
	state->instrument->flags = I_MEMRD;	// Tutte possono leggere dalla memoria

	if(encoding > 0)	// BTS, BTR, BTC
		state->instrument->flags |= I_MEMWR;
}

/* opcode 0FC7 */ // [FV] Non tutte le istruzioni vengono gestite
void grp_9(struct disassembly_state *state)
{
	unsigned char encoding, mod;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	mod = (state->modrm >> 6) & 0x03;

	if(mod != 0x03 && encoding == 0x01) { // cmpxch8b, scrive in memoria
		state->instrument->flags = I_CTRL | I_CONDITIONAL | I_ALU | I_MEMRD | I_MEMWR; // [FV]
		strcpy(state->instrument->mnemonic, "cmpxch8b");
		state->addr[0] = ADDR_M;
		if(REXW(state->rex))
			state->op[0] = OP_DQ;	// [FV] Perche' questo non veniva gestito?
		else
			state->op[0] = OP_Q;
	} else
		strcpy(state->instrument->mnemonic, "ill_grp_9");
}

/* opcode 0FB9 */
void grp_10(struct disassembly_state *state)
{
	// UD e grp 10
	// Qui non c'è nulla da fare
	strcpy(state->instrument->mnemonic, "ill_grp_10");
}

/* opcodes C6-C7 */
void grp_11(struct disassembly_state *state)
{
	read_modrm(state);

	if((state->modrm >> 3) & 0x07) {
		strcpy(state->instrument->mnemonic, "ill_grp_11");
		return;
	}

	// In questo gruppo ci sono delle mov che possono scrivere a memoria,
	// ma il flag è stato già settato a true precedentemente
	strcpy(state->instrument->mnemonic, "mov");
	state->instrument->flags |= I_MEMWR;

	// [FV] La seguente porzione di codice mi pare ridondante
	/* [FV]
	 *	state->addr[0] = ADDR_E;
	 *	state->addr[1] = ADDR_I;
	 *
	 *	if(state->opcode[0] == 0xc6)
	 *		state->op[0] = state->op[1] = OP_B;
	 *	else
	 *		state->op[0] = state->op[1] = OP_V;
	 */
}

/* opcode 0F71 */
void grp_12(struct disassembly_state *state)
{
	unsigned char encoding, mod, sse_prefix;
	bool illegal = false;
	char *mnemonic;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	mod = (state->modrm >> 6) & 0x03;
	sse_prefix = state->sse_prefix;

	state->prefix[0] = state->prefix[3] = 0x00; // cancella i prefissi grp 1 e 3

	if(mod != 0b11  || (sse_prefix != 0x66 && sse_prefix != 0x00))
		illegal = true;

	switch(encoding) {
		case 0x02: mnemonic = "psrlw"; break;
		case 0x04: mnemonic = "psraw"; break;
		case 0x06: mnemonic = "psllw"; break;
		default: illegal = true;
	}

	if(illegal == false) {
		strcpy(state->instrument->mnemonic, mnemonic);
		state->instrument->flags = I_ALU;

		if(sse_prefix == 0x66) {
			state->addr[0] = ADDR_W;	// [FV] Errore!? Era riportato ADDR_P...
			state->op[0] = OP_DQ;
			state->instrument->flags |= I_SSE2 | I_XMM;	// [FV] Controllare! Insieme ad istanze analoghe.
		}
		else {
			state->addr[0] = ADDR_P;
			state->op[0] = OP_Q;
			state->instrument->flags |= I_MMX; //Alice
		}

		state->addr[1] = ADDR_I;
		state->op[1] = OP_B;
	}
	else {
		strcpy(state->instrument->mnemonic, "ill_grp_12");
	}
}

/* opcode 0F72 */
void grp_13(struct disassembly_state *state)
{
	unsigned char encoding, mod, sse_prefix;
	bool illegal = false;
	char *mnemonic;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	mod = (state->modrm >> 6) & 0x03;
	sse_prefix = state->sse_prefix;

	state->prefix[0] = state->prefix[2] = 0x00; // cancella i prefissi grp 1 e 3

	if(mod != 0x03 || (sse_prefix != 0x66 && sse_prefix != 0x00))
		illegal = true;

	switch(encoding) {
		case 0x02: mnemonic = "psrld"; break;
		case 0x04: mnemonic = "psrad"; break;
		case 0x06: mnemonic = "pslld"; break;
		default: illegal = true;
	}

	if(illegal == false) {
		strcpy(state->instrument->mnemonic, mnemonic);
		state->instrument->flags = I_ALU;

		state->addr[0] = ADDR_P;
		state->addr[1] = ADDR_I;

		if(sse_prefix == 0x66) {
			state->op[0] = OP_DQ;
			state->instrument->flags |= I_SSE2 | I_XMM;
		}
		else {	// [FV] In questo caso riportava addr[0] = ADDR_W. Errore!?
			state->op[0] = OP_Q;
			state->instrument->flags |= I_MMX; //Alice
		}

		state->op[1] = OP_B;
	}
	else {
		strcpy(state->instrument->mnemonic, "ill_grp_13");
	}
}

/* opcode 0F73 */
void grp_14(struct disassembly_state *state)
{
	unsigned char encoding, mod, sse_prefix;
	char *mnemonic;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	mod = (state->modrm >> 6) & 0x03;
	sse_prefix = state->sse_prefix;

	state->prefix[0] = state->prefix[3] = 0x00; // Cancella pref grp 1 e 3

	// Per codificare 0x02 e 0x06, i prefissi sse validi sono 0x00 e 0x66.
	// Per codificare 0x03 e 0x07, l'unico prefisso valido è 0x66
	if(mod != 0b11 ||
	(
	(!((encoding == 0x02 || encoding == 0x06) && ((sse_prefix == 0x00) || (sse_prefix == 0x66)))) ||
	(!((encoding == 0x03 || encoding == 0x07) && (sse_prefix == 0x66))))	// [FV] Attenzione! I controlli erano errati perché mancava la negazione su sse_prefix
	) {
		strcpy(state->instrument->mnemonic, "ill_grp_14");
		return;
	}

	// [FV] pslldq solo quando encoding == 7
	mnemonic = (encoding == 2) ? "psrlq" : (encoding == 3) ? "psrldq" : (encoding == 6) ? "psllq" : "pslldq";
	strcpy(state->instrument->mnemonic, mnemonic);

	state->instrument->flags = I_ALU;

	if(sse_prefix == 0x66) {
		state->addr[0] = ADDR_W;
		state->op[0] = OP_DQ;
		state->instrument->flags |= I_XMM | I_SSE2;
	} else {
		state->addr[0] = ADDR_P;
		state->op[0] = OP_Q;
		state->instrument->flags |= I_MMX;
	}

	state->addr[1] = ADDR_I;
	state->op[1] = OP_B;	// [FV] Errore, c'era scritto op[0] !
}

/* opcode 0FAE */	// [FV] Non tutte le istruzioni sono gestite
void grp_15(struct disassembly_state *state)
{
	unsigned char encoding, mod;
	char *mnemonic;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	mod = (state->modrm >> 6) & 0x03;

	if((mod == 0b11 && encoding < 5) || (mod != 0b11 && (encoding < 7 && encoding > 3))) {	// XSAVE, XRSTOR, XSAVEOPT
		strcpy(state->instrument->mnemonic, "ill_grp_15");
		return;
	}

	if(mod == 0b11) { //Alice
		switch(encoding) {
			case 0x05: mnemonic = "lfence"; break;
			case 0x06: mnemonic = "mfence"; break;
			case 0x07: mnemonic = "sfence"; break;
		}
	} else {
		state->addr[0] = ADDR_M;

		switch(encoding) {
			case 0x00: // fxsave
				mnemonic = "fxsave";
				state->instrument->flags = I_MEMWR | I_CTRL | I_MMX | I_XMM | I_FPU;
				state->op[0] = OP_M512byte;
				break;
			case 0x01: // fxrstor
				mnemonic = "fxrstor";
				state->instrument->flags = I_MEMRD | I_CTRL | I_MMX | I_XMM | I_FPU;
				state->op[0] = OP_M512byte;
				break;
			case 0x02: // ldmxcsr
				mnemonic = "ldmxcsr";
				state->instrument->flags = I_MEMRD | I_SSE;
				state->op[0] = OP_D;
				break;
			case 0x03: // stmxcsr
				mnemonic = "stmxcsr";
				state->instrument->flags = I_MEMWR | I_SSE;
				state->op[0] = OP_D;
				break;
			case 0x07: // clflush
				mnemonic = "clflush";
				state->instrument->flags = I_SSE2;	// [FV] SSE2 cacheability instruction, ma ha un proprio CPUID Feature flag
				state->op[0] = OP_B;
				break;
		}

		strcpy(state->instrument->mnemonic, mnemonic);
		// Imposto Addr/Op più probabili
		// [FV] state->addr[0] = ADDR_M;
		// [FV] state->op[0] = OP_B;
	}
}

/* opcode OF18 */
void grp_16(struct disassembly_state *state)
{
	unsigned char encoding, mod;
	char *mnemonic;

	read_modrm(state);

	encoding = (state->modrm >> 3) & 0x07;
	mod = (state->modrm >> 6) & 0x03;

	if(mod == 0x03 || encoding > 0x03) {
		strcpy(state->instrument->mnemonic, "ill_grp_16");
		return;
	}

	switch(encoding) {
		case 0x00: mnemonic = "prefetchnta"; break;
		case 0x01: mnemonic = "prefetcht0"; break;
		case 0x02: mnemonic = "prefetcht1"; break;
		case 0x03: mnemonic = "prefetcht2"; break;
	}

	state->instrument->flags = I_MEMRD;
	strcpy(state->instrument->mnemonic, mnemonic);
	state->addr[0] = ADDR_M;
	state->op[0] = OP_B;
}

/* select_operand_size
 * Determina quanti dati verranno scritti in memoria, utilizzando le
 * dimensioni degli operandi
 */
void select_operand_size(struct disassembly_state *state, enum operand_type op)
{
	unsigned long size;

	// Eccezione per la modalità a 64bit:
	// Se c'è il prefisso 66H e REX.W = 0, la dimensione è 16 bit
	if(state->prefix[3] == 0x66 && !REXW(state->rex)) {
		state->instrument->span = 2;
		return;
	}

	switch(op) {
		case OP_M512byte:
			size = 512;
			break;
		case OP_FSR:
			if(state->opd_size == SIZE_16)
				size = 94;
			else
				size = 108;
			break;
		case OP_FS:
			if(state->opd_size == SIZE_16)
				size = 14;
			else
				size = 24;
			break;
		case OP_A:
			/* Istruzione BOUND: se lavoriamo con operandi a 16 bit, vengono letti dalla memoria due interi word
			 * con segno adiacenti, mentre se lavoriamo a 32 bit, vengono letti dalla memoria due interi doubleword
			 * con segno adiacenti. L'istruzione BOUND non è permessa in 64-bit mode.
			 */
			if(state->opd_size == SIZE_16) {
				size = 4;
			}
			else {
				size = 8;
			}
			break;
		case OP_0:
		case OP_E:
			size = 0;
			break;
		case OP_C: /* byte/word/qword */
			if(REXW(state->rex)) {
				size = 8;
				break;
			}
			if(state->opd_size == SIZE_16) {
				size = 2;
				break;
			}
			/* fallthrough */
		case OP_B: /* byte, a prescindere da tutti gli altri parametri */
			size = 1;
			break;
		case OP_Y:
			if(REXW(state->rex)) {
				size = 8;	// [FV] e.g. istruzione MOVNTI
				break;
			}
			/* fallthrough */
		case OP_D: /* double word */
			/* fallthrough */
		case OP_SI:
			size = 4;
			break;
		case OP_PD: /* double quad word */
		case OP_DQ:
			size = 16;
			break;
		case OP_P: /* 6 byte o double word */ // [FV] Od anche 10 byte
			if(REXW(state->rex)) {
				size = 10;	// [FV] e.g. istruzione LSS
				break;
			}
			if(state->opd_size == SIZE_16) {	// [FV] C'era scritto "SIZE_32" invece di "SIZE_16"?
				size = 4;
				break;
			}
			size = 6;
			break;
		case OP_S: /* 6 byte*/	// [FV] Non dovrebbe riportare 10 byte per IA-32e mode? eg istruzione SGDT?
			if(state->mode64 == true)	// [FV] Da rivedere!
				size = 10;
			else
				size = 6;
			break;
		case OP_PS: /* double quad word */
			size = 16;
			break;
		case OP_M80:
			size = 10;
			break;
		case OP_Q: /* quad word */
		case OP_PI:
			size = 8;
			break;
		case OP_SS: /* double word */
		case OP_SD:
			size = 4;
			break;
		case OP_V: /* word o double word o quad word*/	// [FV] Rivedere, pero', istruzioni come la SLDT
			if(REXW(state->rex)) {
				size = 8;
				break;
			}
			if(state->opd_size == SIZE_32) {
				size = 4;
				break;
			}
			/* fallthrough */
		case OP_W: /* word */
			size = 2;
	}

	state->instrument->span = size;
}


void format_addr_m (struct disassembly_state *state, enum addr_method addr, enum operand_type op);

/* Le funzioni di formato usano l'istruzione, il metodo di accesso ed il tipo
 * di operando per estrarre informazioni sul tipo di dati gestiti dalle
 * istruzioni
 */

/* format_addr_a
 * Accesso diretto. Nessun byte ModR/M, nessun registro di base, nessun indice nei
 * registri, nessun fattore di scala. Decisamente semplice!
 */
void format_addr_a (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;

	// Questo formato è proprio della jmp far e della call far.
	// La jmp imposta il flag to_instrument a true, mentre la
	// call no. Pertanto, soltanto la jmp far sarà intercettata.

	// [FV] state->instrument->is_jmp = true;

	uint16_t segment, short_offset;
	uint32_t long_offset, offset;

	if(op == OP_P) { // Recupera il segmento
		memcpy(&segment, state->text + state->pos, 2);
		state->pos += 2;

		// Recupera l'offset
		if(state->opd_size == SIZE_16) { // 16 bit
			memcpy(&short_offset, state->text + state->pos, 2);
			state->pos += 2;
			offset = short_offset;
		} else { // 32 o 64 bit
			memcpy(&long_offset, state->text + state->pos, 4);
			state->pos += 4;
			offset = long_offset;
		}

		// Memorizza l'indirizzo assoluto
		state->instrument->addr = segment;

		// TODO: ricontrollare a cosa serviva qui offset...
		(void)offset;
	} else {
		fprintf(stderr, "%s:%d: Errore interno\n", __FILE__, __LINE__);
		abort();
	}
}

/* format_addr_c
 * I bit 5-3 del byte ModR/M identificano un registro di controllo
 */
void format_addr_c (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;
	(void)op;

	// Da qui non si può recuperare alcuna informazione interessante...

	char modrm = (state->modrm >> 3) & 0x07;

	// Estende i registri di controllo a 64bit
	if(state->mode64) {
		if(REXR(state->rex))
			modrm |= 0x08;
	}

	switch(modrm) {
		case 0: // cr0
		case 1: // cr1
		case 2: // cr2
		case 3: // cr3
		case 4: // cr4
		case 8: // cr8 [Task Priority Register]
		default: // cr_undef
			break;
	}
}

/* format_addr_d
 * I bit 5-3 del byte ModR/M selezionano un registro di debug
 */
void format_addr_d (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;
	(void)op;

	// Niente di interessante...

	char modrm = (state->modrm >> 3) & 0x07;

	// Estende i registri di controllo a 64bit
	if(state->mode64) {
		if(REXR(state->rex))
			modrm |= 0x08;
	}

	switch(modrm) {
		case 0: // dr0
		case 1: // dr1
		case 2: // dr2
		case 3: // dr3
		case 4: // dr4
		case 5: // dr5
		case 6: // dr6
		case 7: // dr7
		case 8:	// dr8
		case 9:	// dr9
		case 10: // dr10
		case 11: // dr11
		case 12: // dr12
		case 13: // dr13
		case 14: // dr14
		case 15: // dr15
		default: // dr_undef
			break;
	}
}

/* format_addr_e
 * A seconda del byte ModR/M, l'operando o è un registro, o è un
 * indirizzo di memoria con un registro di base opzionale, registro
 * di indice, fattore di scala e displacement...
 */
void format_addr_e (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{

	/* Se ModR/M specifica un registro, allora OP specifica la sua dimensione,
	   come la differenza tra ax, eax o rax. Se ModR/M specifica un indirizzo,
	   allora OP specifica il tipo di dato puntato (byte, word, ...) */
	unsigned char rm;
	enum reg_size reg_size = REG_SIZE_128;

	(void)addr;

	rm = state->modrm & 0x07;

	switch(op) {
		case OP_B: /* 1 */
			reg_size = REG_SIZE_8;
			break;
		case OP_C: /* 1/2 */
      			if(state->opd_size != SIZE_16) {
				reg_size = REG_SIZE_8;
				break;
			}
			/* fallthrough */
		case OP_W: /* 2 */
			reg_size = REG_SIZE_16;
			break;
		case OP_V: /* 2/4/8 */	// [FV] Controllare, tuttavia, la SLDT
			if(state->opd_size == SIZE_64) {
				if(REXW(state->rex)) {
					reg_size = REG_SIZE_64;
					break;
				}
			}
			if(state->opd_size == SIZE_16)	{
				reg_size = REG_SIZE_16;
				break;
			}
			/* fallthrough */
		case OP_D: /* 4 */
		case OP_SI: /* 4 */
			reg_size = REG_SIZE_32;
			break;
		case OP_Q: /* 8 */
		case OP_PI: /* 8 */
			reg_size = REG_SIZE_64;
			break;
		case OP_DQ: /* 16 */
		case OP_PS: /* 16 */
		case OP_PD: /* 16 */
			reg_size = REG_SIZE_128;
			break;
		case OP_Y: /* RICONTROLLARE ASSOLUTAMENTE */
			if(state->opd_size == SIZE_64) {
				reg_size = REG_SIZE_64;
			}
			reg_size = REG_SIZE_32;
			break;
		default:
			fprintf(stderr, "%s: %d: Unexpected operand %d\n", __FILE__, __LINE__, op);
			break;
	}

	if(state->modrm >> 6 == 0x3) { // Specifica un registro
		/* [FV] Potrebbe sia leggere che scrivere la memoria e, se lo fa, e' soltanto tramite questo operando */
		state->instrument->flags &= ~(I_MEMWR | I_MEMRD);

		if(!state->read_dest) {
			// È a registro: non scrive in memoria
			// [FV] state->instrument->to_memory = false;

			// Nel caso di una jump, potrebbe essere una indirect branch:
			// salva il valore del registro come fosse un registro di base
			state->instrument->has_base_register = true;
			state->instrument->breg = rm;
		}
	} else {
		switch(reg_size) {
			case REG_SIZE_8: // byte
			case REG_SIZE_16: // word
			case REG_SIZE_32: // dword
			case REG_SIZE_64: // qword
			case REG_SIZE_128: // oword
			default: // ?
				break;
		}

		if(state->addr[0] != ADDR_G
		    && state->addr[1] != ADDR_G
		    && state->addr[2] != ADDR_G) {
			// Se siamo a 64 bit, REX.R estende il campo reg
			if(state->mode64 && REXR(state->rex)) {
				; // Qui andrebbe esteso il campo reg
			}
		}

		if(state->op[0] == 0x0F && state->op[1] == 0xC4)	// [FV] Eccezione istruzioni PINSRW
			op = OP_W;
		format_addr_m(state, addr, op); // passa il riferimento a memoria
	}
}

/* format_addr_g
 * Il campo Reg del byte ModR/M specifica un registro
 */
void format_addr_g (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	enum reg_size reg_size;
	int reg_field;
	(void)addr;

	// Tipo predefinito - molto probabilmente è quello sbagliato...
	reg_size = REG_SIZE_128;
	reg_field = (state->modrm >> 3) & 0x07;

	// Estende ai registri a 64 bit, se esegue in modalità a 64 bit
	if(state->mode64 && REXR(state->rex)) {
		reg_field |= 0x08;
	}

	switch(op) {
		case OP_B: /* 8 */
			reg_size = REG_SIZE_8;
			break;
		case OP_C: /* 8/16 */
			if(state->opd_size != SIZE_16)	{
				reg_size = REG_SIZE_8;
				break;
			}
			/* fallthrough */
		case OP_W: /* 16 */
			reg_size = REG_SIZE_16;
			break;
		case OP_V: /* 16/32/64 */
			if(state->opd_size == SIZE_64) {
				if(REXW(state->rex)) {
					reg_size = REG_SIZE_64;
					break;
				}
			}
			if(state->opd_size == SIZE_16)	{
				reg_size = REG_SIZE_16;
				break;
			}
			/* fallthrough */
		case OP_D: /* 32 */
			reg_size = REG_SIZE_32;
			break;
		case OP_Y:
			if(state->opd_size == SIZE_64) {
				if(REXW(state->rex)) {
					reg_size = REG_SIZE_64;
					break;
				}
			}
			/* fallthrough */
		case OP_SI: /* 32 */
			reg_size = REG_SIZE_32;
			break;
		default:
			fprintf(stderr, "%s: %d: Unexpected operand %d\n", __FILE__, __LINE__, op);
			break;
	}

	// TODO: reg_field mantiene il codice del registro, a questo punto
	// potremmo aggiungere un campo che ne tiene traccia per l'emissione
	// dell'istruzione assembly inversa
	//
	// TODO: (2) Questa cosa scritta qui sopra è quanto ha implementato Simone
	// qui sotto
	//
	// Il campo reg_size può essere usato per discriminare la dimensione del
	// registro e quindi impostare il nome del registro correttamente
	(void)reg_size;

	// [SE] Hack terribile per ottenere il codice del registro destinazione
	state->instrument->reg_dest = reg_field;
}

/* format_addr_i
 * Dati immediati
 */
void format_addr_i (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	// I dati immediati sono di 1, 2 o 4 byte
	int immed_size = 0;
	uint8_t byte;
	uint16_t word;
	uint32_t dword;
	uint64_t qword, immed_data = 0;
	(void)addr;

	switch(op) {
		case OP_B: /* 8 */
			immed_size = 1;
			break;
		case OP_C: /* 8/16 */
			if(state->opd_size != SIZE_16) {
				immed_size = 1;
				break;
			}
			/* fallthrough */
		case OP_W: /* 16 */
			immed_size = 2;
			break;
		case OP_V: /* 16/32 */
			// Eccezione al funzionamento dei dati immediati: le mov con opcode 0xb8 - 0xbf permettodono di avere
			// come operando anche un dato immediato a 64 bit, qualora REX.W = 1.
			if(state->opcode[0] >= 0xb8 && state->opcode[0] <= 0xbf) {
				if(state->mode64 && REXW(state->rex)) {
					immed_size = 8;
					break;
				}
			}
			if(state->opd_size == SIZE_16) {
				immed_size = 2;
				break;
			}
			/* fallthrough */
		case OP_D: /* 32 */
			immed_size = 4;
			break;
		default:
			fprintf(stderr, "%s: %d: Unexpected operand %d\n", __FILE__, __LINE__, op);
			break;
	}


	switch(immed_size) {
		case 1:
			memcpy(&byte, state->text + state->pos, immed_size);
			immed_data = byte;
			break;
		case 2:
			memcpy(&word, state->text + state->pos, immed_size);
			immed_data = word;
			break;
		case 4:
			memcpy(&dword, state->text + state->pos, immed_size);
			immed_data = dword;
			break;
		case 8:
			memcpy(&qword, state->text + state->pos, immed_size);
			immed_data = qword;
			break;
		default:
			fprintf(stderr, "%s: %d: Unexpected size %d\n", __FILE__, __LINE__, immed_size);
			break;
	}

	// A questo punto immed_data contiene i dati immediati dell'istruzione
  // [SE] Populating immed_* fields
  state->instrument->immed_offset = state->immed_offset = state->pos;
  state->instrument->immed_size = state->immed_size = immed_size;
  state->instrument->immed = immed_data;

	state->pos += immed_size; // Salta i dati immediati appena gestiti
}

/* format_addr_j
 * L'istruzione contiene un offset relativo a EIP
 */
void format_addr_j (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	int off_size = 0, jump_size = 0;
	int8_t byte_jump;
	int16_t word_jump;
	int32_t dword_jump;
	(void)addr;

	switch(op) {
		case OP_B: /* 8 */
			off_size = 1;
			break;
		case OP_C: /* 8/16 */
			if(state->opd_size != SIZE_16) {
				off_size = 1;
				break;
			}
			/* fallthrough */
		case OP_W: /* 16 */
			off_size = 2;
			break;
		case OP_V: /* 16/32 */
			if(state->opd_size == SIZE_16)	{
				off_size = 2;
				break;
			}
			/* fallthrough */
		case OP_D: /* 32 */
			off_size = 4;
			break;
		default:
			fprintf(stderr, "%s: %d: Unexpected operand %d\n", __FILE__, __LINE__, op);
			break;
	}

	jump_size = off_size;

	switch(off_size) {
		case 4:
			memcpy(&dword_jump, state->text + state->pos, off_size);
			break;
		case 2:
			memcpy(&word_jump, state->text + state->pos, off_size);
			break;
		case 1:
			memcpy(&byte_jump, state->text + state->pos, off_size);
			if(state->opd_size == SIZE_16) { // Estensione del segno
				word_jump = (int16_t)byte_jump;
				jump_size = 2;
			} else {
				dword_jump = (int32_t)byte_jump;
				jump_size = 4;
			}
			break;
		default:
			fprintf(stderr, "%s: errore interno alla riga %d: %d\n", __FILE__, __LINE__, jump_size);
			return;
	}

	state->pos += off_size;

	if(jump_size == 2) { // Indirizzi a 16 bit
		state->instrument->jump_dest = (int32_t)word_jump;
	} else { // Indirizzi a 32 bit
		state->instrument->jump_dest = dword_jump;
	}
}

/* format_addr_m
 * Il byte ModR/M può riferirsi soltanto a memoria, e il formato della memoria è terribile:
 *   [base + index * scale + displacement]
 * Tutti gli elementi sono opzionali, anche se ci deve essere almeno "qualcosa".
 * Per i 16 e i 32 bit, nel volume 2A del SDM ci sono delle tabelle molto utili, a metà
 * del capitolo 2. Per i 64bit, la spiegazione è un po' meno accurata, ma il funzionamento
 * è più o meno simile all'indirizzamento per i 32 bit.
 */
void format_addr_m (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	bool no_sib_base = false;
	unsigned char mod, rm;
	unsigned long mem_ref;

	// Determina la dimensione della scrittura/lettura in memoria
	select_operand_size(state, op);

	if(addr == ADDR_M && op == OP_P) {
		format_addr_a(state, addr, op);
		return;
	}

	mod = state->modrm >> 6;
	rm = state->modrm & 0x07;

	if(mod == 0x3) { // Non dovrebbe verificarsi qui. Ma un po' di paranoia è buona
		/* Potrebbe accadere con l'opcode 62
		 * 62 f8 per esattezza
		 * mod = 11
		 * reg = 111
		 * r/m = 000
		 */
		fprintf(stderr, "%s: errore interno alla riga %d: ModR/M %#02x\n", __FILE__, __LINE__, state->modrm);
		return;
	}

	// Gestisce separatamente i 16 e i 32 bit
	if(state->addr_size == SIZE_16) {
		uint8_t disp8;
		uint16_t disp16;
		char *eff_addr[] = { "bx + si", "bx + di", "bp + si", "bp + di",
				     "si", "di", "bp", "bx" };

		// Se Mod è 00b e R/M è 110b, allora c'è solo uno spiazzamento a 16 bit
		if(mod == 0x0 && rm == 0x6) {
			memcpy(&disp16, state->text + state->disp_offset, 2);

			// Riferimento in memoria
			mem_ref = (unsigned long)disp16;

			state->instrument->addr = mem_ref;

			return;
		}

		// Copia il registro di base
		// [FV] if(!state->read_dest) {

		state->instrument->has_base_register = true;
		state->instrument->breg = rm;
		strcpy(state->instrument->breg_mnem, eff_addr[rm]);

		//Alice
		// Controlla se si sta accedendo a un indirizzo a partire dallo stack
		if(rm == 0x2 || rm == 0x3 || rm == 0x6) {

			state->instrument->flags |= I_STACK;
		}


		// [FV] }

		// Se Mod è 01b o 10b allora c'è, rispettivamente, uno spiazzamento di 8 o 16 bit

		// [FV] if(!state->read_dest) {

		if(mod == 0x1)	{
			memcpy (&disp8, state->text + state->disp_offset, 1);
			state->instrument->addr = (int16_t)disp8;
		} else if(mod == 0x2) {
			memcpy (&disp16, state->text + state->disp_offset, 2);
			state->instrument->addr = disp16;
		}

		// [FV] }

	} else { // Indirizzi a 32 o 64 bit
		uint8_t disp8;
		uint32_t disp32;
		char *eff_addr_32[] = { "eax", "ecx", "edx", "ebx",
					"", "ebp", "esi", "edi" };
		char *eff_addr_64[] = { "rax", "rcx", "rdx", "rbx",
					"", "rbp", "rsi", "rdi",
					"r8", "r9", "r10", "r11",
					"r12", "r13", "r14", "r15"};
		char **eff_addr = eff_addr_32;

		// Determina se si indirizzano registri a 32 o a 64 bit
		if(state->mode64)
			eff_addr = eff_addr_64;

		// Se Mod è 00b e R/M  è 101b, c'è solo uno spiazzamento di 32 bit
		if(mod == 0x0 && rm == 0x5) {

			// A 64 bit è RIP-Relative: non va instrumentato
			if(state->mode64) {
				state->uses_rip = true;
			} else {

				memcpy (&disp32, state->text + state->disp_offset, 4);

				// Riferimento in memoria
				mem_ref = (unsigned long)disp32;

				state->instrument->addr = mem_ref;
			}

			return;
		}

		// Se R/M è 100b, allora c'è il SIB che specifica l'operando
		if(rm == 0x4) {
			unsigned char ss, idx, base;
			char *base_r_32[] = { "eax", "ecx", "edx", "ebx",
					      "esp", "", "esi", "edi" };
			char *idx_r_32[] = { "eax", "ecx", "edx", "ebx",
					     "", "ebp", "esi", "edi" };
			char *base_r_64[] = { "rax", "rcx", "rdx", "rbx",
					      "rsp", "", "rsi", "rdi",
					      "r8", "r9", "r10", "r11",
					      "r12", "r13", "r14", "r15" };
			char *idx_r_64[] = { "rax", "rcx", "rdx", "rbx",
					     "", "rbp", "rsi", "rdi",
					      "r8", "r9", "r10", "r11",
					      "r12", "r13", "r14", "r15" };

			char **base_r = base_r_32;
			char **idx_r = idx_r_32;

			// Determina se si indirizzano registri a 32 o 64 bit
			if(state->mode64) {
				base_r = base_r_64;
				idx_r = idx_r_64;
			}

			ss = state->sib >> 6;
			idx = (state->sib >> 3) & 0x07;
			base = state->sib & 0x07;

			// Gestisce i registri estesi a 64 bit
			if(state->addr_size == SIZE_64) {
				// Estensione di idx
				if(REXX(state->rex))
					idx |= 0x08;
				// Estensione di base
				if(REXB(state->rex))
					base |= 0x08;
			}

			// Determina il registro base
			if(mod == 0x0 && (base == 0x5 || base == 0xd))
				no_sib_base = true;
			else {
				// [FV] if(!state->read_dest) {

				strcpy(state->instrument->breg_mnem, base_r[base]);
				state->instrument->breg = base;
				state->instrument->has_base_register = true;

				//Alice
				if(base == 0x04) {	// esp

					state->instrument->flags |= I_STACK;
				}

				// [FV] }
			}

			// Se c'è un registro indice
			if(idx != 0x4 && idx != 0xc) {

				// Controlla la scala
				// [FV] if(!state->read_dest) {

				switch(ss) {
					case 1:
						state->instrument->has_scale = true;
						state->instrument->scale = 2;
						break;
					case 2:
						state->instrument->has_scale = true;
						state->instrument->scale = 4;
						break;
					case 3:
						state->instrument->has_scale = true;
						state->instrument->scale = 8;
				}

				// [FV] }

				// Copia il registro indice
				// [FV] if(!state->read_dest) {

				state->instrument->has_index_register = true;
				state->instrument->ireg = idx;
				strcat(state->instrument->ireg_mnem, idx_r[idx]);

				//Alice
				if(idx == 0x05) {	// ebp

					state->instrument->flags |= I_STACK;
				}


				// [FV] }
			}
		} else { // Non c'è SIB

			// Gestisce i registri estesi a 64 bit
			if(state->addr_size == SIZE_64) {
				// Estensione di base
				if(REXB(state->rex))
					rm |= 0x08;
			}

			// [FV] if(!state->read_dest) {

			strcpy(state->instrument->breg_mnem, eff_addr[rm]);
			state->instrument->breg = rm;
			state->instrument->has_base_register = true;

			//Alice
			if(rm == 0x05) {	// ebp


				state->instrument->flags |= I_STACK;
			}


			// [FV] }
		}

		// Se Mod è 01b o 10b allora c'è, rispettivamente, uno spiazzamento
		// di 8 o 32 bit

		if(mod == 0x1)	{
			memcpy(&disp8, state->text + state->disp_offset, 1);
			state->instrument->addr = (unsigned long)disp8;
		} else if(mod == 0x2) {
			memcpy(&disp32, state->text + state->disp_offset, 4);
			state->instrument->addr = disp32;
		} else if(no_sib_base) {
			memcpy(&disp32, state->text + state->pos, 4);
			state->instrument->addr = disp32;
			state->disp_offset = state->pos; // TODO: questa roba andrebbe spostata in disp_size, anche se è una delle uniche due eccezioni...
			state->pos += 4;
		}
	}
}

/* format_addr_o
 * Byte ModR/M non presente. C'è un offset di dimensione ADDR_SIZE subito dopo l'istruzione
 */
void format_addr_o (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	uint8_t boff;
	uint16_t woff;
	uint32_t doff;
	uint64_t qoff;
	int offsize = 0;

	(void)addr;

	// Determina la dimensione della scrittura/lettura in memoria
	// [FV] if(!state->read_dest)

	select_operand_size(state, op);

	switch(state->addr_size) {
		// [FV] Aggiunto un case
		case SIZE_8:
			offsize = 1;
			break;
		case SIZE_16:
			offsize = 2;
			break;
		case SIZE_32:
			offsize = 4;
			break;
		case SIZE_64:
			offsize = 8;
			break;
		default:
			printf("Caso di default...\n");
	}

	if(offsize == 8)
		memcpy(&qoff, state->text + state->pos, offsize);
	else if(offsize == 4)
		memcpy(&doff, state->text + state->pos, offsize);
	else if(offsize == 2)
		memcpy(&woff, state->text + state->pos, offsize);
	// [FV] Aggiunto il seguente else
	else
		memcpy(&boff, state->text + state->pos, offsize);

	// TODO: La gestione del disp_offset andrebbe spostata alla macro disp_size,
	//	 anche se qui non viene utilizzato il byte ModR/M
	state->disp_offset = state->pos;

	// Fa avanzare il puntatore del parser
	state->pos += offsize;

	// Memorizza l'indirizzo in memoria
	switch(offsize) {
		case 8:
			state->instrument->addr = qoff;
			break;
		case 4:
			state->instrument->addr = doff;
			break;
		case 2:
			state->instrument->addr = woff;
			break;
		// [FV] Aggiunto il seguente case
		case 1:
			state->instrument->addr = boff;
	}
}

/* format_addr_p
 * Il campo reg del byte ModR/M seleziona un registro MMX.
 */
void format_addr_p (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)state;
	(void)addr;
	(void)op;

	// Questo è il numero del registro a 64 bit: (state->modrm >> 3) & 0x07
}

/* format_addr_n [FV]
 * Il campo R/M del byte ModR/M seleziona un registro MMX.
 */
void format_addr_n (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)state;
	(void)addr;
	(void)op;

	// Questo è il numero del registro a 64 bit: (state->modrm & 0x07)
}

/* format_addr_q
 * Il byte ModR/M specifica una posizione in memoria oppure un registro MMX
 */
void format_addr_q (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	if(state->modrm >> 6 == 0x3) { // Specifica un registro a 64 bit
		// [FV] Non legge, né scrive
		state->instrument->flags &= ~I_MEMRD & ~I_MEMWR; //Alice: state => flags
	}
	else {
		// [FV] Altrimenti non è I_MMX
		if(state->addr[0] != ADDR_P && state->addr[1] != ADDR_P && // [FV] Finora presenti solo ai primi due operandi
		   state->addr[0] != ADDR_N && state->addr[1] != ADDR_N)
			state->instrument->flags &= ~I_MMX;
		format_addr_m (state, addr, op);
	}
}

/* format_addr_r
 * Il campo Mod del byte ModR/M può soltanto riferirsi ad un registro general purpose
 */
void format_addr_r (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;

	// Viene usato soltanto da OP_D
	if(op != OP_D) {
		fprintf(stderr, "%s:%d: Errore interno\n", __FILE__, __LINE__);
		abort();
	}

	// Identifica il registro
	char reg = state->modrm & 0x07;
	if(state->mode64 && REXR(state->rex))
		reg |= 0x08;

	// A questo punto, reg identifica il registro per la modalità d'esecuzione corrente
}

/* format_addr_s
 * Il campo Reg del byte ModR/M seleziona un segment register
 */
void format_addr_s (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)state;
	(void)addr;
	(void)op;

	// I segment register sono:
	// es, cs, ss, ds, fs, gs, r_reg, r_seg
	// Gli r_seg sono segment register riservati
	// Il numero di registro è selezionato da: (state->modrm >> 3) & 0x07
}

/* format_addr_t
 * Il campo Reg del byte ModR/M sleziona un registro di test. Dopo un po'
 * di tempo passato sul SDM, ho capito che i registri di test erano usati
 * soltanto nell'80486, sono stati soppiantati dal Pentium in poi dai MSR
 * e con l'avvento dell'architettura P6, un'istruzione di mov che coinvolge
 * un registro di test genera un'eccezione UD. In ogni caso, li prendo
 * comunque in considerazione, anche se è abbastanza inutile. Molto probabilmente
 * questa funzione (e tutte le chiamate ad essa) possono essere cancellate senza
 * problemi.
 */
void format_addr_t (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)state;
	(void)addr;
	(void)op;

	// Il registro di test (se presente) è selezionato da (state->modrm >> 3) & 0x07
}

/* format_addr_v
 * Il campo Reg del byte ModR/M seleziona un registro XMM
 */
void format_addr_v (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;
	(void)op;

	char reg = (state->modrm >> 3) & 0x07;
	if(state->mode64 && REXR(state->rex))
		reg |= 0x08;

}

/* format_addr_u [FV]
 * Il campo R/M del byte ModR/M seleziona un registro XMM
 */
void format_addr_u (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;
	(void)op;

	char reg = state->modrm & 0x07;
	if(state->mode64 && REXR(state->rex))
		reg |= 0x08;
}

/* format_addr_w
 * Il byte ModR/M specifica o un registro XMM o la memoria
 */
void format_addr_w (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	if(state->modrm >> 6 == 0x3) {
		char reg = (state->modrm >> 3) & 0x07;

		// [FV] Ne' legge, ne' scrive (da verificare)
		state->instrument->flags &= ~I_MEMRD & ~I_MEMWR;

		if(state->mode64 && REXR(state->rex))
			reg |= 0x08;
	} else {
		if(state->addr[0] != ADDR_U && state->addr[0] != ADDR_V && /* state->addr[0] != ADDR_L &&*/
		state->addr[1] != ADDR_U && state->addr[1] != ADDR_V /*&& state->addr[1] != ADDR_L */)
		// [FV] Finora tali addressing methods sono presenti solo ai primi due operandi
			state->instrument->flags &= ~I_XMM;
		format_addr_m(state, addr, op);
	}
}

/* format_addr_x
 * X è un operando implicito, il suffisso nell'istruzione è b
 */
void format_addr_x (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;
	(void)op;

	// La dimensione è sicuramente 1 byte
	state->instrument->span = 1;

	// È un'istruzione che lavora sulle stringhe
	// [FV] state->instrument->is_string_insn = true;
}

/* format_addr_y
 * Y è un operando implicito, il suffisso nell'istruzione è l o w o q a seconda
 */
void format_addr_y (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{
	(void)addr;

	// Determina la dimensione della scrittura/lettura in memoria
	// [FV] if(!state->read_dest)
	select_operand_size(state, op);

	// È un'istruzione che lavora sulle stringhe
	// [FV] state->instrument->is_string_insn = true;
}


/* format_addr_op
 * Questa funzione decide quale tipo di indirizzamento viene usato dall'istruzione
 * (se a registro o a memoria, qual è la dimensione dei dati, ...)
 */
void format_addr_op (struct disassembly_state *state, enum addr_method addr, enum operand_type op)
{

	switch(addr) {

		case ADDR_N:
			format_addr_n(state, addr, op);
			break;

		case ADDR_U:
			format_addr_u(state, addr, op);
			break;

		case ADDR_A:
			format_addr_a(state, addr, op);
			break;

		case ADDR_C:
			format_addr_c(state, addr, op);
			break;

		case ADDR_D:
			format_addr_d(state, addr, op);
			break;

		case ADDR_E: // Può esserci accesso a memoria
			format_addr_e(state, addr, op);
			break;

		case ADDR_F:
			// Non occorre fare nulla per questo metodo di indirizzamento.
			// È utilizzato soltanto da pushf/popf e si riferisce sempre e solo ad eflags
			break;

		case ADDR_G:
			// [FV] Si tratta di un registro
			format_addr_g(state, addr, op);
			break;

		case ADDR_I:
			// [FV] Si tratta di un dato immediato
			format_addr_i(state, addr, op);
			break;

		case ADDR_J:
			// [FV] state->instrument->is_jmp = true;
			format_addr_j(state, addr, op);
			break;

		case ADDR_M:
			format_addr_m(state, addr, op);
			break;

		case ADDR_O: // Ci sono due mov (A2 e A3) che scrivono su memoria
			// [FV] ... mentre A0 e A1 leggono dalla memoria
			format_addr_o(state, addr, op);
			break;

		case ADDR_P:
			format_addr_p(state, addr, op);
			break;

		case ADDR_Q: // Può esserci accesso a memoria
			format_addr_q(state, addr, op);
			break;

		case ADDR_R:
			format_addr_r(state, addr, op);
			break;

		case ADDR_S:
			format_addr_s(state, addr, op);
			break;

		case ADDR_T:
			break;

		case ADDR_V:
			format_addr_v(state, addr, op);
			break;

		case ADDR_W: // Può esserci accesso a memoria...
			format_addr_w(state, addr, op);
			break;

		case ADDR_X: // Può esserci accesso a memoria
			format_addr_x(state, addr, op);
			break;

		case ADDR_Y: // Può esserci accesso a memoria
			format_addr_y(state, addr, op);
			break;

		case R_START ... R_END:
			// I registri possono avere dimensioni diverse (es: [e]ax)
			// Per le dimensioni a 64 bit vale il fatto che se REX.B == 1, allora reg |= 0x08
			break;

		case IMMED_1:
			// Il valore 1...
			break;

		default:
			fprintf(stderr, "%s:%d: Unexpected address format %d\n", __FILE__, __LINE__, addr);
	}
}

/* x86_disassemble_instruction
 * Disassembla l'istruzione a text + *pos e restituisce una
 * riga di assembly dopo aver aggiornato *pos
 */
void x86_disassemble_instruction (unsigned char *text, unsigned long *pos, insn_info_x86 *instrument, char flags)
{
	int k = 0;
	bool print_prefixes = false; // Shall this become useful in the future?
	unsigned char opcode;
	insn_table table = one_byte_opcode_table;
	struct disassembly_state state;

	state.text = text;
	state.pos = *pos;
	state.disp_offset = 0;

	state.opcode[0] = 0x00;
	state.opcode[1] = 0x00;

	state.instrument = instrument;
	state.instrument->initial = *pos;

	// In realtà è un'affermazione un po' forte dire che se non è né a 64 né a 32 è a 16
	// Pare che gli ELF non prevedano codice a 16 bit...
	// In questo caso, probabilmente molto del lavoro fatto per supportare i 16
	// bit è stato inutile, ma devo ricontrollare in quali casi gli opcode
	// specificano i dati a 16 bit... E soprattutto, devo controllare
	// se GAS genera mai quegli opcode...
	// Credo che Linux non tratti più codice a 16 bit (e in effetti avrebbe ragione,
	// visto che è dall'8088 che non ci sono più processori a 16 bit).
	// Immagino che questo sproloquio corrisponda a un TODO.
	state.opd_size = D64(flags) || D32(flags) ? SIZE_32 : SIZE_16;
	state.addr_size = A64(flags) || A32(flags) ? SIZE_32 : SIZE_16;

	state.mode64 = D64(flags) ? true : false;

	// [FV] Imposto inizialmente a false il flag "uses_rip"
	state.uses_rip = false;

	state.rex = 0;
	state.modrm = 0;
	state.read_modrm = false;
	state.sib = 0;

	state.read_dest = false;

	state.sse_prefix = 0;
	state.prefix[0] = 0;
	state.prefix[1] = 0;
	state.prefix[2] = 0;
	state.prefix[3] = 0;

	state.orig_pos = *pos;

	/* Controlla e gestisce prefissi - alcune istruzioni xmm ed SSE hanno prefissi
	   che non sono prefissi, pertanto bisogna rinviare la gestione dei prefissi
	   quanto più possibile... */
	while(true) {
		opcode = state.text[state.pos++]; // Legge l'opcode
//		printf("x86.c_POS: %02x byte: %#02x\n", state.text[state.pos]-1, opcode);

		if(!is_prefix(opcode)) break; // I prefissi SSE sono anche prefissi normali (con significato diverso)

		// 66, F2, F3 - Prefissi SSE
		// Controlla il byte 3 dell'opcode
		if(is_sse_prefix(opcode))
			state.sse_prefix = opcode;

		// C'è un prefisso. Occorre capire di che tipo
		if(p_is_group1(opcode)) { /* lock/repne/repe */
			if(!state.prefix[0]) // Ignora prefissi addizionali
				state.prefix[0] = opcode;
		} else if(p_is_group2(opcode)) { /* segment override/branch hint */
			if(!state.prefix[1]) // Ignora prefissi addizionali
				state.prefix[1] = opcode;
		} else if(p_is_group3(opcode)) { /* operand size override prefix */
			if(!state.prefix[2]) // Ignora prefissi addizionali
				state.prefix[2] = opcode;
		} else if(p_is_group4(opcode)) { /* address size override prefix */
			if(!state.prefix[3]) // Ignora prefissi addizionali
				state.prefix[3] = opcode;
		} else { // mmm...
			fprintf(stderr, "%s:%d: Errore interno\n", __FILE__, __LINE__);
			abort();
		}

	}

	// Controlla se è presente il prefisso REX. Il byte REX, se presente, si trova
	// sempre tra i legacy-prefixes e l'opcode. Ce ne può essere più d'uno. In questo
	// caso, ha valore soltanto l'ultimo, gli altri vengono ignorati.
	while(true) {
		if(is_rex_prefix(opcode, state.mode64)) {
			state.rex = opcode;
			opcode = state.text[state.pos++];
			continue;
		}
		break;
	}

	// Recupera gli operandi e i metodi di indirizzamento dell'istruzione
	state.addr[0] = table[opcode].addr_method[0];
	state.addr[1] = table[opcode].addr_method[1];
	state.addr[2] = table[opcode].addr_method[2];

	state.op[0] = table[opcode].operand_type[0];
	state.op[1] = table[opcode].operand_type[1];
	state.op[2] = table[opcode].operand_type[2];


	// Controlla se l'istruzione è da instrumentare
	state.instrument->flags = table[opcode].flags;

	state.opcode[0] = opcode;

	// Salva l'istruzione
	if(table[opcode].instruction != NULL)
		strcpy(state.instrument->mnemonic, table[opcode].instruction);

	// Controlla opcode di escape
	if(table[opcode].instruction == NULL) { // byte di escape
		// Controllo di sicurezza
		if(table[opcode].esc_function == NULL) {
			fprintf(stderr, "%s:%d: Errore interno\n", __FILE__, __LINE__);
			abort();
		} else {
			table[opcode].esc_function(&state);
		}
	} else { // Istruzione normale
	}

	// Se c'è un prefisso di gruppo 2, controlla se c'è un'istruzione Jcc
	if(state.prefix[1]) {
		if(is_jcc_insn (state.opcode[0])	// branch hint prefix
		    || (state.opcode[0] == 0x0f && is_esc_jcc_insn(state.opcode[1]))) {
			if(print_prefixes) {
				if(state.prefix[1] == 0x2e) {// branch not taken
				}
				else if(state.prefix[1] == 0x3e) { // branch taken
				}
				else { 	// invalid branch hint
					fprintf(stderr, "%s:%d: Errore interno\n", __FILE__, __LINE__);
					abort();
				}
			}
			state.prefix[1] = 0;
		} else {  // segment override prefix
		}
	}
	if(state.prefix[2] == 0x66) {
		state.opd_size = ((state.opd_size == SIZE_32) || (state.opd_size == SIZE_64)) ? SIZE_16 : SIZE_32;
	}
	if(state.prefix[3] == 0x67) {
		state.addr_size = ((state.addr_size == SIZE_32) || (state.addr_size == SIZE_64)) ? SIZE_16 : SIZE_32;
	}

	/* Cerca gli offset di varie parti dell'istruzione */

	// Controlla il byte ModR/M
	if(!state.read_modrm && (has_modrm(state.addr[0])
	    || has_modrm(state.addr[1]) || has_modrm(state.addr[2]))) {
		state.modrm = state.text[state.pos];
		state.pos++;
	}

	// Controlla il byte SIB
	if(has_sib(state.modrm, state.addr_size)) {
		state.sib = state.text[state.pos];
		state.pos++;
	}

	// Controlla il displacement
	state.disp_size = disp_size(state.modrm, state.addr_size);

	// [DC] Registra la dimensione dell'opcode, del prefisso, R/M e SIB delle istruzioni per gestire correttamente
	// la rilocazione nella fase di emissione del file instrumentato
	state.instrument->opcode_size = (state.pos - state.orig_pos);

	switch(state.disp_size) {
		case 8:
			state.instrument->disp = (long long) *(int64_t *)(state.text + state.pos);
			state.disp_offset = state.pos;
			state.pos += state.disp_size;
			break;
		case 4:
			state.instrument->disp = (long long) *(int32_t *)(state.text + state.pos);
			state.disp_offset = state.pos;
			state.pos += state.disp_size;
			break;
		case 2:
			state.instrument->disp = (long long) *(int16_t *)(state.text + state.pos);
			state.disp_offset = state.pos;
			state.pos += state.disp_size;
			break;
		case 1:
			state.instrument->disp = (long long) *(int8_t *)(state.text + state.pos);
			state.disp_offset = state.pos;
			state.pos += state.disp_size;
			break;
	}

	//state.instrument->opcode_size = (state.pos - state.orig_pos);

	// A questo punto, state.pos o è l'offset dei dati immediati, oppure
	// l'offset dell'istruzione seguente
	// I dati immediati vengono smaltiti dalle funzioni di formattazione.

	// Gestisce addr/op
	for(k = 0; k < 3; k++) {

		if(state.addr[k] == ADDR_0) break; // Finito!

		// Ogni istruzione può avere fino a tre operandi. Tipicamente il primo
		// è l'operando di destinazione. Effettuiamo un controllo su questo
		// operando per vedere se l'istruzione accede in memoria!
		format_addr_op(&state, state.addr[k], state.op[k]);
		state.read_dest = true;

		// [SE] Hack terribile per capire se un registro è usato come destinazione
		if (k == 0 && state.addr[k] == ADDR_G) {
			state.instrument->dest_is_reg = true;
		}
	}

	// Copia i byte dell'istruzione
	memcpy(state.instrument->insn, &(state.text[*pos]), state.pos - *pos);

	// Copia il displacement (se l'istruzione è da instrumentare ed accede a memoria)
	// [FV] Adesso copio il displacement indipendentemente da alcuna condizione
	// [FV] if(state.instrument->to_instrument && state.instrument->to_memory)
	state.instrument->disp_offset = state.disp_offset;
	state.instrument->disp_size = state.disp_size;


	// [FV] Copia il flag "uses_rip"
	state.instrument->uses_rip = state.uses_rip;


	// Copia i byte dell'opcode
	memcpy(state.instrument->opcode, state.opcode, 2);

	// [SE] Copia i rimanenti campi
	state.instrument->rex = state.rex;
	state.instrument->modrm = state.modrm;
	state.instrument->sib = state.sib;
	state.instrument->sse_prefix = state.sse_prefix;
	memcpy(state.instrument->prefix, state.prefix, 4);

	state.instrument->insn_size = (state.pos - *pos);
	*pos = state.pos;

}

#endif /* defined(__x86_64__) && defined(HAVE_ECS) */

