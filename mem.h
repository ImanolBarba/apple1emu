/***************************************************************************
 *   mem.h  --  This file is part of apple1emu.                            *
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

#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  uint16_t start_addr;
  uint16_t end_addr;
  uint8_t* mem;
  volatile uint16_t* addr_bus;
  volatile uint8_t* data_bus;
  bool* RW;
} Mem_16;

void clock_mem(void* ptr, bool status);
int init_mem(Mem_16* m, uint16_t start, uint16_t end);
int load_data(Mem_16* m, uint8_t* data, size_t data_size, uint16_t addr);
void destroy_mem(Mem_16* m);

#endif
