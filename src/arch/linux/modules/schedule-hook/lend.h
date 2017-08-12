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

#pragma once
#include <stdbool.h>

//extern __thread bool _rip_relative;
extern bool _rip_relative;

/* Switch between 32-bit and 64-bit implementations */
#define MODE_X32	1
#define MODE_X64	2

#define has_rip_relative() (_rip_relative == true)

/* length_disasm */
unsigned int length_disasm(void *opcode0, char mode);
