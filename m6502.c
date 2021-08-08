/***************************************************************************
 *   m6502.c  --  This file is part of apple1emu.                          *
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

void clock_cpu(void* ptr, bool status) {
  M6502* cpu = (M6502*)ptr;
  if(status) {
    tock(&cpu->phi1);
    cpu_cycle(cpu);
    tick(&cpu->phi2);
  } else {
    tock(&cpu->phi2);
    tick(&cpu->phi1);
  }
}

void init_cpu(M6502* cpu) {
  cpu->tick_count = 0;

  cpu->A = 0;
  cpu->X = 0;
  cpu->Y = 0;
  cpu->S = 0xFF;
  cpu->status = 0;
  cpu->AD = 0;

  cpu->ADH = (((uint8_t*)&cpu->AD)+1);
  cpu->ADL = ((uint8_t*)&cpu->AD);

  cpu->PCH = (((uint8_t*)&cpu->PC)+1);
  cpu->PCL = ((uint8_t*)&cpu->PC);

  *cpu->RES = false;
  cpu->RW = true;
  cpu->SYNC = true;
}

void get_arg_indirect_index(M6502* cpu) {
    switch(cpu->IR & 0x3) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      cpu->AD = *cpu->data_bus;
      *cpu->addr_bus = cpu->AD; // zpg
    break;
    case 2:
      *cpu->addr_bus = (cpu->AD+1) & 0x00FF; // zpg wraps around
      cpu->AD = *cpu->data_bus; // low addr
    break;
    case 3:
      cpu->AD |= *cpu->data_bus << 8; // full addr
      *cpu->addr_bus = (cpu->AD & 0xFF00) | ((cpu->AD + cpu->Y) & 0x00FF);
      if((cpu->AD & 0x00FF) + cpu->Y <= 0xFF) {
        // if we're on the same page already, 1 less cycle
        cpu->IR++;
      }
    break;
    case 4:
      *cpu->addr_bus = cpu->AD + cpu->Y;
    break;
  }
}

void get_arg_index_indirect(M6502* cpu) {
  switch(cpu->IR & 0x3) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      cpu->AD = *cpu->data_bus;
      *cpu->addr_bus = cpu->AD;
    break;
    case 2:
      cpu->AD = (cpu->AD + cpu->X) & 0x00FF;
      *cpu->addr_bus = cpu->AD;
    break;
    case 3:
      *cpu->addr_bus = (cpu->AD+1) & 0x00FF;
      cpu->AD = *cpu->data_bus;
    break;
    case 4:
      *cpu->addr_bus = *cpu->data_bus << 8 | cpu->AD;
    break;
  }
}

void get_arg_zero_page(M6502* cpu) {
  switch(cpu->IR & 0x3) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      cpu->AD = *cpu->data_bus;
      *cpu->addr_bus = cpu->AD;
    break;
  }
}

void get_arg_zero_page_index(M6502* cpu, uint8_t index) {
  if((cpu->IR & 0x3) < 2) {
    get_arg_zero_page(cpu);
    return;
  }
  *cpu->addr_bus = (cpu->AD + index) & 0x00FF;
}

void get_arg_absolute(M6502* cpu) {
  switch(cpu->IR & 0x3) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      *cpu->addr_bus = cpu->PC++;
      cpu->AD = *cpu->data_bus;
    break;
    case 2:
      *cpu->addr_bus = *cpu->data_bus << 8 | cpu->AD;
    break;
  }
}

void get_arg_absolute_index(M6502* cpu, uint8_t index) {
  switch(cpu->IR & 0x3) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      *cpu->addr_bus = cpu->PC++;
      cpu->AD = *cpu->data_bus;
    break;
    case 2:
      cpu->AD |= *cpu->data_bus << 8;
      *cpu->addr_bus = (cpu->AD & 0xFF00) | ((cpu->AD + index) & 0x00FF);
      if((cpu->AD & 0x00FF) + index <= 0xFF) {
        cpu->IR++;
      }
    break;
    case 3:
      *cpu->addr_bus = cpu->AD + index;
    break;
  }
}

void branch(M6502* cpu, bool condition) {
  switch(cpu->IR & IR_STATUS_MASK) {
    case 0:
      *cpu->addr_bus = cpu->PC++;
    break;
    case 1:
      *cpu->addr_bus = cpu->PC;
      cpu->AD = cpu->PC + (int8_t)(*cpu->data_bus);
      if(!condition) {
        fetch(cpu);
      }
    break;
    case 2:
      *cpu->addr_bus = (cpu->PC & 0xFF00) | (cpu->AD & 0x00FF);
      // God knows how this is implemented in hw, but the thing is, if the addr
      // is in another page, it takes an extra cycle, so yeah
      if((cpu->AD & 0xFF00) == (cpu->PC & 0xFF00)) {
        cpu->PC=cpu->AD;
        fetch(cpu);
      }
    break;
    case 3:
      cpu->PC = cpu->AD;
      fetch(cpu);
    break;
  }
}

void fetch(M6502* cpu) {
  *cpu->addr_bus = cpu->PC;
  cpu->SYNC = true;
}

void push_stack(M6502* cpu, uint8_t data) {
  *cpu->addr_bus = STACK_TOP_ADDR | cpu->S--;
  *cpu->data_bus = data;
  cpu->RW = false;
}

void do_ORA(M6502* cpu) {
  cpu->A |= *cpu->data_bus;
  if(cpu->A < 0) {
    cpu->status |= STATUS_NF;
  } else if(!cpu->A) {
    cpu->status |= STATUS_ZF;
  }
}

void do_AND(M6502* cpu) {
  cpu->A &= *cpu->data_bus;
  if(cpu->A < 0) {
    cpu->status |= STATUS_NF;
  } else if(!cpu->A) {
    cpu->status |= STATUS_ZF;
  }
}

uint8_t do_ASL(M6502* cpu, uint8_t x) {
  uint8_t res = x << 1;
  if(x & 0x80) {
    cpu->status |= STATUS_CF;
  }
  if(cpu->A < 0) {
    cpu->status |= STATUS_NF;
  } else if(!cpu->A) {
    cpu->status |= STATUS_ZF;
  }
  return res;
}

uint8_t do_ROL(M6502* cpu, uint8_t x) {
  uint8_t res = x << 1 | (cpu->status & STATUS_CF) ;
  if(x & 0x80) {
    cpu->status |= STATUS_CF;
  }
  if(cpu->A < 0) {
    cpu->status |= STATUS_NF;
  } else if(!cpu->A) {
    cpu->status |= STATUS_ZF;
  }
  return res;
}

void cpu_cycle(M6502* cpu) {
  cpu->tick_count++;

  if(!*cpu->RES) {
    cpu->break_status |= BRK_RST;
  }
  if(!*cpu->NMI) {
    cpu->break_status |= BRK_NMI;
  }
  if(!*cpu->IRQ) {
    cpu->break_status |= BRK_IRQ;
  }

  if(!*cpu->RDY && cpu->RW) {
    // RDY is only checked during READ cycle on the original 6502
    return;
  }

  if(cpu->SYNC) {
    if(cpu->break_status) {
      cpu->IR = 0x00; // BRK
    } else {
      cpu->IR = *cpu->data_bus << 3;
      cpu->PC++;
    }
    // Not fetching the new opcode anymore, deassert SYNC
    cpu->SYNC = false;
  }

  cpu->RW = true;
  run_opcode(cpu);
  cpu->IR++;
}
