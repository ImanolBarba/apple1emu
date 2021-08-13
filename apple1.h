/***************************************************************************
 *   apple1.h  --  This file is part of apple1emu.                         *
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

#ifndef APPLE1_H
#define APPLE1_H

#include <stdint.h>
#include <stdlib.h>

#define MAX_USER_RAM 0xD010
#define START_USER_RAM 0x0000
#define START_EXTRA_RAM 0xE000
#define END_EXTRA_RAM 0xEFFF
#define START_ROM 0xFF00
#define END_ROM 0xFFFF
#define DSP 0xD012
#define DSPCR 0xD013
#define KBD 0xD010
#define KBDCR 0xD011

#define CLOCK_SPEED 1e6
#define DEFAULT_PERF_COUNTER_FREQ 10

int init_apple1_binary(uint8_t* binary_data, size_t binary_length, uint16_t start_addr, uint16_t load_addr);
int init_apple1(size_t user_ram_size, uint8_t* rom_data, size_t rom_length, uint8_t* extra_data, size_t extra_length);
int boot_apple1();
void halt_apple1();
void process_emulator_input(char key);

#endif
