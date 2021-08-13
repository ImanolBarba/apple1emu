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
#include <sys/time.h>

void init_clock(Clock* c, unsigned int freq) {
  c->freq = freq;
  c->num_chips = 0;
  memset(c->clock_bus, 0, MAX_CHIPS_ON_BUS * sizeof(Connected_chip*));
  c->turbo = false;
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
  unsigned short tick_count = 0;
  struct timespec begin={0,0};
  struct timespec end={0,0};
  struct timespec delta={0,0};
  clock_gettime(CLOCK_MONOTONIC, &begin);
  while(!(*c->stop)) {
    tick(c);
    tock(c);
    if(c->turbo) {
      continue;
    }
    if((++tick_count == TICKS_FOR_SYNC)) {
      clock_gettime(CLOCK_MONOTONIC, &end);
      delta.tv_nsec = (1e9/c->freq)*TICKS_FOR_SYNC - (end.tv_nsec - begin.tv_nsec) - c->clock_adjust;
      nanosleep(&delta, NULL);
      clock_gettime(CLOCK_MONOTONIC, &begin);
      tick_count = 0;
    }
  }
  fprintf(stderr, "Stopping clock thread...\n");
  pthread_exit(0);
}

void tick(Clock* c) {
  for(unsigned int i = 0; i < c->num_chips; ++i) {
    Connected_chip* chip = c->clock_bus[i];
    (*chip->callback)(chip->chip, 1);
  }
}

void tock(Clock* c) {
  for(unsigned int i = 0; i < c->num_chips; ++i) {
    Connected_chip* chip = c->clock_bus[i];
    (*chip->callback)(chip->chip, 0);
  }
}
