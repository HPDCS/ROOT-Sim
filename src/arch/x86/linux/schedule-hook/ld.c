/*
x86 Length Disassembler.
Copyright (C) 2016 Alessandro Pellegrini
Copyright (C) 2013 Byron Platt

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdbool.h>

#include "lend.h"
#include "ld.h"

//__thread bool _rip_relative;
bool _rip_relative;

/* length_disasm */
unsigned int length_disasm(void *opcode0, char mode) {
    unsigned char *opcode = opcode0;

    unsigned int flag = 0;
    unsigned int ddef = 4, mdef = 4;
    unsigned int msize = 0, dsize = 0;

    unsigned char first_op, op, modrm, mod, rm, rex = 0;
    unsigned char multibyte = 0;

    _rip_relative = false;

prefix:
    op = *opcode++;

    /* prefix */
    if (CHECK_PREFIX(op)) {
        if (CHECK_PREFIX_66(op)) ddef = 2;
        else if (CHECK_PREFIX_67(op)) mdef = 2;
        goto prefix;
    }

    /* Possible REX prefixes, which come after legacy prefixes.
     * Moreover, multiple REX prefixes could be present. Although the behaviour should
     * be undefined, most CPUs consider only the last one. */
    if (mode == MODE_X64 && CHECK_REX(op)) {
	    rex = op;
	    goto prefix;
    }

    first_op = op;

    /* two and three byte opcode */
    if (CHECK_0F(op)) {
	    op = *opcode++;
	    multibyte = 1;

	    /* Three-byte 38 table */
	    if(CHECK_38(op)) {
		op = *opcode++;
		if(CHECK_MODRM38(op)) flag++;
	    } else

	    /* Three-byte 3A table */
	    if(CHECK_3A(op)) {
		op = *opcode++;
		if(CHECK_MODRM3A(op)) flag++;
	    }

	    /* Two-byte table */
	    else {
		if (CHECK_MODRM2(op)) flag++;
		if (CHECK_DATA12(op)) dsize++;
		if (CHECK_DATA662(op)) dsize += ddef;
	    }
    }

    /* one byte opcode */
    else {
        if (CHECK_MODRM(op)) flag++;
        if (CHECK_TEST(op) && !(*opcode & 0x38)) dsize += (op & 1) ? ddef : 1;
        if (CHECK_DATA1(op)) dsize++;
        if (CHECK_DATA2(op)) dsize += 2;
        if (CHECK_DATA66(op)) dsize += ddef;
        if (CHECK_MEM67(op)) msize += mdef;
    }

    /* modrm */
    if (flag) {
        modrm = *opcode++;
        mod = modrm & 0xc0;
        rm  = modrm & 0x07;

        if (mod != 0xc0) {
            if (mod == 0x40) msize++;
            if (mod == 0x80) msize += mdef;
	    if (mdef == 2 && mode == MODE_X32) {
                if ((mod == 0x00) && (rm == 0x06)) msize += 2;
            } else {
                if (rm == 0x04) {
			rm = *opcode++ & 0x07; /* rm is the sib */
		}
                if (rm == 0x05 && mod == 0x00) {
			if(mdef == 2) msize += 2;
			else msize += 4;
		}
            }
	    if(mode == MODE_X64 && first_op != 0xff && mod == 0x00 && rm == 0x05) {
			_rip_relative = true;
	    }
		
	 } else {
		if(op == 0x70) { /* TODO: need another table here! */
			/* Three operands instruction (SSE extension) */
			dsize = 1;
		}
	 }
    }

    /* REX.W causes 66h to be ignored */
    if (CHECK_REXW(rex) && !multibyte) {
        if(CHECK_IMM64(op)) dsize = 8;
        if(CHECK_OFF64(op)) msize = 8;
    }

    opcode += msize + dsize;

    return opcode - (unsigned char *)opcode0;
}
