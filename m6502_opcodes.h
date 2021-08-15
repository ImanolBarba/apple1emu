/***************************************************************************
 *   m6502_opcodes.h  --  This file is part of apple1emu.                  *
 *                                                                         *
 *   Copyright (C) 2021 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   apple1emu is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published     *
 *   by the Free Software Foundation, either version 3 of the License,     *
 *   or (at your option) any later version.                                *
 *                                                                         *
 *   apple1emu is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/

#ifndef M6502_OPCODES_H
#define M6502_OPCODES_H

#include "m6502.h"

#define IR_STATUS_MASK 0x7

enum addressing_modes {
  ADDR_IMPLICIT = 0,
  ADDR_ACCUMULATOR = 1,
  ADDR_IMMEDIATE = 2,
  ADDR_ZPG = 3,
  ADDR_ZPG_X = 4,
  ADDR_ZPG_Y = 5,
  ADDR_RELATIVE = 6,
  ADDR_ABSOLUTE = 7,
  ADDR_ABSOLUTE_X = 8,
  ADDR_ABSOLUTE_Y = 9,
  ADDR_INDIRECT = 10,
  ADDR_INDEX_IND = 11,
  ADDR_IND_INDEX = 12
};

typedef void (*opcode_func)(M6502*);

typedef struct {
  const char* name;
  opcode_func op;
  bool write;
  int addr_mode;
} Opcode;

Opcode* opcodes[0x100];

void run_opcode(M6502* cpu);
void print_disassembly(M6502* cpu, uint16_t addr, unsigned int num_instructions);

#endif
