# Spartan-6 Atlys Porting Plan

## Agreed Target

- Board: Digilent Atlys revC, FPGA `xc6slx45-3-csg324`.
- Internal video: 240x160 pixels, scaled 4x to 960x640.
- Output: 1280x720 at 60 Hz, centered with 160-pixel horizontal and
  40-pixel vertical margins on each side.
- Start with BRAM for CPU RAM and video storage; defer DDR integration.
- BRAM budget: 116 RAMB16BWER total (2 KB each). The CPU/GPU builds use 64
  (32 for 64 KB CPU RAM + 32 for the two frame buffers, each in a 32 KB
  power-of-two window), leaving 52 free. Sprite memory will be limited to
  **32 KB** (16 RAMB16BWER) — a documented divergence from `fridge.h`'s
  64 KB `FRIDGE_GPU_SPRITE_MEMORY_SIZE` — so the rest stays available for
  the ROM device and future features.
- Keyboard: use the USB-host PIC32 stock PS/2 bridge, pending confirmation
  of the board hardware, firmware behavior, and signal connections.
- Flash (Case A, deferred): a read-only, protected data region sharing the
  configuration flash. Confirm the flash part and configuration/data layout
  before any erase. `djtgcfg` is not a flash writer.

## Repository Preparation

- Local clone: `/mnt/data/Projects/Spartan6Fridge/fridge` into
  `/mnt/data/Projects/Spartan6Fridge/Spartan6Toolchain/fridge`.
- Git history preserved; branch `spartan6-atlys` created from `master`
  at commit `e348131`.
- Source repository untouched; ignored build artifacts excluded.
- Port-specific assembler and standard-library changes are committed locally
  in the nested clone at `48a8703` on `spartan6-atlys` (VHDL aggregate emitter
  and `mul_dc` carry correction).

## Implementation Stages

1. **Standalone HDMI demo, complete:** implement an independent rectangle
   demo under `examples/hdmi` before cloning or integrating the CPU.
   Establish board constraints, clocks, reset, 720p raster timing, centered
   4x-scaled internal video, and TMDS output.
2. **Clone and CPU/RAM smoke test, complete:** repository preparation
   performed; CPU brought up with BRAM and a minimal observable test
   program. Simulation, synthesis, and timing all pass.
3. **HDMI/GPU integration, complete:** connect the initial framebuffer/GPU path to the
   validated HDMI pipeline, keeping the standalone demo as a reference.
4. **Keyboard, complete:** confirm the USB-host PIC32 PS/2 bridge on hardware, then add PS/2
   reception and a 32-entry keyboard FIFO with defined overflow behavior.
5. **ROM device (256-byte streams), split into two cases:**
   - **Case B — bitstream ROM, development, complete:** read-only ROM access
     via the emulator's segment-select + 256-byte LOAD stream
     (`fridgemulib.c`), backed by storage sized to the ROM image and
     initialized from the bitstream. "Programming" the ROM = rebuild +
     `djtgcfg prog`, gone at the next power cycle. No SPI flash is touched:
     safe by construction. Serves to lock the device contract, test the
     stream, and run demo ROMs (small images only: XST maps read-only arrays
     to LUT logic, so this is not a capacity path — capacity arrives with
     Case A).
    - **Case A — SPI flash, persistent (deferred):** the same ROM device
      backed by the configuration-flash region. Establish a safe programming
      procedure that protects configuration and reserved data; a UART
      programmer helper is an option, not yet selected. Confirm the flash
      part and configuration/data layout before any erase.
6. **Fridge assembly toolchain (falc), complete:** make `falc` a working part of the
   toolchain so programs can be written in Fridge Assembly, compiled on the
   host, and loaded into the design's RAM (boot image) or ROM (ROM image),
   instead of hand-assembling VHDL byte arrays. Build `fridge/falc` for Linux
   under `tools/`, extend it with an address-indexed VHDL aggregate emitter,
   and generate `FridgeRAMBootImage.vhd` / `FridgeROMImage.vhd` from source.
   Pin the ABI constants the toolchain targets (device IDs, executable
   offset, stack direction, big-endian immediates) and record the source
   divergences. Acceptance: recompile the Stage 5 ROM-paint boot image from
   `.falc` source byte-identically, and run the simulation and hardware gates
   against the generated images.
7. **Advanced GPU — TEXT and VPAL complete:** implement the text video mode that the
   Stage 3/4 GPU left as a stub (`VMODE` A=1 renders black). Contract taken
   from `fridgemulib.c` / `fridge.h` / the upstream DE0-CV adapter: a 40x20
   grid of 2-byte cells in the ordinary frame store at byte address
   `(row*40 + col)*2`, byte 0 = glyph index (raw 0..255), byte 1 =
   `(background << 4) | foreground` palette indices; 6x8 font, one byte per
   glyph column, MSB = top row; pixel color = foreground where the glyph bit
   is set, else background, through the same RGB888 palette EGA uses. The
   CPU writes cells with plain `VFSA` stores (`VFSI` pending; both use the
   same mode-agnostic ABI). Frame offsets (`VPRE` HL) are ignored in text scanout, matching the emulator.
   Deliverable: font ROM, text scanout in the GPU, a demo program written in
   Fridge Assembly through the Stage 6 toolchain, and the simulation +
   ISE/timing + hardware gates, all complete for TEXT. `VPAL` is implemented
   under `examples/palette` with an acknowledged RGB888 update shared by
   TEXT/EGA, invalid-index no-ops and a `.falc` animation demo; its hardware
   gate is complete. Remaining advanced-GPU work
   (`VFSI`/`VFSAC`/`VFLA`/`VFLAC`, then sprites) follows; sprite memory is
   limited to 32 KB on hardware (16 RAMB16BWER; see Agreed Target).

## Source Limitations

- The source has an incomplete video ABI and interrupt support; define and
  validate the required contracts before depending on them.
- Existing boot-image limitations must be checked against the BRAM smoke
  test and later flash-ROM boot path; do not assume those paths already work.

## Verification Gates

- Simulation: test CPU/RAM behavior, reset, interfaces, keyboard FIFO, and
  flash-stream handling independently of hardware results.
- Raster/TMDS: verify active area, porches, sync polarity, frame cadence,
  margins, 4x scaling, and TMDS encoding/serialization.
- Timing: inspect implementation constraints and timing reports separately;
  simulation or a generated bitstream does not establish timing closure.
- Hardware: confirm board revision, pin assignments, clocking, HDMI display,
  PIC32 bridge behavior, and flash part/layout on the actual board.
- Record each gate separately; do not infer hardware success from a build.

## Current Status

- Standalone demo implemented: centered 960x640 white rectangle on blue.
- ISE synthesis, place/route, bitstream generation and post-route timing pass
  with zero timing errors. Bitstream: `examples/hdmi/hdmi.bit`.
- Raster, encoder, serializer and clock-chain simulation tests pass;
  see `examples/hdmi/README.md` for coverage and commands.
- Hardware display verified on the Atlys: the 720p60 rectangle output is
  recognized and displayed by the connected monitor.
- Initial bring-up exposed an active-low reset on T15 that was incorrectly
  treated as active-high, holding the DCM in reset. The polarity was corrected;
  lock LEDs and frame activity provide board-level confirmation.
- Repository cloned into `Spartan6Toolchain/fridge`; branch `spartan6-atlys`
  created from `master` at commit `e348131`.
- CPU/RAM smoke test implemented under `examples/cpu`: FridgeCPU +
  FridgeRAM with BRAM, driven by a 26-byte test program exercising
  MVI, ADD, STA, LDA, CMP, JZ, JMP, HLT.
- Simulation passes: CPU halts at PC=0x001A after writing 0x9C to
  address 0x0080 and reading it back. See `examples/cpu/README.md`.
- ISE synthesis, place/route, bitstream generation and post-route timing
  pass with zero errors. Max frequency 19.4 MHz; CPU runs at 10 MHz.
  BRAM utilization: 32 RAMB16BWERs for 64 KB RAM.
  Bitstream: `examples/cpu/cpu_smoke.bit`.
- Hardware LED verification on the Atlys remains to be performed.
- HDMI/GPU integration implemented under `examples/gpu`: FridgeCPU +
  FridgeRAM + a new framebuffer/GPU (`fridge_gpu.vhd`) driving the validated
  720p TMDS pipeline. Internal video 240x160 at 4 bpp packed, scaled 4x into
  a centered 960x640 window with 160/40 px margins.
- Initial video ABI defined and validated (`examples/gpu/README.md`):
  VFSA stores to the active frame (0..19199, ignored out of range), VPRE
  (A = swap mode 0..5, HL = offsets) flips the active frame immediately and
  the displayed frame at the next vblank, VMODE A=0 EGA / A=1 TEXT (stub
  renders black). RGB888 palette per `fridge.h`; left pixel = high nibble.
  Reset state matches `FRIDGE_gpu_reset`. VPAL, VFSI/VFSAC/VFLA/VFLAC,
  sprites and text mode are reserved for later stages.
- Simulation passes: `tb_gpu` (full-frame pixel-exact scan of margins, 4x
  scaling, nibble order and palette; all six VPRE swap modes with vblank
  semantics; offset wrap; out-of-range writes ignored; VMODE stub) and
  `tb_system` (real boot image: 19200 verified VFSA writes, VMODE/VPRE
  traffic, halt at 0x0029, sampled display pixels, no early frame display).
  See `examples/gpu/README.md` for coverage and commands.
- ISE synthesis, place/route, bitstream and post-route timing pass with zero
  errors and all constraints met. Max pixel domain 11.9 ns against the
  13.47 ns requirement (74.25 MHz); CPU domain 68 ns against 100 ns.
  BRAM: 64 RAMB16BWER (32 CPU RAM + 32 frame buffers). The CPU-to-pixel CDC
  crossings are excluded from synchronous analysis (documented `TS_cdc`);
  the frame-buffer crossing is inside dual-clock BRAM ports.
- `examples/hdmi` and `examples/cpu` regressions pass unchanged.
- FridgeCPU fixes in `examples/gpu` (documented in its README): VPRE now
  forwards swap mode and offsets, VMODE decodes A bit 0 per `fridge.h` (see
  the correction below), and the scrambled INX/DCX register-pair dispatch
  (INX_HL wrote rE/rL) is corrected.
- VMODE decode correction (stage 4): `XCM2_WORD` is `unsigned(0 to 7)`, so
  A bit 0 is index 7; the stage 3 revision decoded `mode(0)` (the 0x80 bit),
  making A=1 select EGA against `fridge.h`. `examples/gpu` is corrected to
  `mode(7)` (bit indexing validated in simulation) and
  `examples/keyboard/tb_system` locks the A=0x01 -> TEXT decode through the
  CPU.
- Boot image draws 16 horizontal color bars via VFSA, presents with VPRE and
  halts.
- Hardware display verified on the Atlys: the monitor shows the 16 color bars
  filling the centered 960x640 window with 160/40 px blue margins, and the
  LED pattern matches (halted, heartbeat, clock locks, frame activity).
  Step 3 hardware gate recorded from the actual board.
- Stage 4 bridge situation confirmed from documentation (hardware gate still
  open): the "Host" USB-A port (J13) is served by a PIC24FJ192 (master UCF
  net names say "PIC32"; the Atlys manual revC sec. 11 names the part) that
  converts one USB keyboard/mouse to PS/2 protocol on four FPGA pins.
  Keyboard: K_CLK = P17, K_DAT = N15 (bank 1, 3.3 V); the master UCF's
  USBCLK/USBSDI SPI names are a stale earlier-revision mapping. No external
  pull-ups on the lines (UCF `PULLUP` required); PS/2 clock 10-16.7 kHz.
- PS/2 reception and the 32-entry keyboard FIFO implemented under
  `examples/keyboard`: `ps2_receiver.vhd` (11-bit frames, odd parity,
  start/stop checks, 150 us idle watchdog), `fridge_keyboard.vhd`
  (scan-code-set-2 decoder with E0/F0 prefixes, Pause swallow, Shift and
  CapsLock event-time folding per `keymap.h`, 32-entry FIFO), and a live
  key-event tape demo on the CPU/GPU/HDMI system reading events via `IIN 3`.
- Keyboard FIFO contract defined (README): `IIN 3` pops the oldest event,
  empty read returns 0x00 and is a no-op; on overflow the new event is
  dropped (buffered order preserved) and a sticky OVERFLOW flag rises until
  reset (LED3; CPU-visible status reserved). Documented divergences from the
  emulator: drop-newest instead of the emulator ring's silent overwrite,
  and full US Shift folding for punctuation.
- FridgeCPU device-bus fixes in `examples/keyboard` (README): DEVICE_DATA is
  now truly bidirectional (CPU drives only during IOUT), `ir_IIN` captures
  the resolved port value (it previously read an undriven internal signal),
  and DEVICE_READ is a one-cycle decode of CPU_DEVICE_READ instead of a
  level that latched until the next write. IOUT write timing is not
  validated (no write strobe yet); only the IIN path is.
- Simulation passes: `tb_ps2_receiver` (valid frames at 15/10 kHz, all 256
  bytes, parity/start/stop rejection, watchdog recovery), `tb_keyboard`
  (make/break, Shift/CapsLock/keypad/extended/Pause handling, empty-read
  semantics, device-select guard, 32-entry order, drop-newest overflow,
  sticky RX_ERROR) and `tb_system` (real boot image: 19200 fill writes,
  typed PS/2 traffic -> 6 tape events `E1 61 C2 42 B1 8A` in order, VMODE
  A-bit-0 TEXT->EGA decode, VPRE MANUAL_11, continuous IIN polling, tape
  pixel colors on display). See `examples/keyboard/README.md`.
- ISE synthesis, place/route, bitstream and post-route timing pass with zero
  errors and all constraints met. Max pixel domain 11.84 ns against the
  13.47 ns requirement (74.25 MHz); CPU domain 57 ns against 100 ns.
  BRAM: 64 RAMB16BWER (32 CPU RAM + 32 frame buffers).
  Bitstream: `examples/keyboard/keyboard.bit`.
- `examples/hdmi`, `examples/cpu` and `examples/gpu` regressions pass
  unchanged (the `examples/gpu` VMODE decode correction re-verified there).
- Stage 4 hardware gate recorded from the actual board: with the
  `examples/keyboard` bitstream, a USB keyboard on J13 (JP11 open) produces
  key-event tape pixels of different colors on the 720p display. PS/2
  reception, set-2 decoding, the FIFO and the `IIN 3` path are confirmed in
  hardware (letters, digits, numpad, space, enter, backspace).
- Deferred anomaly (stage 4 follow-up): on hardware the tape is silent for
  Esc, although Esc is mapped to 0x1B and its 0x9B press pixels (bright blue
  / bright cyan) would be plainly visible; and Up/Down/Left appear to produce
  events although all four arrows are E0-extended and unmapped (`tb_system`
  asserts the Up arrow yields no event). The silence of F-keys, Ctrl, Alt,
  Shift, Ins/Del/Home/End/PgUp/PgDn is by design (keymap parity). To be
  investigated later with a raw scan-code trace of what the PIC24 bridge
  actually emits for Esc and the arrow keys; see `examples/keyboard/README.md`.
- Stage 5 Case B implemented under `examples/rom`: the ROM device (segment
  select + 256-byte LOAD streams) on the CPU/GPU/HDMI system with the ROM
  image baked into the bitstream (`FridgeROMImage.vhd`, 4 demo segments),
  a ROM paint demo boot image, and a new CPU `DEVICE_WRITE` strobe that
  validates the IOUT device-write path (flagged unvalidated in stage 4).
- ROM device contract defined and validated (`examples/rom/README.md`):
  the device map follows the firmware trio (`x2al_std/stdapp.inc`,
  `BootLoader.x2al`, `XCM2System.cpp`: data = device 1, reset = device 2);
  the post-stream state returns to mode so the BootLoader can re-select
  segments without a device reset; STORE is rejected (read-only; identical
  behavior planned for Case A); out-of-range segments and protocol
  violations latch a sticky ERROR (LED3) with a defined no-op instead of
  the sources' corePanic/halt or silent-ignore; streaming is ready
  immediately. Documented divergences from `fridge.h`/`fridgemulib.c`
  (swapped device IDs; OPERATE re-loop + panic on re-select, which
  contradicts the BootLoader) and from the source's HLT + ROM-interrupt
  readiness handshake (CPU interrupt contracts still open per Source
  Limitations).
- Storage note: XST maps the read-only ROM array to LUT logic ("distributed
  Read Only RAM") and ignores `ram_style = block` for ROMs (verified with
  unstructured content); Case B images stay small (~250 LUTs for the 1 KB
  demo) and ROM capacity remains a Case A matter.
- Simulation passes: `tb_rom` (device contract: all four segments
  byte-exact against an independent pattern check, re-select without reset,
  device-select guard mid-stream, reads outside streaming, out-of-range
  segments, STORE/invalid-mode rejection, OUT during streaming, device
  reset command including value 0 and mid-stream resets, sticky ERROR, and
  the module RESET) and `tb_system` (real boot image: 13-write IOUT
  protocol traffic through the CPU, 1024-byte VFSA stream byte-exact,
  VMODE TEXT->EGA and VPRE MANUAL_11 traffic, halt, no ROM ERROR, and the
  on-screen pixel colors of all four segments plus untouched black and blue
  margins). See `examples/rom/README.md`.
- ISE synthesis, place/route, bitstream and post-route timing pass with zero
  errors and all constraints met. Max pixel domain 10.58 ns against the
  13.47 ns requirement (74.25 MHz); CPU domain 62 ns against 100 ns.
  BRAM: 64 RAMB16BWER (32 CPU RAM + 32 frame buffers; the ROM is
  LUT-mapped). Bitstream: `examples/rom/rom.bit`.
- `examples/hdmi`, `examples/cpu`, `examples/gpu` and `examples/keyboard`
  regressions pass unchanged.
- Stage 5 Case B hardware gate recorded from the actual board: with the
  `examples/rom` bitstream the monitor shows the four ROM paint patterns in
  the centered 960x640 window — the two nibble-cycling ramp bands (segments
  0/1, multicolor checkerboard), the green/magenta stripe band (segment 2,
  `AA 55`), the red/green stripe band (segment 3, `42` fill) — followed by
  untouched black frame and blue margins. LD0 high (halted) and LD3 low
  (no ROM ERROR). The IOUT-driven protocol, the 256-byte streams and the
  on-screen byte->pixel mapping are confirmed in hardware.
- Stage 5 Case A (SPI flash backend, flash-part/layout confirmation and the
  safe programming procedure) is deferred per the plan split above.
- Stage 6 implemented under `tools/` + `examples/rom/src`: `fridge/falc` builds
  for Linux (`tools/build-falc.sh`, `tools/falc` wrapper) and generates the
  examples' program images from Fridge Assembly source instead of
  hand-assembled VHDL aggregates. `tools/rom2vhd.py` packs binaries into the
  ROM image. `tools/README.md` documents the workflow, the ABI constants the
  toolchain targets, and the source divergences.
- falc build validated on Linux against the upstream goldens:
  `fridge_emulator/tests/hello.bin` (136 bytes) and `test_phase_b/c.bin`
  reproduce byte-for-byte from source, and `fridgemulib/tests/pamtest*.falc`
  compile. falc is built unmodified into `fridge/build-falc/` (upstream's own
  layout) apart from the additive `-vhdl-aggregate` emitter below.
- falc extended (in the `fridge` clone on `spartan6-atlys`, additive only —
  the existing `-vhdl` flat output is unchanged) with `-vhdl-aggregate`: a
  complete `FridgeRAMBootImage` package whose `RAMBootImage` is an
  address-indexed aggregate, opcode bytes as `FridgeIRCodes` symbolic names,
  operand/data bytes as `X"NN"`, and the source line above each instruction as
  a comment, with the source's comment-only lines reproduced in place. This is
  the toolchain's program-image path; `tools/rom2vhd.py` covers the ROM side
  (packing is a data-layout concern, not a compilation one).
- `examples/rom/src/rom_paint.falc` re-expresses the Stage 5 hand-assembled
  ROM-paint boot image in Fridge Assembly. The generated
  `FridgeRAMBootImage.vhd` is **byte-identical** to the hand-assembled image
  it replaces (48 bytes at 0x0000; verified against the `FridgeIRCodes`
  decode of the old aggregate and against the compiled `.bin`).
  `examples/rom/src/rom_image/seg0-3.hex` hold the four ROM demo patterns as
  reviewable hex dumps; `tools/rom2vhd.py --raw` packs them into
  `FridgeROMImage.vhd`, and the packed bytes reproduce the hand-built
  `rom_image_init` patterns exactly (independently re-implemented in
  `tb_rom`).
- `examples/rom/Makefile` generates both images from `src/` (`make regen`);
  the generated VHDL is checked in so a plain build needs no extra step.
- Simulation passes unchanged with the generated images: `tb_rom` (device
  contract against the packed ROM image) and `tb_system` (real boot image:
  13-write IOUT protocol traffic, 1024-byte VFSA stream byte-exact,
  VMODE/VPRE traffic, halt, no ROM ERROR, on-screen pixel colors of all four
  segments). See `examples/rom/README.md`.
- ISE synthesis, place/route, bitstream and post-route timing pass with zero
  errors and all constraints met. Pixel domain 10.58 ns against the 13.47 ns
  requirement (74.25 MHz); CPU domain 62 ns against 100 ns. BRAM: 64
  RAMB16BWER (32 CPU RAM + 32 frame buffers; the ROM is LUT-mapped).
  Bitstream: `examples/rom/rom.bit`.
- `examples/hdmi`, `examples/cpu`, `examples/gpu` and `examples/keyboard`
  regressions pass unchanged; those examples still carry hand-assembled boot
  images and can be migrated to `.falc` source incrementally (procedure in
  `tools/README.md`).
- ABI pinned in `tools/README.md`, following the firmware trio where the
  sources disagree: ROM data device 1, ROM reset device 2, keyboard device 3,
  `EXECUTABLE_OFFSET` 0x0100 (not `fridge.h`'s 0x0200), 256-byte ROM
  segments, big-endian 16-bit immediates, descending stack with SP reset
  0xFFFF (so falc emits no `LXI SP` prologue). Recorded divergences: swapped
  device IDs in `fridge.h`/`fridgemulib.c`; `rombuild`'s little-endian TOC
  entries against `BootLoader.x2al`'s high-byte-first reads (`rom2vhd.py`
  follows the boot loader); the ROM-ready interrupt and `STORE` mode notes
  carried over from Stage 5.
- Stage 6 hardware gate remains to be performed: load `examples/rom/rom.bit`
  and confirm the ROM paint demo still shows the four segments in the
  centered 960x640 window with the expected LED pattern.
- Stage 7 TEXT mode implemented under `examples/text`: the `VMODE` A=1 mode
  that the Stage 3/4 GPU rendered as a black stub is now a 40x20 character
  screen with a 6x8 raster font and per-cell foreground/background colours,
  on the same dual frame store EGA uses. Deliverable includes a demo
  program in Fridge Assembly built with the Stage 6 toolchain (`falc` +
  `x2al_std/vtext.inc`).
- TEXT contract taken from `fridgemulib.c`/`fridge.h`/the upstream DE0-CV
  adapter (`examples/text/README.md`): 2-byte cells at `(row*40 + col)*2`,
  byte 0 = glyph index (raw 0..255), byte 1 = `(background << 4) |
  foreground` palette indices; 6x8 glyphs, one byte per column with bit 7
  in the top row; pixel colour = foreground where the glyph bit is set, else
  background, through the same RGB888 palette EGA uses. `VFSA` writes
  cells exactly as in EGA (no separate text aperture); `VFSI` remains pending.
  Frame offsets (`VPRE`
  HL) are ignored in text scanout, matching the emulator. Power-on mode is
  TEXT, matching `FRIDGE_gpu_reset`.
- Font ROM: `FridgeRasterFont.vhd`, copied byte-identical from
  `fridge/fpga/fridge_graphics_de0cv/FridgeRasterFont.vhd` (256 glyphs x 6
  columns; verified equal to `FRIDGE_gpu_default_glyph_bitmap` in
  `fridge.h`). LUT-mapped (~192 LUTs).
- GPU frame store widened to a 16-bit word per read (even byte = glyph code,
  odd byte = attribute) so one read fetches both bytes of a cell; EGA still
  picks one byte and a nibble. The colour lookup is registered so the
  frame-store to TMDS-encoder path ends in `fridge_gpu` rather than inside
  the encoder — that is what closes timing with the added font-lookup logic.
  All outputs are delayed together, so the raster alignment of colour, sync
  and position is unchanged.
- Bringing up `vtext.inc` exposed two real bugs, both fixed (details in
  `examples/text/README.md`):
  - **CPU flag registers were aliases of `rF` bits** (`alias fCarry is
    rF(4)`). An alias of an array element is not usable as an `inout` signal
    parameter — a procedure such as `ir_RAL` reads a stale value instead of
    the bit, which silently broke `RAL`/`RAR` carry chaining and `ADC`/`ACI`.
    `vtext.inc` doubles cell addresses with `RAL`, so every address with a
    non-zero high byte came out truncated. The aliases were also at the
    wrong `fridge.h` positions (Carry aliased to `rF(4)` = 0x08, where
    `fridge.h` has Carry at 0x01). The flags are now ordinary signals,
    packed into `rF` only for `PUSH AF`/`POP AF` at the `fridge.h` bit
    positions (`packFlags`/`unpackFlags`, previously unused; `POP AF` gets
    its own unpacking path).
  - **`mul_dc` in `x2al_std/arithm.inc` did not carry `L`'s overflow into
    `H`** (`H = H + B` then `L = L + C` as independent byte adds), so
    `40 * 7 = 0x0118` came out as `0x0018`. It now adds the low byte first
    and the high byte with `ADC`. This is a shared-stdlib fix; the
    checked-in `fridge_emulator/tests/hello.bin` build artifact of
    `hello.falc` changes with it.
- Simulation passes: `tb_gpu` (the Stage 3/4 scan-out suite plus hand-decoded
  `'A'` glyph checks locking the font's bit order and column indexing, a
  full-frame pixel-exact TEXT scan against the reference model, TEXT
  ignoring `VPRE` offsets, and EGA intact after TEXT) and `tb_system` (real
  boot image: 136 VFSA cell writes at the expected addresses and data, one
  `VMODE` selecting TEXT, one `VPRE AUTO` with zero offset, and on-screen
  glyph/foreground/background pixels hand-decoded from the font). See
  `examples/text/README.md`.
- ISE synthesis, place/route, bitstream and post-route timing pass with
  zero errors and all constraints met. Pixel domain 12.37 ns against the
  13.47 ns requirement (74.25 MHz); CPU domain 49 ns against 100 ns.
  BRAM: 64 RAMB16BWER (32 CPU RAM + 32 frame buffers; the font is
  LUT-mapped). Bitstream: `examples/text/text.bit`.
- `examples/hdmi`, `examples/cpu`, `examples/gpu`, `examples/keyboard` and
  `examples/rom` regressions pass unchanged with the CPU flag fix applied to
  their `FridgeCPU.vhd` copies as well (the flag code was identical in all
  of them; their demos do not use `RAL`/`ADC`, so nothing else moved).
- Stage 7 TEXT hardware gate complete: the user tested
  `examples/text/text.bit` on the Atlys and confirmed the demo works
  (reported 2026-10-06). The TEXT portion of Step 7 is complete.

- Stage 7 programmable palette implementation under `examples/palette`:
  `VPAL` takes full-byte entry A (0..15) and RGB from B/C/D, preserves CPU
  registers/flags, and updates the same palette in TEXT and EGA. All invalid
  indices are ignored, correcting the emulator's eight-bit `3*A` wrap.
  The CPU waits for an acknowledged command-to-pixel mailbox transfer;
  held RGB/index data has a 20 ns settling constraint, while asynchronous
  control synchronizers have targeted timing exclusions.
- Palette demo generated from `.falc`: custom colours, TEXT swatches and
  EGA bars in separate framebuffers; live entry-15 colour animation and
  mode switching without further framebuffer writes. See
  `examples/palette/README.md` for the contract and verification commands.
- Palette ISE synthesis, place/route, bitstream generation and post-route
  timing pass: zero errors, all constraints met. Pixel domain 13.275 ns
  against 13.468 ns; CPU domain 47.846 ns against 100 ns. Held palette data
  paths max 5.604 ns against their explicit 20 ns constraint, with 1920
  paths checked. BRAM remains 64 RAMB16BWER; the palette is flip-flop based.
  Bitstream: `examples/palette/palette.bit`.
- Palette simulation passes: the inherited TEXT/EGA `tb_gpu` scanout
  regression; `tb_palette` (all entries, every invalid index, live RGB
  atomicity during an active row, asynchronous backpressure, held payloads
  and reset recovery); `tb_contract` (all 256 indices, unchanged CPU
  registers/flags with a slower pixel clock); and `tb_system` (real `.falc`
  demo: 156 TEXT + 19200 EGA byte stores checked individually, initial and
  animated VPAL traffic, TEXT swatches and all EGA bars).
- Stage 7 palette hardware gate complete: the user tested
  `examples/palette/palette.bit` on the Atlys and confirmed the demo works
  (reported 2026-10-06). TEXT and VPAL are complete; framebuffer access
  instructions and sprites remain.
