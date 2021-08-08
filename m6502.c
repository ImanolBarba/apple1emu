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
#include "errors.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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

void cpu_crash(M6502* cpu) {
  *cpu->stop = true;
  fprintf(stderr, "!! CPU CRASH !!\n");
  fprintf(stderr, "== REGISTERS ==\n");
  fprintf(stderr, "PC=0x%04X\n", cpu->PC);
  fprintf(stderr, "S=0x%02X\n", cpu->S);
  fprintf(stderr, "P=0x%02X\n", cpu->status);
  fprintf(stderr, "X=0x%02X\n", cpu->X);
  fprintf(stderr, "Y=0x%02X\n", cpu->Y);
  fprintf(stderr, "\n");

  fprintf(stderr, "== INTERNAL STATUS ==\n");
  fprintf(stderr, "AD=0x%04X\n", cpu->AD);
  fprintf(stderr, "break_status=0x%02X\n", cpu->break_status);
  fprintf(stderr, "IR=0x%04X\n", cpu->IR);
  fprintf(stderr, "tick_count=%llu\n", cpu->tick_count);
  fprintf(stderr, "\n");

  fprintf(stderr, "== EXTERNAL INTERFACE ==\n");
  fprintf(stderr, "addr_bus=0x%04X\n", *cpu->addr_bus);
  fprintf(stderr, "data=0x%02X\n", *cpu->data_bus);
  fprintf(stderr, "IRQ=%s\n", *cpu->IRQ ? "HI" : "LO");
  fprintf(stderr, "NMI=%s\n", *cpu->NMI ? "HI" : "LO");
  fprintf(stderr, "RES=%s\n", *cpu->RES ? "HI" : "LO");
  fprintf(stderr, "RDY=%s\n", *cpu->RDY ? "HI" : "LO");
  fprintf(stderr, "RW=%s\n", cpu->RW ? "HI" : "LO");
  fprintf(stderr, "SO=%s\n", *cpu->SO ? "HI" : "LO");
  fprintf(stderr, "SYNC=%s\n", cpu->SYNC ? "HI" : "LO");
  fprintf(stderr, "\n");

  save_state(cpu);
}

int dump_file(const char* path, uint8_t* data, size_t data_size) {
  int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if(fd == -1) {
    fprintf(stderr, "Error opening data dump\n");
    return ERROR_OPEN_FILE;
  }
  ssize_t written;
  size_t total = 0;
  while(total != data_size) {
    written = write(fd, data + total, data_size - total);
    if(written < 0) {
      if(errno == EINTR) {
        continue;
      }
      fprintf(stderr, "Error writing data\n");
      close(fd);
      return ERROR_WRITE_FILE;
    }
    total += written;
  }
  close(fd);
  return SUCCESS;
}

int load_dump(const char* path, uint8_t* data, size_t data_size) {
  int fd = open(path, O_RDONLY);
  if(fd == -1) {
    fprintf(stderr, "Error opening data dump\n");
    return ERROR_OPEN_FILE;
  }
  ssize_t bytes_read;
  size_t total = 0;
  while(total != data_size) {
    bytes_read = read(fd, data + total, data_size - total);
    if(bytes_read < 0) {
      if(errno == EINTR) {
        continue;
      }
      fprintf(stderr, "Error reading data\n");
      close(fd);
      return ERROR_READ_FILE;
    }
    total += bytes_read;
  }
  close(fd);
  return SUCCESS;
}


int save_state(M6502* cpu) {
  // Dump registers, external pins and buses
  M6502_Registers regs;
  regs.A = cpu->A;
  regs.X = cpu->X;
  regs.Y = cpu->Y;
  regs.S = cpu->S;
  regs.status = cpu->status;
  regs.PC = cpu->PC;
  regs.data_bus = *cpu->data_bus;
  regs.addr_bus = *cpu->addr_bus;
  regs.RW = cpu->RW ? 1 : 0;
  regs.SYNC = cpu->SYNC ? 1 : 0;
  if(dump_file("regs_dump", (uint8_t*)&regs, sizeof(regs)) != SUCCESS) {
    return FAILURE;
  }
  fprintf(stderr, "Register status dumped to \"regs_dump\"\n");

  uint8_t mem[MEMSIZE];
  for(unsigned int i = 0; i < MEMSIZE; ++i) {
    *cpu->addr_bus = i;
    clock_cpu((void*)cpu, true);
    mem[i] = *cpu->data_bus;
  }
  if(dump_file("mem_dump", mem, MEMSIZE) != SUCCESS) {
    return FAILURE;
  }
  fprintf(stderr, "Memory dumped to \"mem_dump\"\n");

  return SUCCESS;
}

int load_state(M6502* cpu) {
  // Load the stuff back. We have to guarantee that if something fails, the
  // current state won't change

  M6502_Registers regs;
  uint8_t mem[MEMSIZE];

  if(load_dump("regs_dump", (uint8_t*)&regs, sizeof(regs)) != SUCCESS) {
    return FAILURE;
  }
  if(load_dump("mem_dump", mem, MEMSIZE) != SUCCESS) {
    return FAILURE;
  }

  regs.A = cpu->A;
  regs.X = cpu->X;
  regs.Y = cpu->Y;
  regs.S = cpu->S;
  regs.status = cpu->status;
  regs.PC = cpu->PC;
  regs.data_bus = *cpu->data_bus;
  regs.addr_bus = *cpu->addr_bus;
  regs.RW = cpu->RW ? 1 : 0;
  regs.SYNC = cpu->SYNC ? 1 : 0;
  fprintf(stderr, "Loaded registers\n");

  for(unsigned int i = 0; i < MEMSIZE; ++i) {
    *cpu->addr_bus = i;
    clock_cpu((void*)cpu, true);
    mem[i] = *cpu->data_bus;
  }
  fprintf(stderr, "Loaded memory\n");

  return SUCCESS;
}

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

void cpu_cycle(M6502* cpu) {
  if(*cpu->stop) {
    // CPU is disabled
    return;
  }
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
