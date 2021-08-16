
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
#include "m6502_opcodes.h"
#include "debug.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

float emulation_speed = 0.0;

volatile uint16_t address_bus;
volatile uint8_t data_bus;
volatile bool reset_line;
volatile bool poweroff = false;
volatile bool debug_mode = false;

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
  if(extra_data != NULL) {
    load_data(&extra_ram, extra_data, extra_length, START_EXTRA_RAM);
    if(ret != SUCCESS) {
      return FAILURE;
    }
  }

  ret = init_mem(&rom, START_ROM, END_ROM);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  rom.addr_bus = &address_bus;
  rom.data_bus = &data_bus;
  rom.RW = &read_only;
  load_data(&rom, rom_data, rom_length, START_ROM);
  if(ret != SUCCESS) {
    return FAILURE;
  }

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

int init_apple1_binary(uint8_t* binary_data, size_t binary_length, uint16_t start_addr, uint16_t load_addr) {
  int ret;

  // Connect RAM
  ret = init_mem(&user_ram, START_USER_RAM, MEMSIZE-1);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  user_ram.addr_bus = &address_bus;
  user_ram.data_bus = &data_bus;
  user_ram.RW = &cpu.RW;

  load_data(&user_ram, binary_data, binary_length, load_addr);
  if(ret != SUCCESS) {
    return FAILURE;
  }

  user_ram.mem[0xFFFC] = start_addr & 0x00FF;
  user_ram.mem[0xFFFD] = (start_addr & 0xFF00) >> 8;

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

  init_clock(&main_clock, CLOCK_SPEED);
  ret = clock_connect(&main_clock, &cpu_callback);
  if(ret != SUCCESS) {
    return FAILURE;
  }
  main_clock.stop = &poweroff;

  return SUCCESS;
}

void process_emulator_input(char key) {
  switch(key) {
    case EMULATOR_CONTINUE:
        debug_mode = false;
    break;
    case EMULATOR_RESET:
      reset_line = false;
      clear_screen();
    break;
    case EMULATOR_BREAK:
      debug_mode = true;
      poweroff = true;
    break;
    case EMULATOR_STEP_INSTRUCTION:
      if(debug_mode) {
        do{
          tick(&main_clock);
          tock(&main_clock);
        } while(!cpu.SYNC);
        print_disassembly(&cpu, cpu.PC, 1);
      }
    break;
    case EMULATOR_STEP_CLOCK:
      if(debug_mode) {
        tick(&main_clock);
        tock(&main_clock);
        if(cpu.SYNC) {
          print_disassembly(&cpu, cpu.PC, 1);
        }
      }
    break;
    case EMULATOR_PRINT_CYCLES:
      fprintf(stderr, "cycles per second: %.2f\n", emulation_speed);
    break;
    case EMULATOR_SAVE_STATE:
      save_state(&cpu);
    break;
    case EMULATOR_LOAD_STATE:
      load_state(&cpu);
    break;
    case EMULATOR_TURBO:
      main_clock.turbo = !main_clock.turbo;
      fprintf(stderr, "Turbo mode: %s\n", main_clock.turbo ? "ON" : "OFF");
    break;
  }
}

void print_greeting() {
  printf("                   _        _                        \n");
  printf("  __ _ _ __  _ __ | | ___  / |   ___ _ __ ___  _   _ \n");
  printf(" / _` | '_ \\| '_ \\| |/ _ \\ | |  / _ \\ '_ ` _ \\| | | |\n");
  printf("| (_| | |_) | |_) | |  __/ | | |  __/ | | | | | |_| |\n");
  printf(" \\__,_| .__/| .__/|_|\\___| |_|  \\___|_| |_| |_|\\__,_|\n");
  printf("      |_|   |_|                                      \n");
  printf("\n");
  printf("`: Clear screen                         TAB: Toggle turbo mode\n");
  printf("F5: Resume execution (From debugger)    F8: Reset\n");
  printf("F6: Save state                          F9: Break to debugger\n");
  printf("F7: Load state                          F12: Print emulation speed\n");
  printf("\n\n");
}

void print_debugger_help() {
  printf("n or next: Step clock until the next instruction fetch\n");
  printf("s or step: Step clock one full cycle\n");
  printf("c or continue: Exit debugger and resume execution\n");
  printf("l or list <ADDR>: Disassemble a bunch of instructions from this address\n");
  printf("b or breakpoint <ADDR>: Break when we try to execute this address\n");
  printf("bw or breakpointw <ADDR>: Break when we try to write this address\n");
  printf("br or breakpointr <ADDR>: Break when we try to read this address\n");
  printf("p or print PC/A/X/Y/S/<ADDR>: Print the value of the specified register or memory\n");
  printf("set PC/A/X/Y/S/<ADDR> <VALUE>: Change value of the specified register or memory\n");
  printf("h or help: This thing\n");
  printf("q or quit: Exit the emulator\n");
}

int main_loop() {
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
  while(!poweroff) {
    // Main control loop
    unsigned int start_ticks = cpu.tick_count;
    sleep(1);
    emulation_speed = (float)((cpu.tick_count - start_ticks));
    if(!main_clock.turbo) {
      if(emulation_speed > CLOCK_SPEED) {
        main_clock.clock_adjust -= CLOCK_ADJUST_GRANULARITY;
      } else if(emulation_speed < CLOCK_SPEED) {
        main_clock.clock_adjust += CLOCK_ADJUST_GRANULARITY;
      }
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
  while(debug_mode) {
    // The main control loop ended becase a break into debugger happened,
    // we'll handle this until we resume, at which point the main control
    // loop will be restarted by the parent function

    // TODO: Need to find a way to provide input to the Apple I through
    // the debugger without writing to memory, some input buffer of some
    // kind

    // We set poweroff to false again so that the CPU can work whenever
    // we clock it manually
    poweroff = false;
    char input[64];
    char prev_input[64];
    printf("dbg> ");
    char* line_read = fgets(input, 64, stdin);
    if(line_read) {
      line_read[strcspn(line_read, "\n")] = '\0';
      if(*line_read == '\0') {
        // if empty line, replay last command
        memcpy(input, prev_input, sizeof(input));  
      } else {
        memcpy(prev_input, input, sizeof(input));
      }
      if(!strncmp(line_read, "next", 4) || !strncmp(line_read, "n", 1)) {
        process_emulator_input(EMULATOR_STEP_INSTRUCTION);
      } else if(!strncmp(line_read, "set ", 4)) {
        char* arg1 = read_arg(input);
        char* arg2 = read_arg(arg1);
        if(arg1 != NULL && arg2 != NULL) {
          uint16_t value;
          int ret = parse_hex(arg2, &value);
          if(ret != SUCCESS) {
            printf("Invalid value specified\n");
          } else {
            ret = set_value(&cpu, arg1, value);
            if(ret != SUCCESS) {
              printf("Invalid address or register specified\n");
            }
          }
        } else {
          printf("Missing argument\n");
        }
      } else if(!strncmp(line_read, "step", 4) || !strncmp(line_read, "s", 1)) {
        process_emulator_input(EMULATOR_STEP_CLOCK);
      } else if(!strncmp(line_read, "continue", 8) || !strncmp(line_read, "c", 1)) {
        process_emulator_input(EMULATOR_CONTINUE);
      } else if(!strncmp(line_read, "help", 4) || !strncmp(line_read, "h", 1)) {
        print_debugger_help();
      } else if(!strncmp(line_read, "breakpoint ", 11) || !strncmp(line_read, "b ", 2)) {
        // b/breakpoint ADDR
        printf("TODO: breakpoint\n");
      } else if(!strncmp(line_read, "breakpointw ", 12) || !strncmp(line_read, "bw ", 3)) {
        // bw/breakpointw ADDR
        printf("TODO: breakpointw\n");
      } else if(!strncmp(line_read, "breakpointr ", 12) || !strncmp(line_read, "br ", 3)) {
        // br/breakpointr ADDR
        printf("TODO: breakpointr\n");
      } else if(!strncmp(line_read, "list ", 5) || !strncmp(line_read, "l ", 3)) {
        char* arg1 = read_arg(input);
        if(arg1 != NULL) {
          uint16_t addr;
          int ret = parse_hex(arg1, &addr);
          if(ret != SUCCESS) {
            printf("Invalid address specified\n");
          } else {
            print_disassembly(&cpu, addr, 10);
          }
        }
      } else if(!strncmp(line_read, "print ", 6)  || !strncmp(line_read, "p ", 2)) {
        char* arg1 = read_arg(input);
        if(arg1 != NULL) {
          int ret = print_value(&cpu, arg1);
          if(ret != SUCCESS) {
            printf("Invalid address or register specified\n");
          }
        } else {
          printf("Missing argument\n");
        }
      } else if(!strncmp(line_read, "quit", 4) || !strncmp(line_read, "q", 1)) {
        // When we exit the debug_mode loop, it'll either be with poweroff = false, because
        // a continue was called, or poweroff = true because of this break here, which will
        // cause the emulator to shut down, which is what we want
        poweroff = true;
        break;
      } else {
        printf("Unrecognised command: %s\n", input);
      }
    } else if(feof(stdin)) {
      printf("\n");
      poweroff = true;
      debug_mode = false;
    }
  }

  return SUCCESS;
}

int boot_apple1() {
  init_cpu(&cpu);
  clear_screen();
  print_greeting();
  while(!poweroff) {
    init_pia();
    // This loop basically checks if we exited the main loop but poweroff is not true, so we may
    // restart it again. This would happen if we resumed from the debugger.
    int ret = main_loop();
    if(ret != SUCCESS) {
      return FAILURE;
    }
  }
  destroy_mem(&user_ram);
  destroy_mem(&extra_ram);
  destroy_mem(&rom);

  return SUCCESS;
}

void halt_apple1() {
  if(!poweroff) {
    fprintf(stderr, "Halting CPU...\n");
  }
  poweroff = true;
  debug_mode = false;
}
