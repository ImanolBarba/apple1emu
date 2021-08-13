/***************************************************************************
 *   clock.h  --  This file is part of apple1emu.                          *
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

#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <pthread.h>

#define MAX_CHIPS_ON_BUS 0xFF
#define TICKS_FOR_SYNC 1000
#define CLOCK_ADJUST_GRANULARITY 1e5

typedef void (*clock_callback)(void*, bool);
typedef struct {
  clock_callback callback;
  void* chip;
} Connected_chip;

typedef struct {
  unsigned int freq;
  Connected_chip* clock_bus[MAX_CHIPS_ON_BUS];
  unsigned int num_chips;
  volatile bool* stop;
  long int clock_adjust;
  volatile bool turbo;
} Clock;

void init_clock(Clock* c, unsigned int freq);
int clock_connect(Clock* c, Connected_chip* callback);
void *clock_run(void* ptr);
void tick(Clock* c);
void tock(Clock* c);

#endif
