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

typedef void (*opcode_func)(M6502*);

bool is_write_opcode(uint8_t op) {
  // Not ideal, but hey, it works
  return (op == 0x0A || op == 0x06 || op == 0x16 || op == 0x0E || op == 0x1E ||
          op == 0x85 || op == 0x95 || op == 0x8D || op == 0x9D || op == 0x99 ||
          op == 0x81 || op == 0x91 || op == 0xC6 || op == 0xD6 || op == 0xCE ||
          op == 0xDE || op == 0xE6 || op == 0xF6 || op == 0xEE || op == 0xFE ||
          op == 0x4A || op == 0x46 || op == 0x56 || op == 0x4E || op == 0x5E ||
          op == 0x6A || op == 0x66 || op == 0x76 || op == 0x6E || op == 0x7E ||
          op == 0x2A || op == 0x26 || op == 0x36 || op == 0x2E || op == 0x3E);
}

// UNDEF
void op_XX(M6502* cpu) {
  fprintf(stderr, "Unknown opcode: 0x%02X\n", cpu->IR >> 3);
  cpu_crash(cpu);
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
      push_stack(cpu, cpu->status | STATUS_BF | STATUS_XF);
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
      push_stack(cpu, *cpu->PCH);
    break;
    case 3:
      push_stack(cpu, *cpu->PCL);
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
  do_BIT(cpu);
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

// BIT abs
void op_2C(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_BIT(cpu);
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

// AND zpg, X
void op_35(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL zpg, X
void op_36(M6502* cpu) {
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
      *cpu->data_bus = do_ROL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// SEC
void op_38(M6502* cpu) {
 switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status |= STATUS_CF;
      fetch(cpu);
    break;
  }
}

// AND abs, Y
void op_39(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// AND abs, X
void op_3D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL abs, X
void op_3E(M6502* cpu) {
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
      *cpu->data_bus = do_ROL(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 6:
      fetch(cpu);
    break;
  }
}

// RTI
void op_40(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S++;
    break;
    case 2:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S++;
    break;
    case 3:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S++;
      cpu->status = (*cpu->data_bus | STATUS_BF) & ~STATUS_XF;
    break;
    case 4:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S;
      *cpu->PCL = *cpu->data_bus;
    break;
    case 5:
      *cpu->PCH = *cpu->data_bus;
      fetch(cpu);
    break;
  }
}

// EOR X, ind
void op_41(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// EOR zpg
void op_45(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR zpg
void op_46(M6502* cpu) {
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
      *cpu->data_bus = do_LSR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// PHA
void op_48(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      push_stack(cpu, cpu->A);
    break;
    case 2:
      fetch(cpu);
    break;
  }
}

// EOR #
void op_49(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_EOR(cpu);
      fetch(cpu);
    break;
  }
}

// LSR A
void op_4A(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->A = do_LSR(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// JMP abs
void op_4C(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      *cpu->addr_bus = cpu->PC++;
      cpu->AD = *cpu->data_bus;
    break;
    case 2:
      cpu->PC = *cpu->data_bus << 8 | cpu->AD;
      fetch(cpu);
    break;
  }
}

// EOR Abs
void op_4D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR Abs
void op_4E(M6502* cpu) {
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
      *cpu->data_bus = do_LSR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// BVC
void op_50(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_VF));
}

// EOR ind, Y
void op_51(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// EOR zpg, X
void op_55(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR zpg, X
void op_56(M6502* cpu) {
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
      *cpu->data_bus = do_LSR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// CLI
void op_58(M6502* cpu) {
 switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status &= ~STATUS_IF;
      fetch(cpu);
    break;
  }
}

// EOR abs, Y
void op_59(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// EOR abs, X
void op_5D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR abs, X
void op_5E(M6502* cpu) {
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
      *cpu->data_bus = do_LSR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 6:
      fetch(cpu);
    break;
  }
}

// RTS
void op_60(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S++;
    break;
    case 2:
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S++;
    break;
    case 3:
      cpu->AD = *cpu->data_bus;
      *cpu->addr_bus = STACK_TOP_ADDR | cpu->S;
    break;
    case 4:
      cpu->PC = cpu->AD | ((*cpu->data_bus) << 8);
      *cpu->addr_bus = cpu->PC++;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// ADC X, ind
void op_61(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ADC zpg
void op_65(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR zpg
void op_66(M6502* cpu) {
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
      *cpu->data_bus = do_ROR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// PLA
void op_68(M6502* cpu) {
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
      cpu->A = *cpu->data_bus;
      update_flags_register(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// ADC #
void op_69(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_ADC(cpu);
      fetch(cpu);
    break;
  }
}

// ROR A
void op_6A(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->A = do_ROR(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// JMP ind
void op_6C(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
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

// ADC Abs
void op_6D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR Abs
void op_6E(M6502* cpu) {
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
      *cpu->data_bus = do_ROR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// BVS
void op_70(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_VF);
}

// ADC ind, Y
void op_71(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ADC zpg, X
void op_75(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR zpg, X
void op_76(M6502* cpu) {
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
      *cpu->data_bus = do_ROR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// SEI
void op_78(M6502* cpu) {
 switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status |= STATUS_IF;
      fetch(cpu);
    break;
  }
}

// ADC abs, Y
void op_79(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ADC abs, X
void op_7D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR abs, X
void op_7E(M6502* cpu) {
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
      *cpu->data_bus = do_ROR(cpu, cpu->AD);
      cpu->RW = false;
    break;
    case 6:
      fetch(cpu);
    break;
  }
}

// STA X, ind
void op_81(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_index_indirect(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 4:
      // We just want to use the last cycle to also set the accumulator
      get_arg_index_indirect(cpu);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// STY zpg
void op_84(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 1) {
    get_arg_zero_page(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 1:
      get_arg_zero_page(cpu);
      *cpu->data_bus = cpu->Y;
      cpu->RW = false;
    break;
    case 2:
      fetch(cpu);
    break;
  }
}

// STA zpg
void op_85(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 1) {
    get_arg_zero_page(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 1:
      get_arg_zero_page(cpu);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 2:
      fetch(cpu);
    break;
  }
}

// STX zpg
void op_86(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 1) {
    get_arg_zero_page(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 1:
      get_arg_zero_page(cpu);
      *cpu->data_bus = cpu->X;
      cpu->RW = false;
    break;
    case 2:
      fetch(cpu);
    break;
  }
}

// DEY
void op_88(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->Y--;
      update_flags_register(cpu, cpu->Y);
      fetch(cpu);
    break;
  }
}

// TXA
void op_8A(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->A = cpu->X;
      update_flags_register(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// STY abs
void op_8C(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_absolute(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      get_arg_absolute(cpu);
      *cpu->data_bus = cpu->Y;
      cpu->RW = false;
    break;
    case 3:
      fetch(cpu);
    break;
  }
}

// STA abs
void op_8D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_absolute(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      get_arg_absolute(cpu);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 3:
      fetch(cpu);
    break;
  }
}

// STX abs
void op_8E(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_absolute(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      get_arg_absolute(cpu);
      *cpu->data_bus = cpu->X;
      cpu->RW = false;
    break;
    case 3:
      fetch(cpu);
    break;
  }
}

// BCC
void op_90(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_CF));
}

// STA ind, Y
void op_91(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_indirect_index(cpu);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 4:
      get_arg_indirect_index(cpu);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// STY zpg, X
void op_94(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      get_arg_zero_page_index(cpu, cpu->X);
      *cpu->data_bus = cpu->Y;
      cpu->RW = false;
    break;
    case 3:
      fetch(cpu);
    break;
  }
}

// STA zpg, X
void op_95(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      get_arg_zero_page_index(cpu, cpu->X);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 3:
      fetch(cpu);
    break;
  }
}

// STX zpg, Y
void op_96(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page_index(cpu, cpu->Y);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 2:
      get_arg_zero_page_index(cpu, cpu->Y);
      *cpu->data_bus = cpu->X;
      cpu->RW = false;
    break;
    case 3:
      fetch(cpu);
    break;
  }
}

// TYA
void op_98(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->A = cpu->Y;
      update_flags_register(cpu, cpu->A);
      fetch(cpu);
    break;
  }
}

// STA abs, Y
void op_99(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 3:
      get_arg_absolute_index(cpu, cpu->Y);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// TXS
void op_9A(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->S = cpu->X;
      fetch(cpu);
    break;
  }
}

// STA abs, X
void op_9D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  switch(cpu->IR & IR_STATUS_MASK) {
    case 3:
      get_arg_absolute_index(cpu, cpu->X);
      *cpu->data_bus = cpu->A;
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// LDY imm
void op_A0(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_LD_(cpu, &cpu->Y);
      fetch(cpu);
    break;
  }
}

// LDA X, ind
void op_A1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX imm
void op_A2(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_LD_(cpu, &cpu->X);
      fetch(cpu);
    break;
  }
}

// LDY zpg
void op_A4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA zpg
void op_A5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX zpg
void op_A6(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// TAY
void op_A8(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->Y = cpu->A;
      update_flags_register(cpu, cpu->Y);
      fetch(cpu);
    break;
  }
}

// LDA imm
void op_A9(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_LD_(cpu, &cpu->A);
      fetch(cpu);
    break;
  }
}

// TAX
void op_AA(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->X = cpu->A;
      update_flags_register(cpu, cpu->X);
      fetch(cpu);
    break;
  }
}

// LDY abs
void op_AC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA abs
void op_AD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX abs
void op_AE(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// BCS
void op_B0(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_CF);
}

// LDA ind, Y
void op_B1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDY zpg, X
void op_B4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA zpg, X
void op_B5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX zpg, Y
void op_B6(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->Y);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// CLV
void op_B8(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status &= ~STATUS_VF;
      fetch(cpu);
    break;
  }
}

// LDA abs, Y
void op_B9(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// TSX
void op_BA(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->X = cpu->S;
      update_flags_register(cpu, cpu->X);
      fetch(cpu);
    break;
  }
}

// LDY abs, X
void op_BC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA abs, X
void op_BD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX abs, Y
void op_BE(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// CPY imm
void op_C0(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_CMP(cpu, cpu->Y, *cpu->data_bus);
      fetch(cpu);
    break;
  }
}

// CMP X, ind
void op_C1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// CPY zpg
void op_C4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_CMP(cpu, cpu->Y, *cpu->data_bus);
  fetch(cpu);
}

// CMP zpg
void op_C5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC zpg
void op_C6(M6502* cpu) {
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
      cpu->AD--;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// INY
void op_C8(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->Y++;
      update_flags_register(cpu, cpu->Y);
      fetch(cpu);
    break;
  }
}

// CMP imm
void op_C9(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_CMP(cpu, cpu->A, *cpu->data_bus);
      fetch(cpu);
    break;
  }
}

// DEX
void op_CA(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->X--;
      update_flags_register(cpu, cpu->X);
      fetch(cpu);
    break;
  }
}

// CPY abs
void op_CC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_CMP(cpu, cpu->Y, *cpu->data_bus);
  fetch(cpu);
}

// CMP abs
void op_CD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC abs
void op_CE(M6502* cpu) {
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
      cpu->AD--;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// BNE
void op_D0(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_ZF));
}

// CMP ind, Y
void op_D1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// CMP zpg, X
void op_D5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC zpg, X
void op_D6(M6502* cpu) {
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
      cpu->AD--;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// CLD
void op_D8(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status &= ~STATUS_DF;
      fetch(cpu);
    break;
  }
}

// CMP abs, Y
void op_D9(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// CMP abs, X
void op_DD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC abs, X
void op_DE(M6502* cpu) {
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
      cpu->AD--;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 6:
      fetch(cpu);
    break;
  }
}

// CPX imm
void op_E0(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_CMP(cpu, cpu->X, *cpu->data_bus);
      fetch(cpu);
    break;
  }
}

// SBC X, ind
void op_E1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// CPX zpg
void op_E4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_CMP(cpu, cpu->X, *cpu->data_bus);
  fetch(cpu);
}

// SBC zpg
void op_E5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC zpg
void op_E6(M6502* cpu) {
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
      cpu->AD++;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 4:
      fetch(cpu);
    break;
  }
}

// INX
void op_E8(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->X++;
      update_flags_register(cpu, cpu->X);
      fetch(cpu);
    break;
  }
}

// SBC imm
void op_E9(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      do_SBC(cpu);
      fetch(cpu);
    break;
  }
}

// NOP
void op_EA(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      fetch(cpu);
    break;
  }
}

// CPX abs
void op_EC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_CMP(cpu, cpu->X, *cpu->data_bus);
  fetch(cpu);
}

// SBC abs
void op_ED(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC abs
void op_EE(M6502* cpu) {
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
      cpu->AD++;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// BEQ
void op_F0(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_ZF);
}

// SBC ind, Y
void op_F1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// SBC zpg, X
void op_F5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC zpg, X
void op_F6(M6502* cpu) {
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
      cpu->AD++;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 5:
      fetch(cpu);
    break;
  }
}

// SED
void op_F8(M6502* cpu) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC;
    break;
    case 1:
      cpu->status |= STATUS_DF;
      fetch(cpu);
    break;
  }
}

// SBC abs, Y
void op_F9(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// SBC abs, X
void op_FD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC abs, X
void op_FE(M6502* cpu) {
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
      cpu->AD++;
      update_flags_register(cpu, cpu->AD);
      *cpu->data_bus = cpu->AD;
      cpu->RW = false;
    break;
    case 6:
      fetch(cpu);
    break;
  }
}


opcode_func opcode[0x100] = {
  &op_00,&op_01,&op_XX,&op_XX,&op_XX,&op_05,&op_06,&op_XX,&op_08,&op_09,&op_0A,&op_XX,&op_XX,&op_0D,&op_0E,&op_XX,
  &op_10,&op_11,&op_XX,&op_XX,&op_XX,&op_15,&op_16,&op_XX,&op_18,&op_19,&op_XX,&op_XX,&op_XX,&op_1D,&op_1E,&op_XX,
  &op_20,&op_21,&op_XX,&op_XX,&op_24,&op_25,&op_26,&op_XX,&op_28,&op_29,&op_2A,&op_XX,&op_2C,&op_2D,&op_2E,&op_XX,
  &op_30,&op_31,&op_XX,&op_XX,&op_XX,&op_35,&op_36,&op_XX,&op_38,&op_39,&op_XX,&op_XX,&op_XX,&op_3D,&op_3E,&op_XX,
  &op_40,&op_41,&op_XX,&op_XX,&op_XX,&op_45,&op_46,&op_XX,&op_48,&op_49,&op_4A,&op_XX,&op_4C,&op_4D,&op_4E,&op_XX,
  &op_50,&op_51,&op_XX,&op_XX,&op_XX,&op_55,&op_56,&op_XX,&op_58,&op_59,&op_XX,&op_XX,&op_XX,&op_5D,&op_5E,&op_XX,
  &op_60,&op_61,&op_XX,&op_XX,&op_XX,&op_65,&op_66,&op_XX,&op_68,&op_69,&op_6A,&op_XX,&op_6C,&op_6D,&op_6E,&op_XX,
  &op_70,&op_71,&op_XX,&op_XX,&op_XX,&op_75,&op_76,&op_XX,&op_78,&op_79,&op_XX,&op_XX,&op_XX,&op_7D,&op_7E,&op_XX,
  &op_XX,&op_81,&op_XX,&op_XX,&op_84,&op_85,&op_86,&op_XX,&op_88,&op_XX,&op_8A,&op_XX,&op_8C,&op_8D,&op_8E,&op_XX,
  &op_90,&op_91,&op_XX,&op_XX,&op_94,&op_95,&op_96,&op_XX,&op_98,&op_99,&op_9A,&op_XX,&op_XX,&op_9D,&op_XX,&op_XX,
  &op_A0,&op_A1,&op_A2,&op_XX,&op_A4,&op_A5,&op_A6,&op_XX,&op_A8,&op_A9,&op_AA,&op_XX,&op_AC,&op_AD,&op_AE,&op_XX,
  &op_B0,&op_B1,&op_XX,&op_XX,&op_B4,&op_B5,&op_B6,&op_XX,&op_B8,&op_B9,&op_BA,&op_XX,&op_BC,&op_BD,&op_BE,&op_XX,
  &op_C0,&op_C1,&op_XX,&op_XX,&op_C4,&op_C5,&op_C6,&op_XX,&op_C8,&op_C9,&op_CA,&op_XX,&op_CC,&op_CD,&op_CE,&op_XX,
  &op_D0,&op_D1,&op_XX,&op_XX,&op_XX,&op_D5,&op_D6,&op_XX,&op_D8,&op_D9,&op_XX,&op_XX,&op_XX,&op_DD,&op_DE,&op_XX,
  &op_E0,&op_E1,&op_XX,&op_XX,&op_E4,&op_E5,&op_E6,&op_XX,&op_E8,&op_E9,&op_EA,&op_XX,&op_EC,&op_ED,&op_EE,&op_XX,
  &op_F0,&op_F1,&op_XX,&op_XX,&op_XX,&op_F5,&op_F6,&op_XX,&op_F8,&op_F9,&op_XX,&op_XX,&op_XX,&op_FD,&op_FE,&op_XX
};

void run_opcode(M6502* cpu)  {
  opcode[cpu->IR >> 3](cpu);
}
