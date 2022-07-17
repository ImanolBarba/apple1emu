#include "m6502.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

// Read argument from input. Skips the first token and returns the address where the second
// token begins, or NULL. This function can be chained: you can obtain the 2nd argument by
// calling it passing the pointer returned for the first argument as input
char* read_arg(char* input) {
  while(*(input++) != ' ') {
    if(*input == '\0' || *input == '\n') {
      // We reached the end, there's nothing here anymore, bail out
      return NULL;
    }
  }
  // We skipped the first token, now seek the second one
  while(*(input++) == ' ') {
    if(*input == '\0' || *input == '\n') {
      // We reached the end, there's nothing here anymore, bail out
      return NULL;
    }
  }
  return input-1;
}

int parse_hex(char* addr_str, uint16_t* value) {
  char* endptr = NULL;
  *value = (uint16_t)strtoll(addr_str, &endptr, 16);
  if(*endptr != '\0') {
    return FAILURE;
  }
  return SUCCESS;
}



int print_value(M6502* cpu, char* dest) {
  uint16_t addr;
  uint16_t value;
  int ret = parse_hex(dest, &addr);
  if(ret != FAILURE) {
    // This is a mem address
    if(addr == 0xA && !strncmp(dest, "A", 1)) {
      // I hate this stupid edge case, if the address is 0x000A, and the 
      // destination is only 'A', this is supposed to be the accumulator
      value = (uint8_t)cpu->A;
    } else {
      cpu->enabled = false;
      while(cpu->active) {
        // spin
      }

      // Preserve bus and RW
      uint16_t prev_addr = *cpu->addr_bus;
      uint8_t prev_data = *cpu->data_bus;

      *cpu->addr_bus = addr;

      clock_cpu((void*)cpu, true); 
      value = (uint8_t)*cpu->data_bus;

      // Restore bus
      *cpu->data_bus = prev_data;
      *cpu->addr_bus = prev_addr;

      cpu->enabled = true;
    }
  } else {
    // Not an address
    if(!strncmp(dest, "PC", 2)) {
      value = cpu->PC;
    } else if(!strncmp(dest, "X", 1)) {
      value = (uint8_t)cpu->X;
    } else if(!strncmp(dest, "Y", 1)) {
      value = (uint8_t)cpu->Y;
    } else if(!strncmp(dest, "S", 1)) {
      value = (uint8_t)cpu->S;
    } else {
      return FAILURE;
    }
  }
  
  printf("%s: %04X\n", dest, value);
  return SUCCESS;
}

int set_value(M6502* cpu, char* dest, uint16_t value) {
  uint16_t addr;
  dest[strcspn(dest, " ")] = '\0';
  int ret = parse_hex(dest, &addr);
  if(ret != FAILURE) {
    // This is a mem address
    if(addr == 0xA && !strncmp(dest, "A", 1)) {
      // I hate this stupid edge case, if the address is 0x000A, and the 
      // destination is only 'A', this is supposed to be the accumulator
      cpu->A = (uint8_t)value;
      return SUCCESS;
    }

    cpu->enabled = false;
    while(cpu->active) {
      // spin
    }

    // Preserve bus and RW
    uint16_t prev_addr = *cpu->addr_bus;
    uint8_t prev_data = *cpu->data_bus;
    bool prev_RW = cpu->RW;

    *cpu->addr_bus = addr;
    *cpu->data_bus = (uint8_t)value;
    cpu->RW = false;

    clock_cpu((void*)cpu, true); 

    // Restore bus
    *cpu->data_bus = prev_data;
    *cpu->addr_bus = prev_addr;
    cpu->RW = prev_RW;

    cpu->enabled = true;
    return SUCCESS;
  }
  // Not an address
  if(!strncmp(dest, "PC", 2)) {
    cpu->PC = value;
    return SUCCESS;
  } else if(!strncmp(dest, "X", 1)) {
    cpu->X = (uint8_t)value;
    return SUCCESS;
  } else if(!strncmp(dest, "Y", 1)) {
    cpu->Y = (uint8_t)value;
    return SUCCESS;
  } else if(!strncmp(dest, "S", 1)) {
    cpu->S = (uint8_t)value;
    return SUCCESS;
  }
  return FAILURE;
}
