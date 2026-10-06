# Spartan-6 repository integration

The canonical board target is now
`/mnt/data/Projects/Spartan6Fridge/fridge/fpga/fridge_spartan6/` in the Fridge
repository, on branch `spartan6-atlys`.

## History and local inputs

- Fridge starts from the pushed `spartan6-atlys` commit `48a8703` (shared
  assembler VHDL aggregate emitter and multiplication carry fix).
- Subtree import commit `d0ba855` preserves the standalone toolchain's full
  history through `aa704c1`; this is an ordinary directory in one repository.
- Ignored ISE installers, Adept packages/runtime and license files were copied
  to the new directory as independent files. Existing Docker/host setup is
  reused. The Docker build context includes only installation inputs.
- The assembler now builds from root `falc/` into this target's ignored
  `.local/falc-build/`. Standard includes use root `x2al_std/`.
- The original `Spartan6Toolchain/`, including its nested Fridge clone and
  build artifacts, remains untouched. It must stay available until board
  verification of this layout is complete.

## Verification on 2026-10-06

- Shared falc builds successfully from the new location.
- Regenerated ROM-paint (48 bytes), TEXT (248), palette demo (459) and VPAL
  contract (67) binaries match the original byte for byte. The packed ROM
  VHDL also matches; RAM VHDL changes only the source-path comments.
- ROM packer self-test and shell syntax checks pass.
- All 17 ISim testbenches pass across HDMI (3), CPU (1), GPU (2), keyboard
  (3), ROM (2), TEXT (2) and palette (4).
- Fresh palette synthesis, map, place-and-route, bitstream generation and
  post-route timing pass. All constraints are met with zero timing errors;
  BRAM usage is 64 of 116 RAMB16BWERs.
- The palette bitstream's 1,484,404-byte FPGA configuration payload matches
  the original byte for byte (SHA-256
  `7ed5256d643ac2aa2d12898eb26ca1f33576124dede89114abc11c0ef8f2ddc7`).
  Build-path and timestamp headers differ.
- All 85 tracked VHDL files match the original apart from generated
  source-path comments. All 100 copied local installation/license files
  match and have independent inodes. The new Adept wrapper detects the Atlys.
- The original tree matches the pre-migration snapshot: 2,761 file metadata
  entries (including both Git directories) and 194 tracked-file SHA-256
  hashes are unchanged.
- Hardware verification of the newly generated bitstream was pending at
  this stage; the relocated combined-system board gate completed below.
  Earlier TEXT and palette board results are recorded in `PORTING_PLAN.md`.

Timing results from `examples/palette/palette.twr` / `palette.par`:

| Constraint | Requirement | Achieved |
| --- | --- | --- |
| CPU clock (10 MHz) | 100.000 ns | 47.846 ns |
| Pixel clock (74.25 MHz) | 13.468 ns | 13.275 ns |
| Palette CDC payload | 20.000 ns | 5.604 ns |

Local compiler, regeneration, simulation and build logs are retained under
`.local/verification/` (ignored by Git).

## Board verification

The user tested the canonical combined demo from the new board directory on
the Atlys and confirmed it works on 2026-10-07. This completes the relocated
build's board gate; canonical simulation and timing results are recorded in
`INTEGRATION.md`. The original directory remains untouched and retained.

To rebuild and load the verified combined demo from the new board directory:

```bash
make all timing
make load
```

The demo controls are documented in `programs/integration/README.md`.
