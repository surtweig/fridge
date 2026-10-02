# Spartan-6 Atlys Porting Plan

## Agreed Target

- Board: Digilent Atlys revC, FPGA `xc6slx45-3-csg324`.
- Internal video: 240x160 pixels, scaled 4x to 960x640.
- Output: 1280x720 at 60 Hz, centered with 160-pixel horizontal and
  40-pixel vertical margins on each side.
- Start with BRAM for CPU RAM and video storage; defer DDR integration.
- Keyboard: use the USB-host PIC32 stock PS/2 bridge, pending confirmation
  of the board hardware, firmware behavior, and signal connections.
- Flash: a read-only, protected data region sharing the configuration flash.
  Confirm the flash part and configuration/data layout before any erase.
  `djtgcfg` is not a flash writer.

## Repository Preparation

- Local clone: `/mnt/data/Projects/Spartan6Fridge/fridge` into
  `/mnt/data/Projects/Spartan6Fridge/Spartan6Toolchain/fridge`.
- Git history preserved; branch `spartan6-atlys` created from `master`
  at commit `e348131`.
- Source repository untouched; ignored build artifacts excluded.

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
4. **Keyboard:** confirm the USB-host PIC32 PS/2 bridge on hardware, then add PS/2
   reception and a 32-entry keyboard FIFO with defined overflow behavior.
5. **Flash ROM:** implement read-only ROM access using 256-byte streams.
   Establish a safe programming procedure that protects configuration and
   reserved data; a UART programmer helper is an option, not yet selected.
6. **Advanced GPU:** add more complex GPU features after the basic CPU,
   display, keyboard, and ROM paths are verified.

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
- Stage 4 hardware gate remains open: the PIC24 bridge behavior on this
  board (PS/2 traffic on P17/N15 from a USB keyboard on J13, JP11 open) is
  to be recorded from the actual board with the `examples/keyboard`
  bitstream; documentation evidence is recorded above.
