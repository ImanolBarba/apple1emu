#ifndef DEBUG_H
#define DEBUG_H

#include "m6502.h"

char* read_arg(char* input);
int parse_hex(char* addr_str, uint16_t* value);
int print_value(M6502* cpu, char* dest);
int set_value(M6502* cpu, char* dest, uint16_t value);

#endif
