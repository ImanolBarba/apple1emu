/***************************************************************************
 *   pia6821.c  --  This file is part of apple1emu.                        *
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

#include "pia6821.h"
#include "apple1.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <string.h>

struct termios orig_termios;

char ascii_to_apple[0x100] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x00, 0x0D, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x00, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x5F,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

char apple_to_ascii[0x100] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Linux has a 1024 buffer size for stdin (4096 if not reading from a tty), so
// we really don't need to implement a buffer here
unsigned char pressed_key;
volatile bool data_ready = false;

unsigned int current_col = 0;

void process_peripheral_A(PIA6821* p) {
  if(data_ready && !(p->CRA & 0x80)) {
    char translated_char = ascii_to_apple[pressed_key];
    if(translated_char != 0x00) {
      // The Apple I has PA7 always high
      p->PA = (uint8_t)translated_char | 0x80;
      p->CRA |= 0x80;
      data_ready = false;
    }
  }
}

void process_peripheral_B(PIA6821* p) {
  ssize_t written;
  if(p->PB & 0x80) {
    p->PB &= 0x7F;
    char translated_char = apple_to_ascii[p->PB];
    if(translated_char != 0x00) {
      if(translated_char == 0x0A) {
        current_col = 0;
      } else if(current_col++ == MAX_COLUMNS) {
        write(STDOUT_FILENO, "\n", 1);
        current_col = 1;
      }
      written = write(STDOUT_FILENO, &translated_char, 1);
      if(written == -1) {
        fprintf(stderr, "Error printing character to stdout\n");
      }
    }
  }
}

void clock_pia(void* ptr, bool status) {
  PIA6821* p = (PIA6821*)ptr;
  uint8_t* selected_data_register_A = &(p->DDRA);
  uint8_t* selected_data_register_B = &(p->DDRB);
  if(status) {
    if(p->CRA & DDR_FLAG) {
      // If the 2nd bit in the CR is set, the data register point to the
      // peripheral register, not the data direction one
      selected_data_register_A = &(p->PA);
    }
    if(p->CRB & DDR_FLAG) {
      selected_data_register_B = &(p->PB);
    }
    process_peripheral_A(p);
    process_peripheral_B(p);
    if(*p->RW) {
      if(*p->addr_bus == p->CRA_ADDR) {
        *p->data_bus = p->CRA;
      } else if(*p->addr_bus == p->CRB_ADDR) {
        *p->data_bus = p->CRB;
      } else if(*p->addr_bus == p->PA_ADDR) {
        *p->data_bus = *selected_data_register_A;
        // Lower high bit, signaling that the character has been read and the register is available for kbd input
        p->CRA &= 0x7F;
      } else if(*p->addr_bus == p->PB_ADDR) {
        *p->data_bus = *selected_data_register_B;
      }
    } else {
      if(*p->addr_bus == p->CRA_ADDR) {
        // Bits 6 and 7 are RO
        p->CRA = *p->data_bus & 0x3F;
      } else if(*p->addr_bus == p->CRB_ADDR) {
        // Bits 6 and 7 are RO
        p->CRB = *p->data_bus;
      } else if(*p->addr_bus == p->PA_ADDR) {
        *selected_data_register_A = *p->data_bus;
      } else if(*p->addr_bus == p->PB_ADDR) {
        // Not only set to value, but raise last bit
        *selected_data_register_B = *p->data_bus | 0x80;
      }
    }
  }
}

// return 0 if there are no more pending bytes - i.e: the user only pressed ESC
char read_escape_sequence() {
  char sequence_buffer[16];
  size_t buffer_pos = 0;
  memset(sequence_buffer, 0x00, sizeof(sequence_buffer));
  unsigned int pending_bytes = 0;
  while(ioctl(STDIN_FILENO, FIONREAD, &pending_bytes) == 0 && pending_bytes > 0) {
    if(read(STDIN_FILENO, sequence_buffer + buffer_pos++, 1) == -1) {
      if(errno == EINTR) {
        buffer_pos--;
        continue;
      }
      fprintf(stderr, "Error reading from stdin\n");
    }
  }
  if(*sequence_buffer == '\0') {
    return NO_SEQUENCE;
  }
  // F5
  if(!memcmp(sequence_buffer, "[15~", 5)) {
    return EMULATOR_CONTINUE;
  }
  // F6
  if(!memcmp(sequence_buffer, "[17~", 5)) {
    return EMULATOR_SAVE_STATE;
  }
  // F7
  if(!memcmp(sequence_buffer, "[18~", 5)) {
    return EMULATOR_LOAD_STATE;
  }
  // F8
  if(!memcmp(sequence_buffer, "[19~", 5)) {
    return EMULATOR_RESET;
  }
  // F9
  if(!memcmp(sequence_buffer, "[20~", 5)) {
    return EMULATOR_BREAK;
  }
  // F10
  if(!memcmp(sequence_buffer, "[21~", 5)) {
    return EMULATOR_STEP_INSTRUCTION;
  }
  // F11
  if(!memcmp(sequence_buffer, "[23~", 5)) {
    return EMULATOR_STEP_CLOCK;
  }
  // F12
  if(!memcmp(sequence_buffer, "[24~", 5)) {
    return EMULATOR_PRINT_CYCLES;
  }
  return UNKNOWN_SEQUENCE;
}

void clear_screen() {
  // Clear screen
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // Move cursor to top
  write(STDOUT_FILENO, "\x1b[1;1H", 6);
}

void restore_term() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void *input_run(void* ptr) {
  volatile bool* stop = (bool*)ptr;
  char special_input;
  while(!(*stop)) {
    if(data_ready) {
      // Don't read until the CPU has consumed the previous one
      continue;
    }
    ssize_t bytes_read = read(STDIN_FILENO, &pressed_key, 1);
    if(bytes_read == -1) {
      if(errno == EINTR) {
        continue;
      }
      fprintf(stderr, "Error reading from stdin\n");
    } else if(bytes_read == 1) {
      switch(pressed_key) {
        case TILDE_KEY:
          clear_screen();
          continue;
        break;
        case TAB_KEY:
          process_emulator_input(EMULATOR_TURBO);
          continue;
        case ESC_KEY:
          special_input = read_escape_sequence();
          if(special_input) {
            process_emulator_input(special_input);
            continue;
          }
        break;
      }
      data_ready = true;
    }
  }
  fprintf(stderr, "Stopping input thread...\n");
  restore_term();
  pthread_exit(0);
}

void init_pia() {
  // Set RAW mode
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(restore_term);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
