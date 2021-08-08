/***************************************************************************
 *   mem.c  --  This file is part of apple1emu.                            *
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

#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "errors.h"

size_t get_memsize(Mem_16* m) {
  return (m->end_addr - m->start_addr) + 1;
}

int init_mem(Mem_16* m, uint16_t start, uint16_t end) {
  if(end < start) {
    fprintf(stderr, "Trying to set memory end address before start address");
    return ERROR_INVALID_MEMORY_SETUP;
  }
  m->start_addr = start;
  m->end_addr = end;
  size_t mem_size = get_memsize(m);
  m->mem = calloc(mem_size, sizeof(uint8_t));
  if(m->mem == NULL) {
    fprintf(stderr, "Unable to alloc memory");
    return ERROR_MEMORY_ALLOC;
  }
  return SUCCESS;
}

bool is_enabled_mem(Mem_16* m) {
  return (*(m->addr_bus) >= m->start_addr) && (*(m->addr_bus) <= m->end_addr);
}

void clock_mem(void* ptr, bool status) {
  Mem_16* m = (Mem_16*)ptr;
  if(is_enabled_mem(m) && status) {
    if(!*m->RW) {
      m->mem[*(m->addr_bus) - m->start_addr] = *(m->data_bus);
    } else {
      *(m->data_bus) = m->mem[*(m->addr_bus) - m->start_addr];
    }
  }
}

// addr is absolute to final memory map
int load_data(Mem_16* m, uint8_t* data, size_t data_size, uint16_t addr) {
  if(addr < m->start_addr) {
    fprintf(stderr, "Load address does not belong to this memory\n");
    return ERROR_INVALID_MEMORY_RANGE;
  }
  if((addr + data_size) > get_memsize(m) + m->start_addr) {
    fprintf(stderr, "Data is too large for the specified address\n");
    return ERROR_DATA_TOO_LARGE;
  }
  memcpy(m->mem + addr - m->start_addr, data, data_size);
  return SUCCESS;
}

void destroy_mem(Mem_16* m) {
  free(m->mem);
}
