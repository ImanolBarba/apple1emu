/***************************************************************************
 *   pia6821.h  --  This file is part of apple1emu.                        *
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

#ifndef PIA6821_H
#define PIA6821_H

#include <stdint.h>
#include <stdbool.h>

#define DDR_FLAG 0x04
#define MAX_COLUMNS 40

#define ESC_KEY 0x1B
#define TILDE_KEY 0x60
#define TAB_KEY 0x09

enum emulator_input {
  NO_SEQUENCE = 0,
  UNKNOWN_SEQUENCE = 1,
  EMULATOR_CONTINUE = 2,
  EMULATOR_RESET = 3,
  EMULATOR_BREAK = 4,
  EMULATOR_STEP_INSTRUCTION = 5,
  EMULATOR_STEP_CLOCK = 6,
  EMULATOR_PRINT_CYCLES = 7,
  EMULATOR_SAVE_STATE = 8,
  EMULATOR_LOAD_STATE = 9,
  EMULATOR_TURBO = 10
};


// very limited implementation only for the Apple I, not cycle accurate and I don't care

typedef struct {
  uint8_t PA;  // KBD char
  uint8_t PB;  // high bit, yes, here, means DA is high, when printf finishes, drive it low. This is a char written by CPU
  uint8_t CRA; // high bit means char available
  uint8_t CRB; // just for show
  uint16_t PA_ADDR;  // KBD char
  uint16_t PB_ADDR;  // high bit, yes, here, means DA is high, when printf finishes, drive it low. This is a char written by CPU
  uint16_t CRA_ADDR; // high bit means char available
  uint16_t CRB_ADDR; // just for show
  uint8_t DDRA; // ignored
  uint8_t DDRB; // ignored as well
  volatile bool* RES;
  volatile uint8_t* data_bus;
  volatile uint16_t* addr_bus;
  bool* RW;
} PIA6821;

void clock_pia(void* ptr, bool status);
void init_pia();
void *input_run(void* ptr);
void clear_screen();

#endif
