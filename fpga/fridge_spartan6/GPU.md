# Advanced GPU contract on Atlys

The canonical implementation is `rtl/video/fridge_gpu.vhd`,
`fridge_gpu_commands.vhd` and `fridge_sprites.vhd`, with instruction dispatch in
`rtl/core/FridgeCPU.vhd`. Framebuffer operations address the active frame selected
by VPRE, including when it is also visible. Existing TEXT/EGA/VPAL behavior and
10 MHz CPU / 74.25 MHz HDMI clocks are retained.

## Instructions

| Instruction | Behavior |
| --- | --- |
| VFSA | Store A at framebuffer byte address HL (existing instruction). |
| VFSI arg0, arg1 | Store two immediate bytes at HL and HL+1, then advance HL by 2. |
| VFSAC | H=x, L=y. At even x, replace the high pixel nibble with A's high nibble; at odd x, replace the low nibble with A's low nibble. Preserve the neighboring pixel. |
| VFLA | Load the active framebuffer byte at HL into A. |
| VFLAC | H=x, L=y. Load the pixel's palette index (0..15) into A. |
| VSSA | Store A at sprite byte address HL. |
| VSSI arg0, arg1 | Store two immediate bytes in sprite memory and advance HL by 2. |
| VSLA | Load the sprite-memory byte at HL into A. |
| VS2F | Copy one sprite byte from HL into the active framebuffer at BC. |
| VSS | Define sprite A (0..63), width B, height C, data address HL. Hide it until VSD. |
| VSD | Set sprite A's mode B and position H=x, L=y. |

All instructions preserve flags and registers except their documented A/HL
results. Framebuffer byte access is mode independent: TEXT cells and packed
EGA pixels share the same 19200-byte aperture. Coordinate access always uses
the 240x160 packed EGA layout, including while TEXT is selected.

Both bytes of an immediate store must fit: VFSI accepts HL=0..19198 and VSSI
accepts HL=0..32766. The last valid pair advances HL to 19200 or 32768.
Out-of-range requests are no-ops; invalid reads preserve A and invalid immediate
stores preserve HL. This follows the port's existing non-panicking VFSA policy.
Coordinates must satisfy x<240 and y<160. They never alias another row.

Sprite storage is **32 KiB**, addresses 0..32767. VSS accepts dimensions 1..255
provided `HL + ceil(width*height/2) <= 32768`; the last byte may be used.
Invalid indices, dimensions, modes or ranges preserve the previous descriptor.
VSD accepts modes 0..7. Sprites are clipped to the 240x160 frame, without
coordinate subtraction wrapping around an edge.

## Sprite pixels and compositing

Data is a continuous packed 4-bit stream beginning at the VSS address:
`byte = HL + floor((local_y*width + local_x)/2)`. The first pixel is the high
nibble. Odd-width rows continue at the next nibble, without padding.

Sprites apply only in EGA, in ascending descriptor order. The first four
non-invisible sprites covering a pixel participate, including transparent-zero
pixels. Later overlapping sprites are ignored. Frame offsets apply to both the
background and sprite coordinates. Palette lookup precedes blending.

| Mode | Operation on RGB888 channels |
| --- | --- |
| 0 | Invisible; does not consume an overlap slot. |
| 1 | Opaque replacement. |
| 2 | Replace unless the sprite palette index is zero. |
| 3 | Add, saturating each channel at 255. |
| 4 | Subtract, saturating each channel at zero. |
| 5 | Bitwise AND. |
| 6 | Bitwise OR. |
| 7 | Bitwise XOR. |

These semantics follow `include/fridge.h` and the emulator's mode operations.
The emulator currently has two rendering bugs: it uses the sprite's data size
instead of its configured data address, and indexes the palette with a packed
byte instead of a selected nibble. Hardware follows the documented packed-data
contract. Other explicit differences are 32 KiB rather than 64 KiB storage,
strict coordinate bounds/clipping, no-op invalid access instead of panic, and
accepting a descriptor whose exclusive data end is exactly the memory limit.
The emulator is unchanged.

## Hardware implementation

GPU frame and sprite BRAM write ports infer WRITE_FIRST, avoiding the
dual-clock READ_FIRST overlap issue described in
[AMD AR34533](https://docs.amd.com/r/en-US/34533/Solution). A simultaneous
read/write of the same visible location can still produce transient pixels;
programs can use the hidden frame or hide a sprite while replacing its data.

Advanced commands use explicit request, ready and return signals. CPU-side
frame and sprite BRAM ports have registered reads; pixel stores use a
read-modify-write sequence. The CPU waits for completion and holds the request
payload throughout the operation. There are no new internal tri-state buses.

Descriptor updates cross into the pixel domain through a held-payload mailbox
with two-stage request/acknowledgement synchronization. VSS/VSD complete only
after the descriptor has been captured. UCF constrains payload settling to
20 ns and excludes only the first synchronizer stages.

The renderer snapshots descriptors for each logical row, selects up to four
hits per pixel, and fetches their packed colors into two alternating line
caches. It prepares each row during the preceding four HDMI raster lines.
With all 64 descriptors covering every pixel, preparation takes at most
5344 pixel clocks, below the 6600 available. Four registered RGB blending
stages preserve the timing budget; sync, active and pixel coordinates follow
the same pipeline. Descriptor changes appear as subsequent rows are prepared.
Sprite RAM writes are live; hide a sprite before replacing its data when an
atomic visual update is required. Palette updates use the existing shared
acknowledged VPAL path.

Reset hides all sprites and invalidates cached rows. Frame and sprite RAM are
retained over warm reset; programs initialize their own data. Both domains use
the board's synchronized reset release.

## Verification

`make test` includes `access` (CPU ABI, reads/writes, bounds, frame selection,
flags and reset), `sprites` (full-frame pixel reference for all modes, odd-width
packing, clipping, offsets, descriptor 63, transparent-zero overlap counting,
64-hit worst case and reset), and `graphics` (actual boot program, PS/2 controls,
TEXT/EGA pixels and warm reset), alongside the earlier regressions.

Run one suite with `python3 tools/run-tests.py access`, `sprites`, or `graphics`.
See `programs/graphics/README.md` for the hardware demo. Its hardware gate is
pending; the earlier combined integration demo remains hardware-verified.

ISE can map the small ROM image to RAMB8BWER. Its initialization warning is
covered by the ISE 13.2+ bitstream fix described in
[UG383, page 27](https://docs.amd.com/api/khub/documents/Xs~b~O94R6gZSgSD34Yh5Q/content).
This target uses ISE 14.7 with default 9 Kb block-RAM initialization enabled and no
bitstream encryption.

## Results on 2026-10-07

All 14 canonical suites pass. After the RAM-mode correction, the CPU access,
sprite stress and actual graphics-program suites were rerun and pass, including
reset recovery. Both programs complete synthesis, map, place-and-route,
bitstream generation and post-route timing with zero setup/hold errors.

| Constraint | Requirement | Graphics | Integration |
| --- | --- | --- | --- |
| CPU (runs at 10 MHz) | 100.000 ns | 40.318 ns | 40.296 ns |
| Pixel (74.25 MHz) | 13.468 ns | 13.398 ns | 13.414 ns |
| Double pixel (148.5 MHz) | 6.734 ns | 6.636 ns | 4.695 ns |
| Palette payload | 20.000 ns | 5.677 ns | 8.216 ns |
| Sprite descriptor payload | 20.000 ns | 11.607 ns | 11.389 ns |

Both builds use 80/116 RAMB16BWERs plus one RAMB8BWER for the small ROM image,
and 2/58 DSP48A1s. Graphics uses 8884/27288 LUTs and 3627/6822 slices;
integration uses 8908 LUTs and 3557 slices. The routed XDL files confirm that
all 48 GPU frame/sprite RAMB16BWERs use WRITE_FIRST on both ports. Bitgen reports
the special 9 Kb initialization format and `Encrypt=No`.

Each 1484776-byte bitstream and its reports are in `.local/build/<program>/`.
Final build logs are `.local/graphics-build.log` and
`.local/graphics-integration-build.log`. Simulation logs remain in
`.local/tests/<suite>/`; `.local/graphics-memory-regressions.log` summarizes the
final affected-suite reruns. These artifacts are ignored by Git.

The new framebuffer/sprite **hardware gate remains pending**. Earlier hardware
results apply to the previously tested integration/TEXT/palette bitstreams.
The original standalone tree and milestone example RTL remain unchanged.
