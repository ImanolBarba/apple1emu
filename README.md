# apple1emu

A mostly cycle-accurate Apple I emulator

I used [Klaus2m5 functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) to test the opcodes and it passes

Resources used for development:
- https://www.masswerk.at/6502/6502_instruction_set.html
- http://www.visual6502.org/JSSim/index.html
- http://archive.6502.org/books/mcs6500_family_programming_manual.pdf

ROMs are not provided

Usage
--
`./apple1emu -r <ROM_FILE> -e <EXTRA_FILE> -b <BINARY_FILE> -a <START_ADDR> -l <LOAD_ADDR>`

`EXTRA_FILE` is data to be loaded from `E000` to `EFFF`, normally the Apple Integer Basic program

For binary mode (running an arbitrary 6502 binary, not the Apple I rom), you can use:
- -b: To specify the binary to load
- -a: To specify what address the cpu will jump to
- -l: To specify at what RAM offset the binary will be loaded


