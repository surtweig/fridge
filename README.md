# XCM2 Fridge

Fridge is a 8-bit computer based on extended Intel 8080 instruction set with graphics acceleration. Current implementation consists of an emulator (Windows x64 + DirectX 11) and VHDL design for FPGA development board Terasic DE0-CV (Altera Cyclone V), as well as various tools such as - assembly compiler, custom simplistic language compiler and an IDE.

## Current progress
| Component | State |
| - | - |
| Win64 emulator | Almost (no sprites acceleration)
| VHDL design | On hold
| Assembly compiler | Done
| Freon compiler | In progress
| Fridge IDE | On hold

## System specs

### CPU
* Modified Big-Endian Intel 8080
* Graphical instructions
* 10 MHz clock frequency

### RAM
* 64 KB (16-bit address)

### Video
* Display: 240x160 pixels
* 4-bit pallette (16 colors) from 4096 possible colors
* Two framebuffers 240x160x4
* 64 KB sprite memory (102 KB total video memory)
* 40x20 ASCII text mode (6x8 font)

### ROM
* SD card (16 MB maximum)

