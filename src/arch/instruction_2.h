/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of the Hijacker static binary instrumentation tool.
*
* Hijacker is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* Hijacker is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* hijacker; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file instruction.h
* @brief Abstraction of an assembly instruction is somewhat machine-independent way
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/

#pragma once
#ifndef _INSTRUCTION_X86_H
#define _INSTRUCTION_X86_H

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
	long long disp;	// Il valore dello spiazzamento
	unsigned long immed_offset;	// [SE] Lo spiazzamento dei dati immediati dall'inizio del testo, o 0x00
	int immed_size;		// [SE] La dimensione dei dati immediati, o 0x00
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

#endif /* _INSTRUCTION_X86_H */

