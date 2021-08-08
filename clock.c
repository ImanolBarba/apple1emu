/***************************************************************************
 *   clock.c  --  This file is part of apple1emu.                          *
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

#include "clock.h"
#include "errors.h"

#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

void init_clock(Clock* c, unsigned int freq) {
  c->freq = freq;
  c->num_chips = 0;
  memset(c->clock_bus, 0, MAX_CHIPS_ON_BUS * sizeof(Connected_chip*));
}

int clock_connect(Clock* c, Connected_chip* chip) {
  if(c->num_chips == MAX_CHIPS_ON_BUS) {
    return ERROR_TOO_MANY_CHIPS_ON_CLOCK;
  }
  c->clock_bus[c->num_chips++] = chip;
  return SUCCESS;
}

void *clock_run(void* ptr) {
  Clock* c = (Clock*)ptr;
  while(!(*c->stop)) {
    // TODO: Right now, things run at full speed. Can I find a way to limit cpu
    // usage to reproduce the original 1MHz clock speed?
    tick(c);
    tock(c);
  }
  fprintf(stderr, "Stopping clock thread...\n");
  pthread_exit(0);
}

void tick(Clock* c) {
  for(int i = 0; i < c->num_chips; ++i) {
    Connected_chip* chip = c->clock_bus[i];
    (*chip->callback)(chip->chip, 1);
  }
}

void tock(Clock* c) {
  for(int i = 0; i < c->num_chips; ++i) {
    Connected_chip* chip = c->clock_bus[i];
    (*chip->callback)(chip->chip, 0);
  }
}
