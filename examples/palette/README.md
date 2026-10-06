# Stage 7 — programmable palette (`VPAL`)

This extends the hardware-tested TEXT example with a writable 16-entry
RGB888 palette shared by TEXT foreground/background and EGA pixel indices.
The earlier examples remain standalone references; this directory contains
its own CPU/GPU copies with the new palette interface.

## Demo

The `.falc` program initializes all 16 entries with custom colours. Frame 1
contains `VPAL SHARED PALETTE`, an information line and 16 two-cell `A`
swatches. Frame 0 contains 16 horizontal EGA colour bars. After drawing both
frames, the CPU changes only palette entry 15 (red advances by 0x20, green
is 0x80, blue is 0x40) and alternates TEXT/frame 1 and EGA/frame 0.
The first TEXT scene lasts one animation step; subsequent scenes last 16
steps. The title, swatch foregrounds and last EGA bar change colour without
rewriting the framebuffers. Timing comes from a CPU delay loop. Existing
`VMODE` changes are live while `VPRE` switches frames at the next vblank;
a scene transition can briefly interpret the previous frame in the new
mode for part of one frame.

The output remains centered 960x640 inside 1280x720p60, with fixed bright
blue margins. RESET on T15 is active low and restores the default palette
before the program initializes its custom colours again.

## Instruction contract

| Item | Behavior |
|---|---|
| A | Full-byte palette index, valid only for 0..15 |
| B / C / D | Red / green / blue, eight bits each |
| Registers and flags | Unchanged by `VPAL` |
| Invalid indices | No-op, without issuing a GPU request |
| Completion | CPU waits until the GPU acknowledges the update |
| Visibility | Live update when received in the pixel domain; no `VPRE` needed |
| Atomicity | All three RGB channels of one entry update on one pixel-clock edge |
| Palette scope | Global to both framebuffers and both video modes |
| Reset | Original 16 entries from `fridge.h`, matching the TEXT example |
| Margins / blanking | Fixed margin RGB / black; independent of the palette |

The register mapping follows `ir_VPAL` in `fridgemulib.c`. There is one
intentional source correction: that function stores `3*A` in an eight-bit
`FRIDGE_WORD`, allowing large indices such as 86 to wrap into palette
storage. Hardware checks the full A value before indexing, so every index
16..255 is ignored. No panic or CPU halt is introduced.

## Clock crossing and timing

The CPU sends `GPU_PALETTE_SWITCH`, an index and RGB, then waits for READY
to go low (accepted) and high (completed). The GPU captures the payload in
command-domain holding registers and toggles a request. Two pixel-domain
synchronizer stages deliver the request; a single write updates the entry
and toggles the acknowledgement. Two command-domain synchronizer stages
return the acknowledgement. Payload registers stay unchanged until that
return. Busy requests are ignored, and a held write strobe is accepted
once, preventing a repeated write after acknowledgement.

Both reset domains must be asserted together, as the board top does; each
releases synchronously to its own clock. Reset discards pending work.

`palette.ucf` excludes only asynchronous control captures from synchronous
analysis, including the acknowledgement's first synchronizer. It bounds the
held index/RGB path to the receiving palette registers to 20 ns with a
`DATAPATHONLY` FROM/TO constraint. The earliest synchronized request capture
allows at least two pixel periods (26.936 ns) for that data to settle.
Subsequent synchronizer stages retain normal clock-period timing checks.
The ISE syntax follows the [Xilinx Constraints Guide, UG625](https://www.amd.com/content/dam/xilinx/support/documents/sw_manuals/xilinx14_7/cgd.pdf).

## Commands

From the toolchain root:

```sh
make -C examples/palette regen
make -C examples/palette test
make -C examples/palette
make -C examples/palette timing
make -C examples/palette load
```

`load` programs FPGA SRAM. The demo runs continuously: LD0 stays low,
LD1 is the heartbeat, LD2 indicates clock locks, and LD3 indicates frame
activity. LD5/LD7 are unused configuration pins.

## Files and verification

- `src/palette_demo.falc` / `FridgeRAMBootImage.vhd`: the real demo and its
  generated boot image. `make regen` uses the Stage 6 assembler.
- `FridgeCPU.vhd`: TEXT CPU plus the `VPAL` handler and palette handshake.
- `fridge_gpu.vhd`: TEXT GPU plus writable palette and request/ack mailbox.
- `palette.vhd` / `.prj` / `.xst` / `.ucf`: Atlys HDMI top and build inputs.
- `tb_gpu.vhd`: unchanged TEXT/EGA pixel-exact scanout regression against
  the new GPU, including swaps, offsets, scaling, glyphs and raster timing.
- `tb_palette.vhd`: all entries/channels, every invalid index, EGA nibble
  order, TEXT foreground/background, live atomic updates, margin/blanking
  independence, asynchronous clocks, a slower receiving clock, busy
  rejection, input payload mutation, held strobes and reset during an
  in-flight update, followed by all default entries and a new write.
- `src/vpal_contract.falc` / `FridgeRAMTestImage.vhd` / `tb_contract.vhd`:
  a generated CPU test program covering all 256 A values with a slower
  pixel clock. RAM snapshots independently check A/B/C/D/E/H/L and flags
  after every instruction; exactly 16 valid palette requests must occur.
- `tb_system.vhd`: the real demo image through CPU/RAM/GPU, checking every
  framebuffer write, every initial and animated palette request, TEXT
  swatch pixels and all EGA bars. Animation must continue without new
  framebuffer writes or a halt.

Individual simulation targets are `test-gpu`, `test-palette`,
`test-contract` and `test-system`. Each preserves a `tb_*.log` after passing;
run them sequentially because ISim uses the directory's shared work library.

ISE synthesis, mapping, place/route and bitstream generation pass. Post-route
timing has zero setup/hold errors and all constraints met:

| Check | Result | Requirement |
|---|---|---|
| Pixel domain | 13.275 ns | 13.468 ns (74.25 MHz) |
| CPU domain | 47.846 ns | 100 ns (10 MHz) |
| Held palette payload | 5.604 ns maximum | 20 ns |
| BRAM | 64 RAMB16BWER | 116 available |

The palette uses flip-flops, adding no BRAM. Bitstream: `palette.bit`.
All four simulations pass: the inherited TEXT/EGA scanout regression,
programmable palette/CDC suite (including observation of both old and new
RGB during an active-row update), CPU ABI contract test and real demo
integration. The demo performs 156 TEXT byte stores and 19200 EGA byte
stores during setup, then animates solely through palette commands.
Hardware gate complete: the user tested `palette.bit` on the Atlys and
confirmed the demo works (reported 2026-10-06).

Remaining step 7 work: `VFSI`/`VFSAC`/`VFLA`/`VFLAC`, then the 32 KB sprite
store, access instructions and compositing.
