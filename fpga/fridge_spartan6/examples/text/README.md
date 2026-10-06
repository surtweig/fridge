# Stage 7 — TEXT video mode

The GPU's `VMODE` A=1 mode, which the Stage 3/4 GPU rendered as a black
stub, is implemented here: a 40x20 character screen with a 6x8 raster font
and per-cell foreground/background colours. The demo is a Fridge Assembly
program built with the Stage 6 toolchain (`falc`) that prints four coloured
lines through the `x2al_std/vtext.inc` library.

Internal video is still 240x160 at 4x scale in a centered 960x640 window
with 160/40 px blue margins. TEXT and EGA share the same dual frame store.

## Picture

```
        ┌──────────────────────────────────────────────┐
        │                                              │
        │       SPARTAN-6 FRIDGE      <- white on blue │
        │                                              │
        │         TEXT VIDEO MODE     <- bright cyan   │
        │                                              │
        │   40x20 CELLS  6x8 FONT     <- bright green  │
        │                                              │
        │   FALC + VTEXT.INC          <- light gray    │
        │                                              │
        └──────────────────────────────────────────────┘
                       blue margins
```

`HLT` ends the demo (LD0 on).

## TEXT mode contract

Taken from `fridgemulib.c` (`FRIDGE_gpu_render_txt_rgb8`), `fridge.h` and
the upstream DE0-CV adapter (`fpga/fridge_graphics_de0cv/`):

| Item | Value |
|---|---|
| Grid | 40 columns x 20 rows |
| Cell | 2 bytes at frame byte address `(row*40 + col)*2` |
| Byte 0 | glyph index, raw 0..255 |
| Byte 1 | `(background << 4) \| foreground`, two 4-bit palette indices |
| Glyph | 6x8 pixels, one byte per column, **bit 7 is the top row** |
| Pixel | foreground where the glyph bit is set, else background |
| Palette | the same 16 RGB888 entries EGA uses (VPAL reserved) |
| Frame offsets | **ignored** in text scanout (matches the emulator) |

The CPU writes cells with ordinary `VFSA` stores; there is no separate
text aperture or mode check. `VFSI` uses the same aperture in the reference
ABI but remains unimplemented in this example. The
font is `FRIDGE_gpu_default_glyph_bitmap` from `fridge.h`, brought in as
`FridgeRasterFont.vhd` (byte-identical to the upstream DE0-CV font package).

Power-on mode is TEXT, matching `FRIDGE_gpu_reset`.

## Files

- `src/text_demo.falc` — the demo; `include "vtext.inc"` gives `vtext_putstr`.
  The boot image is generated from it (`make regen`); do not edit
  `FridgeRAMBootImage.vhd` (see `tools/README.md`).
- `FridgeRasterFont.vhd` — 256 glyphs x 6 columns of 8 vertical pixels,
  copied from the shared Fridge repository's
  `fpga/fridge_graphics_de0cv/FridgeRasterFont.vhd`.
- `fridge_gpu.vhd` — `../gpu/fridge_gpu.vhd` with the text scanout and the
  frame store widened to a 16-bit word per read (even byte = glyph code,
  odd byte = attribute), so one read fetches both bytes of a cell. The
  colour lookup is registered: the frame-store to TMDS-encoder path ends in
  `fridge_gpu` instead of inside the encoder (this is what closes timing
  with the extra font-lookup logic). All outputs are delayed together, so
  the raster alignment of colour, sync and position is unchanged.
- `FridgeCPU.vhd` — `../gpu/FridgeCPU.vhd`, identical (the flag storage fix
  below has been applied to every example's copy).

## Bugs found and fixed

Bringing up `vtext.inc` exposed two real bugs. Both are fixed; the first is
a CPU bug that has been carried back to every example's `FridgeCPU.vhd`.

1. **Flag registers were aliases of `rF` bits** (`alias fCarry : std_logic is
   rF(4)`). An alias of an array element cannot be used as an `inout` signal
   parameter: a procedure such as `ir_RAL` then reads a **stale** value
   instead of the bit. That silently broke `RAL`/`RAR` carry chaining and
   `ADC`/`ACI` — `vtext.inc` doubles cell addresses with `RAL` and shifts
   the background colour into the high nibble with four `RAL`s, so every
   address with a non-zero high byte came out truncated. The aliases were
   also at the wrong `fridge.h` positions (`fCarry` at `rF(4)` = 0x08, where
   `fridge.h` has Carry at 0x01). The flags are now ordinary signals and are
   packed into `rF` only for `PUSH AF`/`POP AF`, at the `fridge.h` bit
   positions (`packFlags`/`unpackFlags`, previously unused).

2. **`mul_dc` in `x2al_std/arithm.inc` did not carry `L`'s overflow into
   `H`.** It computed `H = H + B` then `L = L + C` as two independent byte
   adds, so `HL + BC` was only correct when `L + C` did not wrap — e.g.
   `40 * 7 = 280 = 0x0118` came out as `0x0018`, putting row 7 in the wrong
   place. It now adds the low byte first and the high byte with `ADC`.
   `fridge_emulator/tests/hello.bin` (a checked-in build artifact of
   `hello.falc`, which includes `arithm.inc`) changes accordingly;
   `run_hello.sh` regenerates it.

A third issue was a demo bug, not a platform one: `VPRE` takes its frame
offsets from HL, and `vtext_putstr` leaves HL at the last cell address, so
the demo has to zero HL before presenting.

## Commands

From the toolchain root:

```sh
make -C examples/text regen    # rebuild the boot image from src/
make -C examples/text
make -C examples/text test
make -C examples/text timing
make -C examples/text load
```

`load` programs FPGA SRAM only; it does not write flash.

LEDs (LD5/LD7 share configuration pins and are unused):

| LED | Meaning |
|-----|---------|
| LD0 | CPU halted (the demo prints four lines and halts: LED on) |
| LD1 | ~2 Hz heartbeat |
| LD2 | all four clocks locked |
| LD3 | unused (low) |
| LD4, LD5 | unused (low) |

The RESET button on T15 is active low.

## Verification

- `tb_gpu.vhd`: the Stage 3/4 framebuffer/scan-out suite (margins, 4x
  scaling, nibble order, palette, raster, all six `VPRE` swap modes with
  vblank semantics, offset wrap, out-of-range writes) plus
  - hand-decoded glyph checks for `'A'` (`7E A0 A0 A0 7E 00`) locking the
    font's bit order and column indexing independently of the reference
    model,
  - a full-frame pixel-exact scan of TEXT mode against the model (cell
    addressing, attribute nibbles, glyph bits, palette),
  - that TEXT scanout ignores the `VPRE` frame offsets,
  - and that EGA is intact after returning from TEXT.
- `tb_system.vhd`: the real boot image through CPU + RAM + GPU + HDMI —
  136 VFSA cell writes at the expected addresses and data (the title's
  first pair is cell (3,2) = glyph `'S'` 0x53 then attribute 0x1F), one
  `VMODE` selecting TEXT, one `VPRE AUTO` with zero offset, and on-screen
  pixels: blue margins, black untouched cells, and glyph/foreground/
  background colours of the title and the info line, hand-decoded from the
  font.
- ISE synthesis, place/route, bitstream and post-route timing pass with
  zero errors and all constraints met. Pixel domain 12.37 ns against the
  13.47 ns requirement (74.25 MHz); CPU domain 49 ns against 100 ns.
  BRAM: 64 RAMB16BWER (32 CPU RAM + 32 frame buffers; the font is
  LUT-mapped). Bitstream: `examples/text/text.bit`.
- `examples/hdmi`, `examples/cpu`, `examples/gpu`, `examples/keyboard` and
  `examples/rom` regressions pass unchanged.

- Hardware gate complete: the user tested this demo on the Atlys and
  confirmed it works (reported 2026-10-06).

The next Step 7 milestone is the programmable palette in
[`../palette/`](../palette/README.md). Framebuffer instructions
`VFSI`/`VFSAC`/`VFLA`/`VFLAC` and sprites follow; see PORTING_PLAN.md.
