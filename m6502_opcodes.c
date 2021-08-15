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

// UNDEF
void do_XX(M6502* cpu) {
  fprintf(stderr, "Unknown opcode: 0x%02X\n", cpu->IR >> 3);
  cpu_crash(cpu);
  fetch(cpu);
}

// BRK
void do_00(M6502* cpu) {
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
void do_01(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ORA zpg
void do_05(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL zpg
void do_06(M6502* cpu) {
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
void do_08(M6502* cpu) {
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
void do_09(M6502* cpu) {
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
void do_0A(M6502* cpu) {
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
void do_0D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL Abs
void do_0E(M6502* cpu) {
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
void do_10(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_NF));
}

// ORA ind, Y
void do_11(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ORA zpg, X
void do_15(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL zpg, X
void do_16(M6502* cpu) {
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
void do_18(M6502* cpu) {
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
void do_19(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ORA abs, X
void do_1D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_ORA(cpu);
  fetch(cpu);
}

// ASL abs, X
void do_1E(M6502* cpu) {
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
void do_20(M6502* cpu) {
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
void do_21(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// BIT zpg
void do_24(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_BIT(cpu);
  fetch(cpu);
}

// AND zpg
void do_25(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL zpg
void do_26(M6502* cpu) {
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
void do_28(M6502* cpu) {
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
void do_29(M6502* cpu) {
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
void do_2A(M6502* cpu) {
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
void do_2C(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_BIT(cpu);
  fetch(cpu);
}

// AND Abs
void do_2D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL Abs
void do_2E(M6502* cpu) {
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
void do_30(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_NF);
}

// AND ind, Y
void do_31(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// AND zpg, X
void do_35(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL zpg, X
void do_36(M6502* cpu) {
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
void do_38(M6502* cpu) {
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
void do_39(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// AND abs, X
void do_3D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_AND(cpu);
  fetch(cpu);
}

// ROL abs, X
void do_3E(M6502* cpu) {
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
void do_40(M6502* cpu) {
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
void do_41(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// EOR zpg
void do_45(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR zpg
void do_46(M6502* cpu) {
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
void do_48(M6502* cpu) {
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
void do_49(M6502* cpu) {
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
void do_4A(M6502* cpu) {
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
void do_4C(M6502* cpu) {
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
void do_4D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR Abs
void do_4E(M6502* cpu) {
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
void do_50(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_VF));
}

// EOR ind, Y
void do_51(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// EOR zpg, X
void do_55(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR zpg, X
void do_56(M6502* cpu) {
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
void do_58(M6502* cpu) {
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
void do_59(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// EOR abs, X
void do_5D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_EOR(cpu);
  fetch(cpu);
}

// LSR abs, X
void do_5E(M6502* cpu) {
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
void do_60(M6502* cpu) {
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
void do_61(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ADC zpg
void do_65(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR zpg
void do_66(M6502* cpu) {
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
void do_68(M6502* cpu) {
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
void do_69(M6502* cpu) {
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
void do_6A(M6502* cpu) {
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
void do_6C(M6502* cpu) {
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
void do_6D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR Abs
void do_6E(M6502* cpu) {
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
void do_70(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_VF);
}

// ADC ind, Y
void do_71(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ADC zpg, X
void do_75(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR zpg, X
void do_76(M6502* cpu) {
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
void do_78(M6502* cpu) {
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
void do_79(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ADC abs, X
void do_7D(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_ADC(cpu);
  fetch(cpu);
}

// ROR abs, X
void do_7E(M6502* cpu) {
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
void do_81(M6502* cpu) {
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
void do_84(M6502* cpu) {
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
void do_85(M6502* cpu) {
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
void do_86(M6502* cpu) {
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
void do_88(M6502* cpu) {
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
void do_8A(M6502* cpu) {
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
void do_8C(M6502* cpu) {
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
void do_8D(M6502* cpu) {
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
void do_8E(M6502* cpu) {
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
void do_90(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_CF));
}

// STA ind, Y
void do_91(M6502* cpu) {
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
void do_94(M6502* cpu) {
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
void do_95(M6502* cpu) {
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
void do_96(M6502* cpu) {
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
void do_98(M6502* cpu) {
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
void do_99(M6502* cpu) {
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
void do_9A(M6502* cpu) {
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
void do_9D(M6502* cpu) {
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
void do_A0(M6502* cpu) {
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
void do_A1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX imm
void do_A2(M6502* cpu) {
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
void do_A4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA zpg
void do_A5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX zpg
void do_A6(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// TAY
void do_A8(M6502* cpu) {
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
void do_A9(M6502* cpu) {
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
void do_AA(M6502* cpu) {
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
void do_AC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA abs
void do_AD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX abs
void do_AE(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// BCS
void do_B0(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_CF);
}

// LDA ind, Y
void do_B1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDY zpg, X
void do_B4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA zpg, X
void do_B5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX zpg, Y
void do_B6(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->Y);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// CLV
void do_B8(M6502* cpu) {
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
void do_B9(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// TSX
void do_BA(M6502* cpu) {
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
void do_BC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->Y);
  fetch(cpu);
}

// LDA abs, X
void do_BD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_LD_(cpu, &cpu->A);
  fetch(cpu);
}

// LDX abs, Y
void do_BE(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_LD_(cpu, &cpu->X);
  fetch(cpu);
}

// CPY imm
void do_C0(M6502* cpu) {
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
void do_C1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// CPY zpg
void do_C4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_CMP(cpu, cpu->Y, *cpu->data_bus);
  fetch(cpu);
}

// CMP zpg
void do_C5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC zpg
void do_C6(M6502* cpu) {
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
void do_C8(M6502* cpu) {
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
void do_C9(M6502* cpu) {
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
void do_CA(M6502* cpu) {
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
void do_CC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_CMP(cpu, cpu->Y, *cpu->data_bus);
  fetch(cpu);
}

// CMP abs
void do_CD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC abs
void do_CE(M6502* cpu) {
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
void do_D0(M6502* cpu) {
  branch(cpu, !(cpu->status & STATUS_ZF));
}

// CMP ind, Y
void do_D1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// CMP zpg, X
void do_D5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC zpg, X
void do_D6(M6502* cpu) {
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
void do_D8(M6502* cpu) {
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
void do_D9(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// CMP abs, X
void do_DD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_CMP(cpu, cpu->A, *cpu->data_bus);
  fetch(cpu);
}

// DEC abs, X
void do_DE(M6502* cpu) {
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
void do_E0(M6502* cpu) {
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
void do_E1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_index_indirect(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// CPX zpg
void do_E4(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_CMP(cpu, cpu->X, *cpu->data_bus);
  fetch(cpu);
}

// SBC zpg
void do_E5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC zpg
void do_E6(M6502* cpu) {
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
void do_E8(M6502* cpu) {
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
void do_E9(M6502* cpu) {
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
void do_EA(M6502* cpu) {
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
void do_EC(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_CMP(cpu, cpu->X, *cpu->data_bus);
  fetch(cpu);
}

// SBC abs
void do_ED(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_absolute(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC abs
void do_EE(M6502* cpu) {
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
void do_F0(M6502* cpu) {
  branch(cpu, cpu->status & STATUS_ZF);
}

// SBC ind, Y
void do_F1(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 5) {
    get_arg_indirect_index(cpu);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// SBC zpg, X
void do_F5(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 3) {
    get_arg_zero_page_index(cpu, cpu->X);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC zpg, X
void do_F6(M6502* cpu) {
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
void do_F8(M6502* cpu) {
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
void do_F9(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->Y);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// SBC abs, X
void do_FD(M6502* cpu) {
  if((cpu->IR & IR_STATUS_MASK) < 4) {
    get_arg_absolute_index(cpu, cpu->X);
    return;
  }
  do_SBC(cpu);
  fetch(cpu);
}

// INC abs, X
void do_FE(M6502* cpu) {
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

Opcode op_XX = {
  .name = "UNK", .op = &do_XX, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_00 = {
  // Not really immediate addressing, but has the break mark, so...
  .name = "BRK", .op = &do_00, .write = false, .addr_mode = ADDR_IMMEDIATE 
};
Opcode op_01 = {
  .name = "ORA", .op = &do_01, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_05 = {
  .name = "ORA", .op = &do_05, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_06 = {
  .name = "ASL", .op = &do_06, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_08 = {
  .name = "PHP", .op = &do_08, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_09 = {
  .name = "ORA", .op = &do_09, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_0A = {
  .name = "ASL", .op = &do_0A, .write = true, .addr_mode = ADDR_ACCUMULATOR
};
Opcode op_0D = {
  .name = "ORA", .op = &do_0D, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_0E = {
  .name = "ASL", .op = &do_0E, .write = true, .addr_mode = ADDR_ABSOLUTE 
};
Opcode op_10 = {
  .name = "BPL", .op = &do_10, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_11 = {
  .name = "ORA", .op = &do_11, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_15 = {
  .name = "ORA", .op = &do_15, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_16 = {
  .name = "ASL", .op = &do_16, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_18 = {
  .name = "CLC", .op = &do_18, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_19 = {
  .name = "ORA", .op = &do_19, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_1D = {
  .name = "ORA", .op = &do_1D, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_1E = {
  .name = "ASL", .op = &do_1E, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_20 = {
  .name = "JSR", .op = &do_20, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_21 = {
  .name = "AND", .op = &do_21, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_24 = {
  .name = "BIT", .op = &do_24, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_25 = {
  .name = "AND", .op = &do_25, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_26 = {
  .name = "ROL", .op = &do_26, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_28 = {
  .name = "PLP", .op = &do_28, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_29 = {
  .name = "AND", .op = &do_29, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_2A = {
  .name = "ROL", .op = &do_2A, .write = true, .addr_mode = ADDR_ACCUMULATOR
};
Opcode op_2C = {
  .name = "BIT", .op = &do_2C, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_2D = {
  .name = "AND", .op = &do_2D, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_2E = {
  .name = "ROL", .op = &do_2E, .write = true, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_30 = {
  .name = "BMI", .op = &do_30, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_31 = {
  .name = "AND", .op = &do_31, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_35 = {
  .name = "AND", .op = &do_35, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_36 = {
  .name = "ROL", .op = &do_36, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_38 = {
  .name = "SEC", .op = &do_38, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_39 = {
  .name = "AND", .op = &do_39, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_3D = {
  .name = "AND", .op = &do_3D, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_3E = {
  .name = "ROL", .op = &do_3E, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_40 = {
  .name = "RTI", .op = &do_40, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_41 = {
  .name = "EOR", .op = &do_41, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_45 = {
  .name = "EOR", .op = &do_45, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_46 = {
  .name = "LSR", .op = &do_46, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_48 = {
  .name = "PHA", .op = &do_48, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_49 = {
  .name = "EOR", .op = &do_49, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_4A = {
  .name = "LSR", .op = &do_4A, .write = true, .addr_mode = ADDR_ACCUMULATOR
};
Opcode op_4C = {
  .name = "JMP", .op = &do_4C, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_4D = {
  .name = "EOR", .op = &do_4D, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_4E = {
  .name = "LSR", .op = &do_4E, .write = true, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_50 = {
  .name = "BVC", .op = &do_50, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_51 = {
  .name = "EOR", .op = &do_51, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_55 = {
  .name = "EOR", .op = &do_55, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_56 = {
  .name = "LSR", .op = &do_56, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_58 = {
  .name = "CLI", .op = &do_58, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_59 = {
  .name = "EOR", .op = &do_59, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_5D = {
  .name = "EOR", .op = &do_5D, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_5E = {
  .name = "LSR", .op = &do_5E, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_60 = {
  .name = "RTS", .op = &do_60, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_61 = {
  .name = "ADC", .op = &do_61, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_65 = {
  .name = "ADC", .op = &do_65, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_66 = {
  .name = "ROR", .op = &do_66, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_68 = {
  .name = "PLA", .op = &do_68, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_69 = {
  .name = "ADC", .op = &do_69, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_6A = {
  .name = "ROR", .op = &do_6A, .write = true, .addr_mode = ADDR_ACCUMULATOR
};
Opcode op_6C = {
  .name = "JMP", .op = &do_6C, .write = false, .addr_mode = ADDR_INDIRECT
};
Opcode op_6D = {
  .name = "ADC", .op = &do_6D, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_6E = {
  .name = "ROR", .op = &do_6E, .write = true, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_70 = {
  .name = "BVS", .op = &do_70, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_71 = {
  .name = "ADC", .op = &do_71, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_75 = {
  .name = "ADC", .op = &do_75, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_76 = {
  .name = "ROR", .op = &do_76, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_78 = {
  .name = "SEI", .op = &do_78, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_79 = {
  .name = "ADC", .op = &do_79, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_7D = {
  .name = "ADC", .op = &do_7D, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_7E = {
  .name = "ROR", .op = &do_7E, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_81 = {
  .name = "STA", .op = &do_81, .write = true, .addr_mode = ADDR_INDEX_IND
};
Opcode op_84 = {
  .name = "STY", .op = &do_84, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_85 = {
  .name = "STA", .op = &do_85, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_86 = {
  .name = "STX", .op = &do_86, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_88 = {
  .name = "DEY", .op = &do_88, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_8A = {
  .name = "TXA", .op = &do_8A, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_8C = {
  .name = "STY", .op = &do_8C, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_8D = {
  .name = "STA", .op = &do_8D, .write = true, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_8E = {
  .name = "STX", .op = &do_8E, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_90 = {
  .name = "BCC", .op = &do_90, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_91 = {
  .name = "STA", .op = &do_91, .write = true, .addr_mode = ADDR_IND_INDEX
};
Opcode op_94 = {
  .name = "STY", .op = &do_94, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_95 = {
  .name = "STA", .op = &do_95, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_96 = {
  .name = "STX", .op = &do_96, .write = false, .addr_mode = ADDR_ZPG_Y
};
Opcode op_98 = {
  .name = "TYA", .op = &do_98, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_99 = {
  .name = "STA", .op = &do_99, .write = true, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_9A = {
  .name = "TXS", .op = &do_9A, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_9D = {
  .name = "STA", .op = &do_9D, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_A0 = {
  .name = "LDY", .op = &do_A0, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_A1 = {
  .name = "LDA", .op = &do_A1, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_A2 = {
  .name = "LDX", .op = &do_A2, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_A4 = {
  .name = "LDY", .op = &do_A4, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_A5 = {
  .name = "LDA", .op = &do_A5, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_A6 = {
  .name = "LDX", .op = &do_A6, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_A8 = {
  .name = "TAY", .op = &do_A8, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_A9 = {
  .name = "LDA", .op = &do_A9, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_AA = {
  .name = "TAX", .op = &do_AA, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_AC = {
  .name = "LDY", .op = &do_AC, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_AD = {
  .name = "LDA", .op = &do_AD, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_AE = {
  .name = "LDX", .op = &do_AE, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_B0 = {
  .name = "BCS", .op = &do_B0, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_B1 = {
  .name = "LDA", .op = &do_B1, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_B4 = {
  .name = "LDY", .op = &do_B4, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_B5 = {
  .name = "LDA", .op = &do_B5, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_B6 = {
  .name = "LDX", .op = &do_B6, .write = false, .addr_mode = ADDR_ZPG_Y
};
Opcode op_B8 = {
  .name = "CLV", .op = &do_B8, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_B9 = {
  .name = "LDA", .op = &do_B9, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_BA = {
  .name = "TSX", .op = &do_BA, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_BC = {
  .name = "LDY", .op = &do_BC, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_BD = {
  .name = "LDA", .op = &do_BD, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_BE = {
  .name = "LDX", .op = &do_BE, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_C0 = {
  .name = "CPY", .op = &do_C0, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_C1 = {
  .name = "CMP", .op = &do_C1, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_C4 = {
  .name = "CPY", .op = &do_C4, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_C5 = {
  .name = "CMP", .op = &do_C5, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_C6 = {
  .name = "DEC", .op = &do_C6, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_C8 = {
  .name = "INY", .op = &do_C8, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_C9 = {
  .name = "CMP", .op = &do_C9, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_CA = {
  .name = "DEX", .op = &do_CA, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_CC = {
  .name = "CPY", .op = &do_CC, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_CD = {
  .name = "CMP", .op = &do_CD, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_CE = {
  .name = "DEC", .op = &do_CE, .write = true, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_D0 = {
  .name = "BNE", .op = &do_D0, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_D1 = {
  .name = "CMP", .op = &do_D1, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_D5 = {
  .name = "CMP", .op = &do_D5, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_D6 = {
  .name = "DEC", .op = &do_D6, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_D8 = {
  .name = "CLD", .op = &do_D8, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_D9 = {
  .name = "CMP", .op = &do_D9, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_DD = {
  .name = "CMP", .op = &do_DD, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_DE = {
  .name = "DEC", .op = &do_DE, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_E0 = {
  .name = "CPX", .op = &do_E0, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_E1 = {
  .name = "SBC", .op = &do_E1, .write = false, .addr_mode = ADDR_INDEX_IND
};
Opcode op_E4 = {
  .name = "CPX", .op = &do_E4, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_E5 = {
  .name = "SBC", .op = &do_E5, .write = false, .addr_mode = ADDR_ZPG
};
Opcode op_E6 = {
  .name = "INC", .op = &do_E6, .write = true, .addr_mode = ADDR_ZPG
};
Opcode op_E8 = {
  .name = "INX", .op = &do_E8, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_E9 = {
  .name = "SBC", .op = &do_E9, .write = false, .addr_mode = ADDR_IMMEDIATE
};
Opcode op_EA = {
  .name = "NOP", .op = &do_EA, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_EC = {
  .name = "CPX", .op = &do_EC, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_ED = {
  .name = "SBC", .op = &do_ED, .write = false, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_EE = {
  .name = "INC", .op = &do_EE, .write = true, .addr_mode = ADDR_ABSOLUTE
};
Opcode op_F0 = {
  .name = "BEQ", .op = &do_F0, .write = false, .addr_mode = ADDR_RELATIVE
};
Opcode op_F1 = {
  .name = "SBC", .op = &do_F1, .write = false, .addr_mode = ADDR_IND_INDEX
};
Opcode op_F5 = {
  .name = "SBC", .op = &do_F5, .write = false, .addr_mode = ADDR_ZPG_X
};
Opcode op_F6 = {
  .name = "INC", .op = &do_F6, .write = true, .addr_mode = ADDR_ZPG_X
};
Opcode op_F8 = {
  .name = "SED", .op = &do_F8, .write = false, .addr_mode = ADDR_IMPLICIT
};
Opcode op_F9 = {
  .name = "SBC", .op = &do_F9, .write = false, .addr_mode = ADDR_ABSOLUTE_Y
};
Opcode op_FD = {
  .name = "SBC", .op = &do_FD, .write = false, .addr_mode = ADDR_ABSOLUTE_X
};
Opcode op_FE = {
  .name = "INC", .op = &do_FE, .write = true, .addr_mode = ADDR_ABSOLUTE_X
};

Opcode* opcodes[0x100] = {
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
  (*opcodes[cpu->IR >> 3]->op)(cpu);
}
