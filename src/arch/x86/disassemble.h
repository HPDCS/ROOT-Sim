/**
 * @file arch/x86/disassemble.h
 *
 * @brief x86 ISA disassembler header
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

#pragma once

#if defined(__x86_64__)

#include <stdint.h>
#include <stdbool.h>

/// Data fields in instructions are 32-bits
#define	DATA_32 0x01
/// Address fields in instructions are 32-bits
#define	ADDR_32 0x02
/// Data fields in instructions are 64-bits
#define	DATA_64 0x04
/// Address fields in instructions are 64-bits
#define	ADDR_64 0x08


typedef struct insn_info_x86 {
	unsigned long flags;		// Insieme di flags contenente informazioni utili generiche riguardo l'istruzione
	unsigned char insn[15];		// I byte dell'istruzione (15 è il limite massimo)
	unsigned char opcode[2];	// L'opcode dell'istruzione
	char mnemonic[16];		// Il nome dell'istruzione
	unsigned long initial;		// Posizione iniziale nel testo
	unsigned long insn_size;	// Lunghezza dell'istruzione
	unsigned long addr;		// Indirizzo puntato dall'istruzione, o 0x00
	unsigned long span;		// Quanto in memoria verrà riscritto/letto, o 0x00
	bool has_index_register;	// L'indirizzamento sfrutta un indice?
	unsigned char ireg;		// Quale registro contiene l'indice?
	char ireg_mnem[8];		// Mnemonico del registro di indice
	bool has_base_register;		// L'indirizzamento sfrutta una base?
	unsigned char breg;		// Quale registro contiene la base?
	char breg_mnem[8];		// Mnemonico del registro
	bool has_scale;			// L'indirizzamento utilizza una scala
	unsigned long scale;		// La scala
	unsigned long disp_offset;	// Lo spiazzamento del displacement dall'inizio del testo, o 0x00
	int disp_size;			// Dimensione in byte del displacement, o 0x00
	long long disp;			// Il valore dello spiazzamento
	unsigned long immed_offset;	// [SE] Lo spiazzamento dei dati immediati dall'inizio del testo, o 0x00
	int immed_size;			// [SE] La dimensione dei dati immediati, o 0x00
	unsigned long long immed;	// [SE] Il valore dei dati immediati
	unsigned int opcode_size;	// [DC] Dimensione dell'opcode per l'istruzione
	int32_t jump_dest;		// Dove punta la jmp
	bool uses_rip;
	unsigned char rex;		// Il byte REX, o 0x00
	unsigned char modrm;		// Byte ModR/M o 0x00
	unsigned char sib;		// Byte SIB o 0x00
	unsigned char sse_prefix;	// Terzo byte dell'istruzione SSE/SSE2
	unsigned char prefix[4];	// Prefissi all'istruzione o 0x00

	bool dest_is_reg;   // [SE] Indica se la destinazione è un registro
	unsigned char reg_dest;   // [SE] Codice del registro destinazione (se esiste)
} insn_info_x86;

// [FV] Flags contenenti informazioni utili sul comportamento o la classe delle funzioni
#define I_MEMRD		0x1	// Legge dalla memoria
#define I_MEMWR		0x2	// Scrive sulla memoria
#define I_CTRL 		0x4	// Istruzioni della famiglia "test" e "control" che controllano valori di dati o singoli bit
#define I_JUMP 		0x8	// Istruzioni della famiglia "jump", che modificano il flusso di esecuzione del processore
#define I_CALL	 	0x10	// Istruzione del tipo "call"
#define I_RET 		0x20	// Istruzione del tipo "call"
#define I_CONDITIONAL	0x40	// Istruzione che viene eseguite se e solo se e' soddisfatta una condizione
#define I_STRING	0x80	// Opera su stringhe (e.g. movs, stos, cmps...)
#define I_ALU		0x100	// Esegue operazioni di tipo logico-aritmetico
#define I_FPU		0x200	// Utilizza la Floating Point Unit (FPU)
#define I_MMX		0x400	// Istruzione che utilizza i registri MMX
#define I_XMM		0x800	// Istruzione che utilizza i registri XMM
#define I_SSE		0x1000	// Istruzione SSE
#define I_SSE2		0x2000	// Istruzione SSE2
#define I_PUSHPOP	0x4000	// Istruzione di tipo "push" o di tipo "pop"
#define I_STACK		0x8000	// Se l'istruzione opera nello stack
#define I_JUMPIND	0x10000	// Indirect Branch
#define I_CALLIND 	0x20000 // [SE] Indirect Call
#define I_MEMIND	0x40000 // [SE] Indirect memory address load (LEA)

// [FV] Macro per il testing dei flags
#define IS_MEMRD(X)		((X)->flags & I_MEMRD)
#define IS_MEMWR(X)		((X)->flags & I_MEMWR)
#define IS_MEMIND(X)		((X)->flags & I_MEMIND)
#define IS_CTRL(X)		((X)->flags & I_CTRL)
#define IS_JUMP(X)		((X)->flags & I_JUMP)
#define IS_JUMPIND(X)		((X)->flags & I_JUMPIND)
#define IS_CALL(X)		((X)->flags & I_CALL)
#define IS_CALLIND(X)		((X)->flags & I_CALLIND)
#define IS_RET(X)		((X)->flags & I_RET)
#define IS_CONDITIONAL(X)	((X)->flags & I_CONDITIONAL)
#define IS_STRING(X)		((X)->flags & I_STRING)
#define IS_ALU(X)		((X)->flags & I_ALU)
#define IS_FPU(X)		((X)->flags & I_FPU)
#define IS_MMX(X)		((X)->flags & I_MMX)
#define IS_XMM(X)		((X)->flags & I_XMM)
#define IS_SSE(X)		((X)->flags & I_SSE)
#define IS_SSE2(X)		((X)->flags & I_SSE2)
#define IS_PUSHPOP(X)		((X)->flags & I_PUSHPOP)
#define IS_STACK(X)		((X)->flags & I_STACK)


// Strings to load macros from the configuration file
#define I_MEMRD_S	"I_MEMRD"
#define I_MEMWR_S	"I_MEMWR"
#define I_MEMIND_S	"I_MEMIND"
#define I_CTRL_S	"I_CTRL"
#define I_JUMP_S	"I_JUMP"
#define I_JUMPIND_S	"I_JUMPIND"
#define I_CALL_S	"I_CALL"
#define I_CALLIND_S	"I_CALLIND"
#define I_RET_S		"I_RET"
#define I_CONDITIONAL_S	"I_CONDITIONAL"
#define I_STRING_S	"I_STRING"
#define I_ALU_S		"I_ALU"
#define I_FPU_S		"I_FPU"
#define I_MMX_S		"I_MMX"
#define I_XMM_S		"I_XMM"
#define I_SSE_S		"I_SSE"
#define I_SSE2_S	"I_SSE2"
#define I_PUSHPOP_S	"I_PUSHPOP"
#define I_STACK_S	"I_STACK"

// Arch-Dependent Instruction Sets
#define UNRECOG_INSN	0
#define X86_INSN	7


#define A32(f) ((f) & ADDR_32) // Indirizzi a 32 bit?
#define D32(f) ((f) & DATA_32) // Dati a 32 bit?
#define A64(f) ((f) & ADDR_64) // Indirizzi a 64 bit?
#define D64(f) ((f) & DATA_64) // Dati a 64 bit?

/* Test sui prefissi */
#define p_is_group1(p) (((p) == 0xf0)     /* lock */ \
			|| ((p) == 0xf2)  /* repne/repnz */ \
			|| ((p) == 0xf3)) /* rep/repe/repz */

#define p_is_group2(p) (((p) == 0x2e)     /* CS override/branch not taken */ \
			|| ((p) == 0x36)  /* SS override */ \
			|| ((p) == 0x3e)  /* DS override/branch taken */ \
			|| ((p) == 0x26)  /* ES override */ \
			|| ((p) == 0x64)  /* FS override */ \
			|| ((p) == 0x65)) /* GS override */

#define p_is_group3(p) ((p) == 0x66) /* opsize override */

#define p_is_group4(p) ((p) == 0x67) /* addr size override */

#define is_prefix(o) (p_is_group1 (o) || p_is_group2 (o) \
		      || p_is_group3 (o) || p_is_group4 (o))

#define is_sse_prefix(o) (((o) == 0xf2) || ((o) == 0xf3) || ((o) == 0x66))

#define is_rex_prefix(r, mode64) (((r) >= 0x40 && (r) <= 0x4f ) && (mode64))


/* Recuperano i bit di interesse del byte REX (i primi 4 bit sono fissi e valgono 0100b) */

#define REXW(r) (((r) & 0x08) >> 3)
#define REXR(r) (((r) & 0x04) >> 2)
#define REXX(r) (((r) & 0x02) >> 1)
#define REXB(r) (((r) & 0x01))


/* Test per le operazioni Jcc */

// opcode nell'intervallo 70-7f,e3
#define is_jcc_insn(o) (((o) == 0xe3) || (((o) >= 0x70) && ((o) <= 0x7f)))

// opcode nell'intervallo 80-8f (quando il primo byte è 0f)
#define is_esc_jcc_insn(o) (((o) >= 0x80) && ((o) <= 0x8f))


// Gli operandi dell'istruzione specificano se c'è o meno un byte ModR/M
#define has_modrm(addr) (((addr) == ADDR_C)    \
			 || ((addr) == ADDR_D) \
			 || ((addr) == ADDR_E) \
			 || ((addr) == ADDR_G) \
			 || ((addr) == ADDR_M) \
			 || ((addr) == ADDR_P) \
			 || ((addr) == ADDR_Q) \
			 || ((addr) == ADDR_R) \
			 || ((addr) == ADDR_S) \
			 || ((addr) == ADDR_T) \
			 || ((addr) == ADDR_V) \
			 || ((addr) == ADDR_W))

// È presente un byte SIB se il campo Mod non è 11b, il campo R/M è
// 100b e la modalità di indirizzamento è a 32 bit o 64 bit
#define has_sib(modrm, addr) ((((modrm) & 0x07) == 0x04) \
			      && (((addr) == SIZE_32) || (addr) == SIZE_64) \
			      && (((modrm) & 0xC0) != 0xC0))

// Se il campo Mod è 01b, allora c'è uno spiazzamento di 1 byte. Se il campo
// Mod è 10b, e siamo in modalità di indirizzamento a 32/64 bit, allora c'è
// uno spiazzamento di 4 byte. Inoltre, se la modalità di indirizzamento è
// a 16 bit e il campo Mod è 10b, allora c'è uno spiazzamento di 2 byte.
// Se la modalità di indirizzamento è a 32 o 64 bit e sia il campo Mod è 00b sia
// il campo R/M è 101b, o il campo Mod è 10b, allora c'è uno spiazzamento
// di 4 byte. Altrimenti non c'è spiazzamento.
#define disp_size(modrm, addr) ((((modrm) & 0xC0) == 0x40) ? 1 \
			       : ((((addr) == SIZE_16) \
				   && ((((modrm) & 0xC7) == 0x06) \
				       || (((modrm) & 0xC0) == 0x80))) ? 2 \
				   : (((((addr) == SIZE_32) || (addr) == SIZE_64) \
				      && ((((modrm) & 0xC7) == 0x05) \
					  || (((modrm) & 0xC0) == 0x80))) ? 4\
				     : 0)))

enum addr_method {
  ADDR_0, /* Nessun metodo di indirizzamento */
  ADDR_A, /* Indirizzamento diretto, nessun byte ModR/M */
  ADDR_C, /* Il campo reg del byte ModR/M seleziona un registro di controllo */
  ADDR_D, /* Il campo reg del byte ModR/M seleziona un registro di debug */
  ADDR_E, /* Il byte ModR/M specifica l'operando: o un registro general purpose
	     o un offset per un indirizzo di memoria da un segment register
	     con un registro base, un registro d'indice, un fattore di scala
	     o spiazzamento */
  ADDR_F, /* registro EFLAGS */
  ADDR_G, /* Il campo reg del byte ModR/M seleziona un registro generale */
  ADDR_I, /* Dati immediati */
  ADDR_J, /* L'istruzione contiene un offset relativo, da (E)IP */
  ADDR_M, /* Il byte ModR/M può riferirsi solo a memoria */
  ADDR_N, /* [FV] Il campo R/M del byte ModR/M indica un registro MMX */
  ADDR_O, /* Nessun byte ModR/M. L'op è codificata come word o dword o qword in 64bit */
  ADDR_P, /* Il campo reg del byte ModR/M seleziona un registro packed qword MMX */
  ADDR_Q, /* Il byte ModR/M specifica o un registro MMX o un indirizzo in memoria (scala, ecc...) */
  ADDR_R, /* Il campo reg del byte ModR/M si può riferire solo a un registro generale */
  ADDR_S, /* Il campo reg del byte ModR/M seleziona un segment register */
  ADDR_T, /* Il campo reg del byte ModR/M seleziona un registro di test */
  ADDR_U, // Alice: Il campo R/M del byte ModR/M seleziona un registro XMM
  ADDR_V, /* Il campo reg del byte ModR/M seleziona un registro MMX */
  ADDR_W, /* Il byte ModR/M seleziona un registro XMM o un indirizzo in memoria (scala, ecc...) */
  ADDR_X, /* Indirizzo di memoria da DS:SI */
  ADDR_Y, /* Indirizzo di memoria da ES:DI */

  /* Registri */
  R_START,

  R_AL,
  R_AH,
  R_AX,
  R_EAX,
  R_RAX,

  R_BL,
  R_BH,
  R_BX,
  R_EBX,
  R_RBX,

  R_CL,
  R_CH,
  R_CX,
  R_ECX,
  R_RCX,

  R_DL,
  R_DH,
  R_DX,
  R_EDX,
  R_RDX,

  R_SIL,
  R_SI,
  R_ESI,
  R_RSI,
  R_DIL,
  R_DI,
  R_EDI,
  R_RDI,
  R_BP,
  R_EBP,
  R_SPL,
  R_SP,
  R_ESP,
  R_RSP,

  /* segment registers */
  R_CS,
  R_DS,
  R_SS,
  R_ES,
  R_FS,
  R_GS,

  /* EFLAGS */
  R_F,
  R_EF,

  /* EIP */
  R_IP,
  R_EIP,
  R_RIP,

  /* floating point registers */
  R_ST0,
  R_ST1,
  R_ST2,
  R_ST3,
  R_ST4,
  R_ST5,
  R_ST6,
  R_ST7,

  /* Extra registers in 64bit mode */
  R_R8L,
  R_R8W,
  R_R8D,
  R_R8,

  R_R9L,
  R_R9W,
  R_R9D,
  R_R9,

  R_R10L,
  R_R10W,
  R_R10D,
  R_R10,

  R_R11L,
  R_R11W,
  R_R11D,
  R_R11,

  R_R12L,
  R_R12W,
  R_R12D,
  R_R12,

  R_R13L,
  R_R13W,
  R_R13D,
  R_R13,

  R_R14L,
  R_R14W,
  R_R14D,
  R_R14,

  R_R15L,
  R_R15W,
  R_R15D,
  R_R15,


  /* MMX registers */
  R_MM0,
  R_MM1,
  R_MM2,
  R_MM3,
  R_MM4,
  R_MM5,
  R_MM6,
  R_MM7,

  /* SSE/SSE2 registers */
  R_XMM0,
  R_XMM1,
  R_XMM2,
  R_XMM3,
  R_XMM4,
  R_XMM5,
  R_XMM6,
  R_XMM7,
  R_XMM8,
  R_XMM9,
  R_XMM10,
  R_XMM11,
  R_XMM12,
  R_XMM13,
  R_XMM14,
  R_XMM15,

  R_END,

  /* Valore immediato interno all'istruzione (D0, D1) */
  IMMED_1, /* il valore 1 */
};

enum operand_type {
  OP_0,  /* nessun tipo */
  OP_A,  /* due oper da 1 word o 2 operandi dword in mem (dipende dall'attr opsize) */
  OP_B,  /* byte, indifferentemente dall'attrib opsize */
  OP_C,  /* byte o word, a seconda dell'attrib opsize */
  OP_D,  /* dword, indifferentemente dall'attributo opsize */
  OP_DQ, /* dqword, indifferentemebre dall'attr opsize */
  OP_P,  /* puntatore di 32 o 48 bit a seconda dell'attributo opsize */
  OP_PI, /* Registro MMX qword */
  OP_PS, /* 128 bit packed single float [FV] o 256 bit */
  OP_Q,  /* qword, indifferentemente dall'att opsize */
  OP_S,  /* 6 byte pseudo-descriptor */
  OP_SS, /* Elemento scalare di un packed single float a 128 bit */
  OP_SI, /* registro dword (es: eax) */
  OP_V,  /* word o dword, a seconda dell'attributo opsize */
  OP_W,  /* word, indifferentemente dall'attributo opsize */
  OP_PD, /* registri dqword o xmm [FV] 128 bit o 256 bit packed double-precision float */
  OP_SD, /* registri qword o xmm */
  OP_E,	 /* usato quando i registri sono codificati direttamente */
  OP_Y,	 /* [FV] doubleword o quadword (in modalita' 64 bit), a seconda dell'attributo operand-size */
  OP_FS,	 /* [FV] 14 o 24 byte in memoria, a seconda di operand-size (usato da istruzioni load/store FPU state) */
  OP_FSR,	 /* [FV] 94 o 108 byte in memoria, a seconda di operand-size (usato da istruzioni FRSTOR/FSAVE FPU
			  * state piu' 80 bit registri FPU) */
  OP_M80,	 /* 80 bit in memoria */
  OP_M512byte	 /* 512 byte in memoria */
};

enum op_size {
  SIZE_8, //Alice
  SIZE_16,
  SIZE_32,
  SIZE_64
};

/* dimensione operando / indirizzo */
enum reg_size {
  REG_SIZE_8, REG_SIZE_16, REG_SIZE_32, REG_SIZE_64, REG_SIZE_128
};


// Per tenere traccia dello stato dell'interpretazione
struct disassembly_state {
  enum op_size opd_size;	// Dimensione dell'operando corrente
  enum op_size addr_size;	// Dimensione dell'indirizzo corrente
  unsigned char *text;		// Sezione testo corrente (su cui si opera)
  unsigned long pos;		// Posizione corrente nel testo
  unsigned char rex;		// Il byte REX, o 0x00
  bool mode64;			// Stiamo eseguendo a 32 o a 64 bit?
  unsigned char opcode[2];	// Opcode corrente
  unsigned char modrm;		// Byte ModR/M o 0x00
  bool read_modrm;		// Indica se il byte ModR/M è stato letto
  unsigned char sib;		// Byte SIB o 0x00
  unsigned long disp_offset;	// L'inizio del displacement o 0
  int disp_size;		// Dimensione in byte del displacement
  unsigned long immed_offset;	// L'inizio dei dati immediati o 0
  int immed_size;		// La dimensione dei dati immediati
  enum addr_method addr[3];	// Codice per il metodo di indirizzamento
  enum operand_type op[3];	// Codice per il tipo di operando
  unsigned char prefix[4];	// Prefissi all'istruzione o 0x00
  unsigned char sse_prefix;	// Terzo byte dell'istruzione SSE/SSE2
  unsigned long orig_pos;	// Posizione iniziale nel testo
  bool read_dest;		// Indica se è stata analizzata la destinazione

  // [FV] Flag indicante se l'istruzione usi un indirizzamento RIP_Relative o meno
  bool uses_rip;

  insn_info_x86 *instrument;	// Struttura dati per gestire l'instrumentazione
};


struct _insn {
  char *instruction;	// Nome dell'istruzione (es: "mov")
  enum addr_method addr_method[3];	// Metodo di indirizzamento (Appendice A di IA-32 SDM)
  enum operand_type operand_type[3];	// Tipo corrispondete (sempre dall'appendice A)
  void (*esc_function)(struct disassembly_state *);	// Gestore dei byte addizionali dell'opcode
  unsigned long flags;	// Flag contenente info utili sull'istruzione (così come definiti in "instruction.h")
};

typedef struct _insn insn, *insn_table;


extern void x86_disassemble_instruction(unsigned char *text, unsigned long *pos, insn_info_x86 *instrument, char flags);

#endif /* defined(__x86_64__) */

