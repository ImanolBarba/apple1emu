cmake_minimum_required(VERSION 3.10)
project(apple1emu)
set(CMAKE_C_STANDARD 11)

add_executable(apple1emu
        main.c mem.c m6502.c m6502_opcodes.c clock.c apple1.c pia6821.c debug.c
        mem.h m6502.h m6502_opcodes.h clock.h apple1.h pia6821.h debug.h)

target_link_libraries(apple1emu pthread)
