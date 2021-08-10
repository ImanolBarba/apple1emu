
/***************************************************************************
 *   apple1.c  --  This file is part of apple1emu.                         *
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

#include "mem.h"
#include "m6502.h"
#include "errors.h"
#include "apple1.h"
#include "pia6821.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

// TODO: reset triggered by user
// TODO: debugger

volatile uint16_t address_bus;
volatile uint8_t data_bus;
volatile bool reset_line;
volatile bool poweroff = false;

M6502 cpu;
Connected_chip cpu_callback = {
  .callback = &clock_cpu,
  .chip = &cpu,
};

Clock main_clock;

Mem_16 user_ram;
Connected_chip user_ram_callback = {
  .callback = &clock_mem,
  .chip = &user_ram,
};
Mem_16 extra_ram;
Connected_chip extra_ram_callback = {
  .callback = &clock_mem,
  .chip = &extra_ram,
};
Mem_16 rom;
Connected_chip rom_callback = {
  .callback = &clock_mem,
  .chip = &rom,
};

PIA6821 pia;
Connected_chip pia_callback = {
  .callback = &clock_pia,
  .chip = &pia,
};

bool read_only = true;
bool on = true;
bool off = false;

int init_apple1(size_t user_ram_size, uint8_t* rom_data, size_t rom_length, uint8_t* extra_data, size_t extra_length) {
  int ret;

  // Connect PIA
  pia.addr_bus = &address_bus;
  pia.data_bus = &data_bus;
  pia.RW = &cpu.RW;
  pia.CRA_ADDR = KBDCR;
  pia.CRB_ADDR = DSPCR;
  pia.PA_ADDR = KBD;
  pia.PB_ADDR = DSP;
  pia.RES = &reset_line;

  // Connect RAMs and ROM
  if(user_ram_size > MAX_USER_RAM) {
    fprintf(stderr, "Requested too much user memory. Maximum is 0x%02X\n", MAX_USER_RAM);
    return ERROR_TOO_MUCH_USER_MEMORY;
  }
  ret = init_mem(&user_ram, START_USER_RAM, user_ram_size - 1);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  user_ram.addr_bus = &address_bus;
  user_ram.data_bus = &data_bus;
  user_ram.RW = &cpu.RW;

  ret = init_mem(&extra_ram, START_EXTRA_RAM, END_EXTRA_RAM);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  extra_ram.addr_bus = &address_bus;
  extra_ram.data_bus = &data_bus;
  extra_ram.RW = &(cpu.RW);
  load_data(&extra_ram, extra_data, extra_length, START_EXTRA_RAM);

  ret = init_mem(&rom, START_ROM, END_ROM);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  rom.addr_bus = &address_bus;
  rom.data_bus = &data_bus;
  rom.RW = &read_only;
  load_data(&rom, rom_data, rom_length, START_ROM);

  // Connect CPU
  cpu.addr_bus = &address_bus;
  cpu.data_bus = &data_bus;
  cpu.IRQ = &on;
  cpu.NMI = &on;
  cpu.RDY = &on;
  cpu.SO = &on;
  cpu.RES = &reset_line;
  // If the CPU stops, shut the rest of the stuff down
  cpu.stop = &poweroff;
  ret = clock_connect(&cpu.phi2, &user_ram_callback);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  ret = clock_connect(&cpu.phi2, &extra_ram_callback);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  ret = clock_connect(&cpu.phi2, &rom_callback);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  ret = clock_connect(&cpu.phi2, &pia_callback);
  if(ret != SUCCESS) {
    return FAILURE;
  }

  init_clock(&main_clock, CLOCK_SPEED);
  ret = clock_connect(&main_clock, &cpu_callback);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  main_clock.stop = &poweroff;

  return SUCCESS;
}

int boot_apple1(unsigned int perf_counter_freq) {
  init_pia();
  init_cpu(&cpu);
  pthread_t clock_thread;
  pthread_t input_thread;
  if(pthread_create(&input_thread, NULL, input_run, (void*)&poweroff)) {
    fprintf(stderr, "Error creating thread\n");
    return ERROR_PTHREAD_CREATE;
  }
  if(pthread_create(&clock_thread, NULL, clock_run, &main_clock)) {
    fprintf(stderr, "Error creating thread\n");
    return ERROR_PTHREAD_CREATE;
  }

  unsigned long long int perf_counter = 0;
  if(!perf_counter_freq) {
    while(!poweroff) {
      sleep(1);
    }
  } else {
    while(!poweroff) {
      // Main control loop
      sleep(perf_counter_freq);
      fprintf(stderr, "cycles per second: %.2f\n", (float)(cpu.tick_count/(perf_counter_freq*(++perf_counter))));
    }
  }
  // Send SIGINT to the input thread so that the read syscall gets interrupted
  if(pthread_kill(input_thread, SIGINT)) {
    fprintf(stderr, "Error signaling input thread, it probably already finished\n");
  }

  if(pthread_join(clock_thread, NULL)) {
    fprintf(stderr, "Error joining clock thread\n");
    return ERROR_PTHREAD_JOIN;
  }
  if(pthread_join(input_thread, NULL)) {
    fprintf(stderr, "Error joining input thread\n");
    return ERROR_PTHREAD_JOIN;
  }

  destroy_mem(&user_ram);
  destroy_mem(&extra_ram);
  destroy_mem(&rom);

  return SUCCESS;
}

void halt_apple1() {
  if(!poweroff) {
    fprintf(stderr, "Halting CPU...\n");
    poweroff = true;
  }
}
