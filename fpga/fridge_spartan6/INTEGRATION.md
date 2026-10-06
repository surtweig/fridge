# Canonical Fridge system for Atlys

`rtl/` is the source for the combined computer. It includes the implemented
CPU/RAM, EGA/TEXT GPU, programmable palette, keyboard and bitstream ROM.
Existing `examples/` remain independently buildable milestone snapshots while
this system is verified on hardware. New hardware work belongs in `rtl/`.

## Build and program

Run from `fpga/fridge_spartan6/`:

```bash
make                     # .local/build/integration/fridge.bit
make timing              # .local/build/integration/fridge.twr
make test                # 10 shared-RTL unit regressions + combined-system test
make test-system         # combined-system test only
make regressions         # original 17 milestone testbenches
make load                # load the combined bitstream into FPGA SRAM
```

`PROGRAM=integration` is the default. A program directory supplies `boot.falc`
and ordered `rom/*.hex` sections. `make PROGRAM=name` generates its RAM/ROM
packages and manifests in `.local/build/name/`, so packages with the same VHDL
names never share a build library. `make regen` forces image regeneration;
normal builds track assembly, standard-library and compiler-source changes.
The host compiler cache remains `.local/falc-build/`.

From a build/test subdirectory, the ISE wrapper mounts shared board sources
read-only and the current working directory writable. Direct board-root tool
invocation also works, with a single writable current-directory mount. Compiler outputs, manifests, test executables,
logs and bitstreams remain ignored by Git. `make clean` removes only the
selected program's build directory.

## Architecture

- `rtl/core/`: one CPU, RAM implementation, ISA/global packages and a debug
  observation record. The CPU combines the palette version's VPAL command
  handshake with the ROM version's IIN/IOUT timing fixes.
- `rtl/video/`: one framebuffer/GPU, raster font, 720p timing generator,
  TMDS encoders and serializers. TEXT and EGA share both frames and the same
  RGB888 palette.
- `rtl/devices/`: PS/2 receiver/keyboard FIFO and Case B bitstream ROM.
- `rtl/fridge_system.vhd`: connects all components using externally supplied
  CPU/pixel clocks and resets. Boot RAM and ROM images are generic inputs.
  The debug record observes the actual system bus and GPU commands in tests;
  it is left open in the hardware build.
- `rtl/atlys_clocks.vhd`: existing 100 MHz oscillator -> 10 MHz CPU and
  74.25 MHz pixel-clock chain, with asynchronous reset assertion and separate
  synchronized release in each clock domain.
- `rtl/atlys_top.vhd`: board clock/reset wrapper, computer, HDMI output and LEDs.
- `constraints/fridge_atlys.ucf`: HDMI, keyboard and status pins plus timing.
  Targeted palette-control exclusions and the 20 ns held-payload bound remain
  explicit; there is no blanket CPU-to-pixel timing exclusion.

The canonical CPU also fixes warm reset: both internal fetch/stack variables
and their registered addresses reset together, normal execution is skipped
while reset is asserted, and RAM/I/O write strobes are suppressed. Previously
the end-of-process variable assignments overwrote the reset addresses. The
integration test exposed this by restarting the running polling program; it
also resets the system during an active ROM stream. This fix is confined to
canonical RTL while the old example snapshots remain intact.

The CPU and devices use separate read/write data signals. A single mux picks
ROM or keyboard read data. Reads of unmapped IDs, including the write-only ROM
reset device, return zero. Unknown writes are ignored. The existing device
strobe timing is preserved: IIN samples during the read state and advances the
selected device at the rising edge ending it; IOUT latches at the corresponding
write edge. ROM reads cannot pop the keyboard FIFO and keyboard reads cannot
advance ROM.

| Device ID | Function |
| --- | --- |
| 1 | ROM mode/segment writes and 256-byte LOAD stream reads |
| 2 | ROM reset writes (nonzero resets; zero ignored) |
| 3 | Keyboard event FIFO reads |

CPU RAM remains 64 KiB, video 240x160 / 40x20 TEXT, and stack descending from
0xFFFF. Interrupts/PAM16, persistent flash ROM, remaining framebuffer access
instructions and sprites are deferred as in the porting plan. The current
small ROM image is LUT mapped; integration does not introduce a flash backend
or claim boot-loader interrupt compatibility.

## Demo and verification

See [the integration program](programs/integration/README.md) for controls and
expected output. Unit tests under `sim/units/` are adapted from the existing
milestone benches and compile canonical RTL. The CPU VPAL contract test still
checks all 256 indices and register/flag preservation with a slow pixel clock.
The ROM/keyboard contract tests retain select guards, bounds, FIFO, error and
reset coverage, with zero-valued unselected read outputs replacing tri-states.

`sim/integration/tb_system.vhd` runs the real assembly program on
`fridge_system`, sends physical PS/2 bit traffic, and independently checks ROM
bytes, keyboard event order, every framebuffer write, unknown-device traffic,
TEXT/EGA raster pixels, shared palette updates, hidden-frame text writes and
recovery from warm reset and a reset during ROM streaming. Every suite uses
its own `.local/tests/<suite>/` work library and log.

Verification on 2026-10-06:

- All 11 canonical testbenches pass (10 unit/regression suites plus the real
  combined-system program). The full-system test includes cold start, warm
  restart and restart after aborting an active ROM stream.
- Synthesis, map, place-and-route and bitstream generation pass. Post-route
  timing meets all constraints with zero setup/hold errors; the palette
  payload constraint checks 1,920 paths and has zero failing endpoints.
- Utilization: 64/116 RAMB16BWERs, 4,029/27,288 LUTs and 1,400/6,822 slices.
- Bitstream: `.local/build/integration/fridge.bit`; implementation/timing logs
  and reports remain in the same ignored build tree. Per-suite simulation
  logs are `.local/tests/<suite>/test.log` and `isim.log`.
- Earlier example RTL and the original standalone directory are unchanged.
- Hardware verification of this combined bitstream is pending. Earlier
  milestone board gates remain recorded separately in `PORTING_PLAN.md`.

| Constraint | Requirement | Achieved |
| --- | --- | --- |
| CPU clock (runs at 10 MHz) | 100.000 ns | 32.416 ns |
| Pixel clock (74.25 MHz) | 13.468 ns | 12.742 ns |
| Double pixel clock (148.5 MHz) | 6.734 ns | 4.807 ns |
| Palette held-data payload | 20.000 ns | 4.584 ns |

The original `/mnt/data/Projects/Spartan6Fridge/Spartan6Toolchain/` remains
untouched and must be retained until verification of the relocated build is
complete.
