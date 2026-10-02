# Atlys CPU/GPU/HDMI Integration

Connects the initial Fridge framebuffer/GPU path to the validated 720p TMDS
pipeline, driven by the Fridge CPU through the video ABI. Digilent Atlys
revision C (`xc6slx45-3-csg324`). The standalone rectangle demo in
`examples/hdmi` stays untouched as the raster/TMDS reference.

## Picture

- Output: 1280x720 at 60 Hz, progressive, positive sync — same raster as the
  standalone demo (`../hdmi/video_pattern.vhd` is the raster source of truth).
- Internal video: 240x160 pixels, 4 bpp packed two pixels per byte, scaled
  4x to a 960x640 window at x=160..1119, y=40..679.
- The boot image fills the framebuffer with **16 horizontal color bars** (10
  internal rows each, byte `i*0x11` for bar i) via `VFSA`, then presents the
  frame with `VPRE` and halts. On screen: bars 0..15 top to bottom
  (black, blue, green, cyan, red, magenta, brown, light gray, dark gray,
  bright blue, bright green, bright cyan, bright red, bright magenta,
  bright yellow, white), with 160 px blue (`00 00 FF`) margins left/right and
  40 px top/bottom.

## Video ABI (initial contract)

Palette entries are RGB888 from `FRIDGE_gpu_default_palette` in `fridge.h`.
Frame byte layout follows `FRIDGE_GPU_WORD`: left pixel = high nibble.

| Op | Semantics |
|----|-----------|
| `VFSA` (0xEC) | Store A at `active_frame[HL]`, byte address 0..19199. Out-of-range writes are ignored (the emulator panics; hardware cannot — documented divergence). |
| `VPRE` (0xE9) | A = swap mode 0..5 (`SWAP_NONE/AUTO/MANUAL_00/01/10/11` per `fridge.h`), HL = frame offsets (H hor, L ver). The active frame (VFSA target) flips immediately; the displayed frame switches at the **next vblank** (tear-free; documented divergence from the emulator's immediate switch). Offsets apply at the same vblank, wrapped mod 240/160 (reduced mod at set time like the emulator). |
| `VMODE` (0xEA) | A=0 selects EGA (implemented); A=1 selects TEXT, which is reserved and renders black in the window until the advanced GPU stage. Only A in {0,1} is defined. |

Reset state (matches `FRIDGE_gpu_reset`): visible=frame0, active=frame1,
mode=TEXT, offsets=0, palette=defaults. Reserved for later stages (not
implemented here): `VPAL`, `VFSI`, `VFSAC`, `VFLA`, `VFLAC`, `VS2F`, the
`VMEM`/sprite interface, `GPU_BACK_LOAD`, `GPU_PALETTE_SWITCH`.

## Commands

From the toolchain root:

```sh
make -C examples/gpu
make -C examples/gpu test
make -C examples/gpu timing
make -C examples/gpu load
```

`load` programs FPGA SRAM only; it does not write flash. The generated image
is `examples/gpu/gpu.bit`. LED0 = CPU halted (pass), LED1 = ~2 Hz heartbeat,
LED2 = all four clocks locked, LED3 = frame activity. The RESET button on T15
is active low.

## Implementation

- `video_timing.vhd` — 1650x750 raster copied from `../hdmi/video_pattern.vhd`.
- `fridge_gpu.vhd` — two 19200-byte frame buffers in dual-clock block RAM
  (write on the CPU clock, read on the pixel clock), RGB888 palette ROM, 4x
  scaling with offsets, VPRE swap logic, VMODE stub. Scan-out is a two-stage
  pipeline: raster to frame coordinates (registered), then byte address from
  the registered coordinates using the stride 120 = 128 - 8 so the multiply
  stays in carry chains (a `py*120` multiply pulled in a DSP48 and broke
  74.25 MHz). RGB, sync and the `PIXEL_X/PIXEL_Y` debug outputs share one
  latency, so the testbenches are latency-independent.
- `gpu.vhd` — standalone demo's clock chain (DCM 99/100, PLL 15/2, 742.5 MHz
  VCO, BUFPLL) plus a second DCM_CLKGEN (2/20) for the 10 MHz CPU clock;
  FridgeCPU + FridgeRAM + fridge_gpu + the shared TMDS encoder/serializer.
- `FridgeCPU.vhd` — copy of `examples/cpu` with three documented fixes (see
  below); the TMDS sources are unchanged copies of `../hdmi`'s (the Docker
  harness mounts only the example directory, so cross-directory references
  are not visible to the tools).

### CPU changes vs `examples/cpu`

1. `ir_VPRE` now latches the swap mode (A) and frame offsets (HL) into new
   outputs `GPU_PRESENT_MODE`/`GPU_FRAME_OFFSET` alongside the trigger pulse;
   previously VPRE only pulsed a trigger and dropped A/HL.
2. `ir_VMODE` decodes `mode(7)` per `fridge.h` (A=0 EGA, A=1 TEXT): `XCM2_WORD`
   is `unsigned(0 to 7)`, so index 7 is the A=0x01 bit and index 0 is the
   0x80 bit. An earlier revision of this file decoded `mode(0)` (inverted);
   it was corrected after the bit indexing was validated in simulation
   (`examples/keyboard/tb_system` now locks the A=0x01 -> TEXT decode).
3. `INX_DE`/`INX_HL`/`DCX_DE`/`DCX_HL` dispatched to scrambled register pairs
   (`rD,rH` and `rE,rL` instead of `rD,rE` and `rH,rL`) — an upstream bug
   breaking HL-pointer loops; fixed to match `fridge.h`.

## Verification

- `tb_gpu.vhd` (GPU unit, single clock): full 1650x750 raster sweep with
  per-pixel checks of sync windows, margins, 4x scaling block boundaries,
  nibble order and all 16 RGB888 palette entries; reset state; both frames
  independent; all six VPRE swap modes with the display switch deferred to
  vblank; frame offsets with wrap; out-of-range writes ignored; VMODE TEXT
  stub and return to EGA.
- `tb_system.vhd` (CPU + RAM + GPU, 10 MHz + 74.25 MHz): runs the real boot
  image; verifies the VFSA stream (all 19200 writes, in order, with expected
  bar data), VMODE/VPRE traffic, halt at PC=0x0029; then checks sampled
  display pixels (bar colors, window edges, blue margins) and that the new
  frame is never shown before the VPRE request.
- TMDS encoding/serialization stays proven by `../hdmi`'s tests (the encoder
  and serializer sources are unchanged); `make -C examples/hdmi test` and
  `make -C examples/cpu test` pass as regressions.
- ISE build: synthesis, map, place/route, bitstream (0 DRC errors) and
  post-route timing with **zero errors, all constraints met**. Resources:
  494 registers, 8859 LUTs (32%), 3073 slices (45%), 64 RAMB16BWER (55%:
  32 CPU RAM + 32 frame buffers), 2 DCM_CLKGEN, 1 PLL_ADV, 4 BUFG.
  Tightest constrained path: 11.9 ns against the 13.47 ns pixel period
  (74.25 MHz); the CPU domain closes at 68 ns against 100 ns.

### Constraints

The UCF derives all period constraints from the 100 MHz input as in the
standalone demo. `TS_cdc` marks the CPU-to-pixel clock-domain crossings
false paths: those nets are the two-stage synchronizer inputs and the
request-captured pending registers in `fridge_gpu` (change in the CPU domain,
sampled in the pixel domain only after they are stable); the frame buffer
crossing is entirely inside dual-clock block RAM ports and has no fabric
path. The 742.5 MHz serial constraint has no fabric paths (dedicated I/O
serialization hardware), same as the standalone demo.

## Hardware

Verified on the Atlys: `make load` shows the 16 color bars filling the
centered 960x640 window with 160/40 px blue margins; LED0 goes high after
the fill (CPU halted), LED1 blinks at ~2 Hz, LED2 is high (clocks locked),
LED3 blinks with frame activity.
