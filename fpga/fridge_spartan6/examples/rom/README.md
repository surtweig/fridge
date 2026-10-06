# Atlys ROM Device (Case B): BRAM-Backed Read-Only 256-Byte Streams

Adds the Fridge ROM device to the validated CPU/GPU/HDMI system (`../gpu`),
implementing the emulator's ROM segment protocol (segment select + 256-byte
LOAD streams) with the ROM image baked into the bitstream. Digilent Atlys
revision C (`xc6slx45-3-csg324`).

This is **Stage 5 Case B** of PORTING_PLAN.md: the development path ("program
the ROM over JTAG", gone at power cycle). Case A — the same device backed by
the configuration SPI flash, persistent across power cycles — is deferred.
No SPI flash is touched by this design: safe by construction.

## Picture

The boot image is a **ROM paint demo**. It clears nothing (frame BRAMs reset
to black), presents frame 1 with `VPRE MANUAL_11`, then walks ROM segments
0..3 with the device protocol and paints each ROM byte as one framebuffer
byte (two 4bpp pixels) at the frame pointer, 4 * 256 = 1024 bytes at frame
bytes 0..1023. The demo ROM image (`FridgeROMImage.vhd`) is four pattern
segments — ascending ramp, descending ramp, `AA 55...`, `42` fill — so the
screen shows a gray ramp, its mirror, an alternating green/magenta stripe
field and a red/green checkerboard stripe field in the top rows of the
centered window; the rest of the frame stays black and the margins blue.
`HLT` ends the demo (LD0).

The hardware gate (see PORTING_PLAN.md) is recorded from the actual board
with this bitstream: the monitor shows the ramp bands (multicolor
checkerboard from the nibble-cycling ramp bytes), the green/magenta `AA 55`
stripe band and the red/green `42` stripe band in the top rows of the
centered window, then untouched black frame and blue margins; LD0 high
(halted), LD3 low (no ROM ERROR).

## ROM device contract

Device map (firmware-facing, matching `x2al_std/stdapp.inc`,
`fridge-boot/BootLoader/BootLoader.x2al` and the old emulator's
`emulator/emulator/XCM2System.cpp`):

| Device | Direction | Meaning |
|--------|-----------|---------|
| `1` (`XCM2_ROM_DEVICE_ID`) | `IOUT` | mode select (2 = LOAD), then segment hi, segment lo |
| `1` | `IIN` | stream bytes while streaming (256 per selection) |
| `2` (`XCM2_ROM_DEVICE_RESET_ID`) | `IOUT` | value > 0 resets the device; 0 is ignored |

State protocol (the `XCM2ROM.cpp` semantics the BootLoader depends on):

| State | `IOUT` on device 1 | `IIN` on device 1 |
|-------|--------------------|-------------------|
| mode | 2 (LOAD) -> segment-hi state; anything else (incl. STORE = 1) -> sticky ERROR, stay | 0x00, sticky ERROR |
| segment-hi | store byte -> segment-lo state | 0x00, sticky ERROR |
| segment-lo | store byte; in range -> streaming (stream pos 0); out of range -> sticky ERROR, back to mode | 0x00, sticky ERROR |
| streaming | sticky ERROR (read-only device; ignored) | stream byte at the current position; after byte 256 back to **mode** state (a new segment can be selected without a device reset, as the BootLoader does) |

- **Ready immediately**: the first `IIN` after the segment-lo write returns
  stream byte 0. Reads of other device IDs neither drive the bus nor advance
  the stream.
- `ERROR` is sticky until reset (the `RESET` input or the device reset
  command) and is shown on LED3.

### Documented divergences from the source

The source has three ROM implementations that disagree; this device follows
the pair that firmware actually uses (`XCM2ROM.cpp` + `BootLoader.x2al`) and
records the rest here.

1. **Device IDs.** `fridge.h` / `fridgemulib.c` swap the two IDs
   (`FRIDGE_DEV_ROM_RESET_ID` = 0x01, `FRIDGE_DEV_ROM_ID` = 0x02). This
   device follows `stdapp.inc` / `XCM2System.cpp` (data = 1, reset = 2),
   which is what `BootLoader.x2al` addresses.
2. **Post-stream state.** `fridgemulib.c` returns to OPERATE and its tick
   re-enters STREAMING (re-reading the same segment); a new segment select
   from that state corePanics. The BootLoader re-selects segments without a
   reset, so this device returns to the mode state like `XCM2ROM.cpp`.
3. **Readiness.** Both source ROMs raise a ROM interrupt on the
   operate->streaming transition and the BootLoader `HLT`s waiting for it.
   The CPU's interrupt contracts are not defined yet (plan: source interrupt
   support is incomplete), so this device streams with no latency and no
   interrupt. Case A (flash prefetch) will need a defined latency or a
   status read here.
4. **STORE mode.** Both source ROMs accept STORE (RAM-backed). The appliance
   ROM is read-only — Case A flash is written only by the host-side
   programmer — so STORE is rejected at mode select with sticky ERROR.
   Rejected STORE behaves the same in both cases, so firmware developed on
   Case B predicts Case A.
5. **Errors.** The sources either corePanic and halt the CPU
   (`fridgemulib.c`) or silently ignore / return stale data (`XCM2ROM.cpp`,
   which also has no out-of-range check). This device latches the sticky
   ERROR flag and performs a defined no-op instead (reads return 0x00
   without advancing).

## CPU change vs `examples/keyboard`

`DEVICE_WRITE` is a new CPU output: a pure decode of the FSM state, high for
exactly the one `CPU_DEVICE_WRITE` cycle (the symmetric strobe to
`DEVICE_READ`). Devices latch the write value (`rA` on `DEVICE_DATA`) at the
rising edge that ends the state. The `examples/keyboard` README recorded the
IOUT device-write timing as unvalidated; this stage exercises it end to end
(`tb_system` checks the full 13-write protocol sequence through the CPU).

## Storage

The ROM image is `FridgeROMImage.vhd`, generated by `tools/rom2vhd.py --raw`
from the four hex dumps in `src/rom_image/` (the appliance `.rom` layout with
its segment-0 TOC is what `--toc` produces; the device treats its contents as
opaque either way). XST maps
this read-only array to **LUT logic** ("distributed Read Only RAM") and
ignores `ram_style = block` for ROMs — verified with unstructured content
(a 256-byte random table still infers distributed RAM; the demo's regular
patterns additionally constant-fold). Small development images are fine
this way (1 KB demo: ~250 LUTs); large ROM images belong to Case A (flash).

## Commands

From the toolchain root:

```sh
make -C examples/rom regen    # rebuild both images from src/
make -C examples/rom
make -C examples/rom test
make -C examples/rom timing
make -C examples/rom load
```

`load` programs FPGA SRAM only (including the ROM image — this is the Case B
"programming" path); it does not write flash. The generated image is
`examples/rom/rom.bit`.

LEDs (LD5/LD7 share configuration pins and are unused):

| LED | Meaning |
|-----|---------|
| LD0 | CPU halted (the demo paints 1024 bytes and halts: LED on) |
| LD1 | ~2 Hz heartbeat |
| LD2 | all four clocks locked |
| LD3 | ROM device **ERROR** (sticky until reset) |
| LD4, LD5 | unused (low) |

The RESET button on T15 is active low.

## Implementation

- `fridge_rom.vhd` — the ROM device: the state machine above, the
  image-sized storage (a registered read address + the array, `FridgeRAM`
  pattern) and the `IIN 1` / `IOUT 1` / `IOUT 2` device interface
  (combinational show-ahead read data; one pop per `DEVICE_READ`; one latch
  per `DEVICE_WRITE`).
- `FridgeROMImage.vhd` — the demo ROM image (4 segments, 1 KB) and its size
  (`ROM_SEGMENTS`); the device bounds-checks segment selects against it.
  Generated by `tools/rom2vhd.py --raw` from `src/rom_image/seg0-3.hex`;
  do not edit (see `tools/README.md`).
- `FridgeRAMBootImage.vhd` — the ROM paint demo: VMODE
  TEXT then EGA, `VPRE MANUAL_11`, ROM reset command, then the segment loop
  (LOAD, hi, lo, 256 x `IIN 1` / `VFSA`) for segments 0..3, `HLT`.
  Generated by `tools/falc ... -vhdl-aggregate` from `src/rom_paint.falc`;
  do not edit. The generated bytes are identical to the hand-assembled
  image this example originally shipped with.
- `rom.vhd` — `../gpu/gpu.vhd` clock chain and CPU/GPU/TMDS path plus the
  ROM instance wired to the CPU device bus and the 4-LED status.
- `FridgeCPU.vhd` — copy of `../keyboard` with the `DEVICE_WRITE` strobe
  added; the GPU/TMDS/RAM sources are unchanged copies of `../keyboard`
  (the Docker harness mounts only the example directory).

## Verification

- `tb_rom.vhd` (device contract, PS/2-free bus-level): all four segments
  byte-exact (independent pattern check); re-select without a device reset
  (stream end -> mode state); mid-stream device-select guard (no bus drive,
  no pop, no error); reads outside streaming return 0x00; out-of-range
  segments (ERROR, no stream); STORE and invalid modes rejected; OUT during
  streaming (ERROR, stream position unaffected); device reset command
  (clears ERROR, mid-stream, value 0 ignored); sticky ERROR across good
  streams; module RESET clears ERROR.
- `tb_system.vhd` (CPU + RAM + GPU + ROM): runs the real boot image;
  verifies the IOUT-driven protocol traffic through the CPU (1 reset
  command + 12 writes: LOAD/hi/lo for segments 0..3), the 1024-byte VFSA
  stream byte-exact in order, exactly two VMODE (TEXT then EGA, A-bit-0
  decode) and one `VPRE MANUAL_11`, halt, no ERROR, and the on-screen pixel
  colors of all four segments plus the untouched black area and blue
  margins.
- ISE build: synthesis, map, place/route, bitstream (0 DRC errors) and
  post-route timing with **zero errors, all constraints met**. Resources:
  522 registers, 8926 LUTs (32%), 3004 slices (44%), 64 RAMB16BWER (32 CPU
  RAM + 32 frame buffers; the ROM is LUT-mapped, see Storage). Tightest
  constrained path: 10.58 ns against the 13.47 ns pixel period (74.25 MHz);
  the CPU domain closes at 62 ns against 100 ns.
- Regressions: `make -C examples/hdmi test`, `make -C examples/cpu test`,
  `make -C examples/gpu test` and `make -C examples/keyboard test` pass
  unchanged.
