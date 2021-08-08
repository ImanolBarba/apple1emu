/***************************************************************************
 *   m6502_opcodes.c  --  This file is part of apple1emu.                  *
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

#include "m6502.h"
#include "m6502_opcodes.h"

#include <stdio.h>

// TODO: Opcodes

typedef void (*opcode_func)(M6502*);

// UNDEF
void op_XX(M6502* cpu) {
  //fprintf(stderr, "Unknown opcode: 0x%02X\n", cpu->IR >> 3);
  fetch(cpu);
}

// BRK
void op_00(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      // The 6502 does this on the first cycle of interrupt for implicit addressing,
      // pretty useless but meh
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      if(!cpu->break_status) {
        // manual BRK instruction, print break mark
        fprintf(stderr, "Triggered software BRK, reason: 0x%02X\n", *cpu->data_bus);
        cpu->PC++;
      }
      push_stack(cpu, *cpu->PCH);
      if(cpu->break_status & BRK_RST) {
        // Write only enabled in non-RST interrupt, so set it to read again
        cpu->RW = true;
      }
    break;
    case 2:
      push_stack(cpu, *cpu->PCL);
      if(cpu->break_status & BRK_RST) {
        // Write only enabled in non-RST interrupt, so set it to read again
        cpu->RW = true;
      }
    break;
    case 3:
      push_stack(cpu, cpu->status | STATUS_BF);
      if(cpu->break_status & BRK_RST) {
        // Write only enabled in non-RST interrupt, so set it to read again
        cpu->RW = true;
        cpu->AD = RESET_VECTOR_ADDR;
      } else if(cpu->break_status & BRK_NMI) {
        cpu->AD = NMI_VECTOR_ADDR;
      } else {
        cpu->AD = IRQ_VECTOR_ADDR;
      }
      cpu->break_status = 0;
      *cpu->RES = true;
    break;
    case 4:
      *cpu->addr_bus = cpu->AD++;
      // set Interrupt Disable flag automatically during interrupt, it will be restored
      // by either the startup routine or the interrupt routine when restoring status register
      cpu->status |= STATUS_IF;
    break;
    case 5:
      // We can set the addr since the memory won't be refreshed until CPU finishes in this impl.
      *cpu->addr_bus = cpu->AD;
      cpu->AD = *cpu->data_bus;
    break;
    case 6:
      cpu->PC = *cpu->data_bus << 8 | cpu->AD;
      fetch(cpu);
    break;
  }
}

// ORA X,ind
void op_01(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ORA zpg
void op_05(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL zpg
void op_06(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      cpu->AD = *cpu->data_bus;
      // For some reason, the CPU sets RW to 0 here
      cpu->RW = false;
    break;
    case 3:
      *cpu->data_bus = do_ASL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// PHP
void op_08(M6502* cpu) {
   switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      push_stack(cpu, cpu->status | STATUS_BF | STATUS_XF);
    break;
    case 2:
      fetch(cpu);
    break;
  }
}

// ORA #
void op_09(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_ORA(cpu);
      fetch(cpu);
    break;
  }
}

// ASL A
void op_0A(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->A = do_ASL(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// ORA Abs
void op_0D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL Abs
void op_0E(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 3:
      cpu->AD = *cpu->data_bus;
      cpu->RW = false;
    break;
    case 4:
      *cpu->data_bus = do_ASL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// BPL rel
void op_10(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_NF));
}

// ORA ind, Y
void op_11(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ORA zpg, X
void op_15(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL zpg, X
void op_16(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 3:
      cpu->AD = *cpu->data_bus;
      cpu->RW = false;
    break;
    case 4:
      *cpu->data_bus = do_ASL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// CLC
void op_18(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status &= ~STATUS_CF;
      fetch(cpu);
    break;
  }
}

// ORA abs, Y
void op_19(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ORA abs, X
void op_1D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL abs, X
void op_1E(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 4:
      cpu->AD = *cpu->data_bus;
      cpu->RW = false;
    break;
    case 5:
      *cpu->data_bus = do_ASL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 6:
      fetch(cpu);
    break;
  }
}

// JSR abs
void op_20(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      cpu->AD = *cpu->data_bus;
      *cpu->addr_bus = STACK_BASE_ADDR | cpu->S;
    break;
    case 2:
      push_stack(cpu, cpu->PC >> 8);
    break;
    case 3:
      push_stack(cpu, cpu->PC);
    break;
    case 4:
      *cpu->addr_bus = cpu->PC;
    break;
    case 5:
      cpu->PC = *cpu->data_bus << 8 | cpu->AD;
      fetch(cpu);
    break;
  }
}

// AND X, ind
void op_21(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// BIT zpg
void op_24(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  if(cpu->A & *cpu->data_bus) {
    cpu->status |= STATUS_ZF;
  }
  cpu->status = (cpu->status & 0x3F) | (*cpu->data_bus & 0xC0);
  fetch(cpu);
}

// AND zpg
void op_25(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL zpg
void op_26(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      cpu->AD = *cpu->data_bus;
      cpu->RW = false;
    break;
    case 3:
      *cpu->data_bus = do_ROL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// PLP
void op_28(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S++;
    break;
    case 2:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S;
    break;
    case 3:
      cpu->status = (*cpu->data_bus | STATUS_BF) & ~STATUS_BF;
      fetch(cpu);
    break;
  }
}

// AND #
void op_29(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_AND(cpu);
      fetch(cpu);
    break;
  }
}

// ROL A
void op_2A(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->A = do_ROL(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// BIT zpg
void op_2C(M6502* cpu) {
if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  if(cpu->A & *cpu->data_bus) {
    cpu->status |= STATUS_ZF;
  }
  cpu->status = (cpu->status & 0x3F) | (*cpu->data_bus & 0xC0);
  fetch(cpu);
}

// AND Abs
void op_2D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL Abs
void op_2E(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 3:
      cpu->AD = *cpu->data_bus;
      cpu->RW = false;
    break;
    case 4:
      *cpu->data_bus = do_ROL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// BMI
void op_30(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_NF);
}

// AND ind, Y
void op_31(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}



// JMP ind
void op_6C(M6502* cpu) {
  switch(cpu->IR & 0x3) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      cpu->AD = *cpu->data_bus;
      *cpu->addr_bus = cpu->PC++;
    break;
    case 2:
      cpu->AD |= *cpu->data_bus << 8;
      *cpu->addr_bus = cpu->AD;
    break;
    case 3:
      *cpu->addr_bus = (cpu->AD & 0xFF00) | ((cpu->AD + 1) & 0x00FF);
      cpu->AD = *cpu->data_bus;
    break;
    case 4:
      cpu->PC = *cpu->data_bus << 8 | cpu->AD;
      fetch(cpu);
    break;
  }
}

opcode_func opcode[0x100] = {
  &op_00,&op_01,&op_XX,&op_XX,&op_XX,&op_05,&op_06,&op_XX,&op_08,&op_09,&op_0A,&op_XX,&op_XX,&op_0D,&op_0E,&op_XX,
  &op_10,&op_11,&op_XX,&op_XX,&op_XX,&op_15,&op_16,&op_XX,&op_18,&op_19,&op_XX,&op_XX,&op_XX,&op_1D,&op_1E,&op_XX,
  &op_20,&op_21,&op_XX,&op_XX,&op_24,&op_25,&op_26,&op_XX,&op_28,&op_29,&op_2A,&op_XX,&op_2C,&op_2D,&op_2E,&op_XX,
  &op_30,&op_31,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_6C,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,
  &op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX,&op_XX
};

void run_opcode(M6502* cpu)  {
  opcode[cpu->IR >> 3](cpu);
}
