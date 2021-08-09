/***************************************************************************
 *   m6502.h   --  This file is part of apple1emu.                         *
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

#ifndef M6502_H
#define M6502_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "clock.h"

#define STACK_BASE_ADDR 0x01FF
#define STACK_TOP_ADDR 0x0100
#define MEMSIZE 0x10000

#define BRK_IRQ 0x01
#define BRK_NMI 0x02
#define BRK_RST 0x04

#define STATUS_CF 0x01 // CARRY
#define STATUS_ZF 0x02 // ZERO
#define STATUS_IF 0x04 // IRQ DISABLE
#define STATUS_DF 0x08 // DECIMAL
#define STATUS_BF 0x10 // BREAK
#define STATUS_XF 0x20 // EXTENSION - NOT USED
#define STATUS_VF 0x40 // OVERFLOW
#define STATUS_NF 0x80 // NEGATIVE

#define NMI_VECTOR_ADDR   0xFFFA
#define RESET_VECTOR_ADDR 0xFFFC
#define IRQ_VECTOR_ADDR   0xFFFE

typedef struct {
  // Just profiling
  unsigned long long int tick_count;

  // To signal that the CPU has stopped
  bool* stop;

  // Internal registers - for internal use only
  // IR not only tracks the current opcode, but at what stage of the opcode we
  // are, by using the 2 lower bits, since the opcode is shifted 3 bits to the right
  uint16_t IR;
  // Tracks if an interrupt was requested and by what
  uint8_t break_status;
  // Internal register to hold addresses, for abs/ind/zpg addressing and whatnot
  uint16_t AD;

  // External lines (RW)
  uint16_t* addr_bus;
  uint8_t* data_bus;
  bool *IRQ;
  bool *NMI;
  bool *RES;
  bool *RDY;
  bool *SO;

  // External lines (RDONLY)
  bool RW;
  bool SYNC;

  // Internal registers - for programmer
  int8_t A;
  int8_t X;
  int8_t Y;
  uint16_t PC;
  uint8_t S;
  uint8_t status;

  uint8_t* PCH;
  uint8_t* PCL;

  uint8_t* ADH;
  uint8_t* ADL;

  // CPU external clocks, to drive some other chips
  Clock phi1;
  Clock phi2;
} M6502;

struct M6502_Registers {
  int8_t A;
  int8_t X;
  int8_t Y;
  uint16_t PC;
  uint8_t S;
  uint8_t status;
  uint8_t RW;
  uint8_t SYNC;
  uint16_t addr_bus;
  uint8_t data_bus;
} __attribute__((packed));

typedef struct M6502_Registers M6502_Registers;

void clock_cpu(void* ptr, bool status);
void init_cpu(M6502* cpu);
void cpu_cycle(M6502* cpu);
void cpu_crash(M6502* cpu);
int save_state(M6502* cpu);
int load_state(M6502* cpu);

// Indexing stuff
void get_arg_indirect_index(M6502* cpu);
void get_arg_index_indirect(M6502* cpu);
void get_arg_zero_page(M6502* cpu);
void get_arg_zero_page_index(M6502* cpu, uint8_t index);
void get_arg_absolute(M6502* cpu);
void get_arg_absolute_index(M6502* cpu, uint8_t index);

// Instruction stuff
void do_ORA(M6502* cpu);
void do_AND(M6502* cpu);
void do_EOR(M6502* cpu);
void do_ADC(M6502* cpu);
void do_SBC(M6502* cpu);
void do_BIT(M6502* cpu);
void do_LD_(M6502* cpu, int8_t* reg);
void do_CMP(M6502* cpu, uint8_t A, uint8_t B);
uint8_t do_ROL(M6502* cpu, uint8_t x);
uint8_t do_ASL(M6502* cpu, uint8_t x);
uint8_t do_LSR(M6502* cpu, uint8_t x);
uint8_t do_ROR(M6502* cpu, uint8_t x);
void branch(M6502* cpu, bool condition);
void fetch(M6502* cpu);
void push_stack(M6502* cpu, uint8_t data);
void update_flags_register(M6502* cpu, uint8_t reg);

#endif
