# Atlys HDMI Rectangle Demo

Standalone VHDL-93 demo for Digilent Atlys revision C (`xc6slx45-3-csg324`).
Connect a monitor to the dedicated **HDMI OUT** connector.

## Picture

- Output: 1280x720 at 60 Hz, progressive, positive horizontal/vertical sync.
- White rectangle: 960x640, at x=160..1119 and y=40..679.
- Background: pure blue (`RGB = 00,00,FF`). Rectangle: `FF,FF,FF`.
- Margins: 160 pixels left/right and 40 pixels top/bottom.
- Video is DVI-compatible TMDS over HDMI, without audio, EDID/DDC negotiation,
  or HDMI data islands. Most HDMI monitors accept this fixed mode.

## Commands

From the toolchain root:

```sh
make -C examples/hdmi
make -C examples/hdmi test
make -C examples/hdmi timing
make -C examples/hdmi load
```

`load` programs FPGA SRAM only; it does not write flash. The generated image
is `examples/hdmi/hdmi.bit`. The board RESET button on T15 is active low;
pressing it restarts clocking and the raster. LED0, LED1 and LED2 indicate DCM,
PLL and BUFPLL lock;
LED3 toggles every 32 frames (about 0.53 seconds).

ISim requires GCC/G++ in the ISE image. The toolchain Dockerfile now includes
them, and the local `ise:14.7` image has been updated with these packages.

## Implementation

`video_pattern.vhd` generates a 1650x750 raster: horizontal active/front porch/
sync/back porch = 1280/110/40/220, vertical = 720/5/5/20.
`tmds_encoder.vhd` produces DC-balanced 10-bit words, with sync on the blue
channel during blanking. `tmds_serializer.vhd` captures each complete word,
emits low/high 5-bit halves, and uses cascaded OSERDES2 in 5:1 SDR mode.
The primitive wiring is adapted from LiteVideo; see `LICENSE.litevideo`.

Clock chain: 100 MHz -> DCM 99/100 -> 99 MHz -> PLL 15/2 -> 742.5 MHz VCO.
The PLL supplies 74.25 MHz pixels, 148.5 MHz gearbox clock, and 742.5 MHz to
BUFPLL and the I/O serializers. The serial clock never enters fabric logic.
Clock-lock loss asserts downstream reset asynchronously; release is synchronized
in each domain. Use RESET if a clock generator needs to reacquire lock.

The UCF constrains the 100 MHz input; ISE derives related output periods.
Only the asynchronous button and lock-loss reset paths are excluded from timing.
Pixel-to-gearbox paths remain timed; no dedicated-clock routing override is used.

## Verification

- Raster simulation checks every pixel of one complete frame, sync windows,
  exact colors, rectangle boundaries, pixel counts, and frame wrap.
- Encoder simulation decodes 32,768 symbols covering all byte values and
  checks control words and cumulative DC balance.
- Xilinx primitive simulation checks 128 consecutive serialized words,
  LSB-first order and differential polarity.
- Top-level primitive simulation checks clock-chain lock and the forwarded
  74.25 MHz clock. It uses a bounded run because DCM/PLL models keep running.
- ISE build and post-route timing pass: zero setup/hold errors, zero bitgen DRC
  errors, 40 occupied slices, 80 slice registers, 114 LUTs, no BRAM.

ISE emits a DCM input/output phase warning; there are no synchronous data
crossings from the reference clock into the video domains. It also warns about
rounding of the PLL input period (10.10101 versus 10.1010101 ns); analysis uses
the derived constraint. Constant-color optimization removes redundant encoder
registers. The 742.5 MHz constraint has no fabric paths: serialization uses
dedicated I/O hardware, within the device's specified clock range.
The timing pass covers constrained internal paths, not the HDMI electrical eye
or an external source-synchronous output budget. Monitor/cable validation is
still required.

Hardware programming and monitor display have not yet been verified.
