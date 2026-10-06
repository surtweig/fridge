# Host tools: Fridge assembly → VHDL program images

These tools turn Fridge Assembly source into the VHDL packages that the
FPGA examples load into RAM (the boot image) and ROM (the ROM device image).
The examples' `FridgeRAMBootImage.vhd` / `FridgeROMImage.vhd` are **generated**
by them; do not hand-edit those files.

```
tools/
  build-falc.sh   build the assembler from fridge/falc into fridge/build-falc/
  falc            host wrapper for the assembler (builds it on first use)
  rom2vhd.py      pack binaries into FridgeROMImage.vhd
```

## Workflow

Write a program, then let its Makefile regenerate the image and build the
bitstream:

```asm
; examples/rom/src/rom_paint.falc
offset 0x0000
main start
    MVI A, 1          // A = 1 (TEXT)
    VMODE
    ...
    HLT
```

```console
$ tools/falc src/rom_paint.falc rom_paint.bin -vhdl-aggregate   # -> FridgeRAMBootImage.vhd
$ tools/rom2vhd.py --raw -o FridgeROMImage.vhd src/rom_image/*.hex
$ make          # ISE build
$ make load     # djtgcfg prog to the Atlys
```

`examples/rom/Makefile` wires this up: `make regen` rebuilds both images from
`src/`, and the generated VHDL files are checked in so a plain `make` needs no
extra step. On hardware the program is in the bitstream — "programming" it is
rebuild + `make load`, and it is gone at the next power cycle.

## falc — the assembler

`tools/falc <input.falc|.x2al> <output.bin> [-vhdl | -vhdl-aggregate]`

Built from the nested `fridge/falc` checkout (`spartan6-atlys`, local commit
`48a8703`; see `build-falc.sh`). The wrapper makes
input and output paths absolute and runs from inside `fridge/falc/`, because
falc resolves its standard include path (`../x2al_std/`) relative to the
current directory.

| Option | Output |
|---|---|
| *(none)* | flat machine-code binary |
| `-vhdl` | upstream's flat `X"NN", ... others => X"00"` literal list (`<output>.vhd`) |
| `-vhdl-aggregate` | complete `FridgeRAMBootImage` package, address-indexed aggregate with `FridgeIRCodes` symbolic names and source comments (`<output>.vhd`) |

`-vhdl-aggregate` writes opcode bytes as symbolic names (`16#00# => MVI_A,`),
operand and data bytes as `X"NN"`, and puts the source line above each
instruction as a comment. Comment-only lines of the source are reproduced in
place. Use it to generate the RAM boot image of an example.

Mnemonic notes:

- The assembler takes `IN` / `OUT`; the `FridgeIRCodes` names in generated
  VHDL are `IIN` / `IOUT` (same opcodes).
- Jump targets are declared with `entry name`; `subroutine name` / `endsub`
  is for CALL/RET code. See `fridge/doc/fridge-assembly-language.md`.
- `offset 0x0000` makes the program the reset image (it runs from PC = 0).
  With `main start` at the offset no jump prologue is emitted.

## rom2vhd.py — ROM images

`tools/rom2vhd.py [-o FridgeROMImage.vhd] [--toc | --raw] SECTION ...`

Packs section inputs into a ROM image and emits `FridgeROMImage.vhd`
(`ROM_SEGMENTS`, `ROM_BYTES`, `ROM_IMAGE_T`, `ROM_IMAGE`). Inputs are raw
binaries or hex dumps (`.hex`/`.txt`: whitespace-separated bytes, `#` comments,
an optional `ADDR:` column is ignored).

| Option | Layout |
|---|---|
| `--toc` *(default)* | appliance image: segment 0 is a TOC of 16-bit segment-start indices, then the sections zero-padded to whole segments |
| `--raw` | no TOC; sections are concatenated as raw segments (opaque payload) |

Use `--raw` when the ROM is data a program streams, `--toc` when the ROM is a
bootable image a boot loader will load (that path is not exercised on hardware
yet — the ROM device treats its contents as opaque).

`tools/rom2vhd.py --self-test` checks the packer round-trips (pack → emit →
read the VHDL back → compare).

## ABI constants

The constants the toolchain targets. Where the sources disagree, the FPGA
follows the **firmware trio** (`x2al_std/stdapp.inc`, `fridge-boot/BootLoader/
BootLoader.x2al`, `emulator/emulator/XCM2System.cpp`), because that is what
the hardware implements.

| Constant | Value | Note |
|---|---|---|
| ROM data device | `1` | `OUT`/`IN` this device for the 256-byte LOAD stream |
| ROM reset device | `2` | `OUT` value 1 resets the device to mode-select |
| Keyboard device | `3` | `IN` pops one key event |
| `EXECUTABLE_OFFSET` | `0x0100` | `stdapp.inc`. `fridge.h` says `0x0200`; only matters for the boot-loader path, which is deferred |
| ROM segment | 256 bytes | `FRIDGE_ROM_SEGMENT_SIZE` |
| Immediate order | high byte first | 16-bit operands are big-endian |
| Stack | descending | `SP` resets to `0xFFFF`; `FRIDGE_ASCENDING_STACK` is undefined in `fridge.h`, so falc emits no `LXI SP` prologue |

Known divergences from the upstream sources — recorded, not "fixed":

- **Device IDs are swapped** between `fridge.h`/`fridgemulib.c`
  (`RESET=1`, `ROM=2`) and the firmware trio (`ROM=1`, `RESET=2`). The FPGA
  examples and these tools use the firmware trio.
- **TOC endianness.** `emulator/rombuild/XCM2ROMImageBuilder.cpp` writes TOC
  entries in host (little-endian) order, while `BootLoader.x2al` reads each
  entry high byte first. `rom2vhd.py --toc` follows the boot loader.
- **ROM-ready interrupt.** The emulator raises an interrupt when a segment is
  ready; the Case B ROM streams with zero latency and no interrupt. Programs
  that `HLT` waiting for it cannot be used as-is.
- **`STORE` mode** is rejected by the appliance ROM device (read-only).

## Using falc programs on other examples

`examples/rom` is the wired-up example; `examples/cpu`, `examples/gpu` and
`examples/keyboard` still carry hand-assembled `FridgeRAMBootImage.vhd` files.
To migrate one, write the same program as `.falc` under `src/`, generate the
image with `-vhdl-aggregate`, and check it is byte-identical to the image it
replaces before switching the Makefile over.
