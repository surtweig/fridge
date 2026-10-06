# XCM2 Fridge

Fridge is an 8-bit computer based on an extended Intel 8080 instruction set with graphics acceleration. It includes an emulator, FPGA designs for Digilent Atlys (Spartan-6) and Terasic DE0-CV (Cyclone V), an assembly compiler, a simple language compiler and an IDE.

## Current progress
| Component | State |
| - | - |
| Win64 emulator | Done
| WebAssembly emulator | To do
| Spartan-6 Atlys VHDL | Active: TEXT and programmable palette verified on hardware
| DE0-CV VHDL | On hold
| Assembly compiler | Done
| Frion compiler | In progress
| Fridge IDE | On hold

## Spartan-6 Atlys target

The board build is part of this repository at
[`fpga/fridge_spartan6`](fpga/fridge_spartan6/README.md), on branch
`spartan6-atlys`. It uses ISE 14.7 in Docker and Digilent Adept for programming.
The CPU runs at 10 MHz, with 64 KB BRAM RAM, 240x160 graphics and 40x20 TEXT
mode scaled to 720p60 HDMI. ROM, keyboard and programmable palette demos are
included. The shared `falc/` assembler and `x2al_std/` library generate the
program images; no separate Fridge checkout is needed.

```bash
make -C fpga/fridge_spartan6/examples/palette regen
make -C fpga/fridge_spartan6/examples/palette test
make -C fpga/fridge_spartan6/examples/palette all timing
make -C fpga/fridge_spartan6/examples/palette load
```

See the [porting plan](fpga/fridge_spartan6/PORTING_PLAN.md) for the implemented
hardware contracts, verification gates and remaining GPU work.

## Block diagram
![Block diagram](https://github.com/surtweig/fridge/blob/master/doc/fridge-block.png?raw=true)

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

