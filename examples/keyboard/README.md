# Atlys Keyboard: PS/2 Reception and Key-Event FIFO

Adds PS/2 reception from the Atlys USB-HID bridge and the 32-entry Fridge
keyboard FIFO to the validated CPU/GPU/HDMI system (`../gpu`), read by the
CPU through `IIN 3` exactly like the emulator's `FRIDGE_KEYBOARD_CONTROLLER`.
Digilent Atlys revision C (`xc6slx45-3-csg324`).

## Picture

The boot image is a live **key-event tape**. It clears the 240x160 internal
frame to black, presents it with `VPRE MANUAL_11` (displayed and active
frames both frame 1, single buffered), then polls `IIN 3` forever and paints
each event byte as one framebuffer byte (two 4bpp pixels) at the tape cursor,
wrapping at 19200. Press events have bit 7 set (bright left pixel), releases
are dark; the byte value is the `FRIDGE_KEYBOARD_*` event byte. Typing 'a'
shows `E1 61` as pixel pairs at the top left.

## Keyboard contract

Event bytes match `fridge.h` / `fridgemulib.c` exactly: bit 7 = pressed
(`FRIDGE_KEYBOARD_KEY_STATE_MASK`), bits 6..0 = 7-bit Fridge key code
(`FRIDGE_KEYBOARD_KEY_CODE_MASK`, the ASCII subset of
`fridge_emulator/src/keymap.h`).

| Operation | Semantics |
|-----------|-----------|
| `IIN 3` (`FRIDGE_DEV_KEYBOARD_ID`) | Pops the oldest buffered event. An **empty read returns 0x00** and is a no-op (the read pointer does not advance; no event byte is ever 0x00). |
| FIFO write | One event per key press/release; typematic repeat emits repeated press events, like the emulator's repeated KEYDOWN handling. |
| Overflow | **Defined here: when all 32 entries (`FRIDGE_KEYBOARD_BUFFER_SIZE`) are unread, a new event is dropped** — the 32 buffered events keep their original order — and the sticky `OVERFLOW` flag rises until reset (LED3). The emulator instead wraps its ring and overwrites the oldest unread entry (scrambling order once partially consumed); hardware preserves order and reports the loss. A CPU-visible overflow status read is reserved until the I/O map has a defined status contract. |

PS/2 scan-code-set-2 translation (US layout), matching the emulator's
event-time folding (`keymap.h`):

- `0xF0` = break prefix, `0xE0` = extended prefix; `0xE1` (Pause) swallows the
  remaining 7 bytes of its fixed 8-byte sequence.
- Modifiers emit no events: Shift (`0x12`/`0x59`) and CapsLock (`0x58`, make
  toggles) fold case at event time (letters: Shift XOR CapsLock; digits and
  punctuation: Shift); Ctrl/Alt are ignored.
- Extended keys are dropped except `E0 5A` (keypad Enter) -> `'\n'`; arrows
  and other E0 keys are unmapped, as in `keymap.h`.
- Keypad digits (`70 69 72 7A 6B 73 74 6C 75 7D`) -> `'0'..'9'`.
- Mapped codes: Esc/Enter/Backspace/Tab/Space, a-z/A-Z, 0-9 and the full US
  punctuation set. **Documented divergence:** Shift+digit folds to
  `)!@#$%^&*(` like the emulator, but Shift+punctuation folds to the shifted
  character (Shift+`,` -> `'<`); the emulator's SDL frontend returns the
  unshifted code for punctuation keys.
- Protocol bytes (`AA` BAT, `FA` ACK, `FE`, `00`, ...) fall out of the keymap
  and are silently dropped.

## Hardware: the USB-HID bridge

The "Host" USB-A connector (J13) is served by a **PIC24FJ192** (the master
UCF's net names `PIC32-*` are stale; the Atlys reference manual rev C sec. 11
names the part) which converts a single USB keyboard (or mouse) to **PS/2
protocol** on four FPGA pins. Keyboard PS/2: **K_CLK = P17, K_DAT = N15**
(bank 1, 3.3 V). The stock `constraints/AtlysGeneral.ucf` calls these nets
`USBCLK`/`USBSDI` and describes an SPI interface — that file belongs to an
earlier board revision; the PS/2 mapping is confirmed by the reference
manual's pin table and community sources (litex-buildenv `platforms/atlys.py`,
stromeko's Atlys notes). The bridge runs standard scan code set 2 at a
10-16.7 kHz PS/2 clock (15 kHz typical).

Practical notes from the reference manual and field reports:

- There are **no external pull-ups** on the PS/2 lines; the UCF enables the
  FPGA internal ones (`PULLUP`) — without them nothing works.
- The keyboard side is receive-only safe (no host-to-device traffic is
  needed); the mouse side would need host TX to leave its idle state, and is
  out of scope.
- JP11 ("HOST") should be **open** during normal use (loaded = the PIC looks
  for a USB stick at power-on and lights LD7).
- A keyboard that enumerates but uses a hub will not work; only a single
  keyboard or mouse is supported.

The hardware gate (see PORTING_PLAN.md) is recorded from the actual board
with this bitstream: a USB keyboard on J13 (JP11 open) paints key-event tape
pixels on the 720p display (letters, digits, numpad, space, enter,
backspace). The pin/protocol evidence above is documentation-level.

**Deferred anomaly:** on hardware the tape is silent for Esc although Esc is
mapped to 0x1B (its 0x9B press pixels, bright blue / bright cyan, would be
plainly visible), and Up/Down/Left appear to produce events although all four
arrows are E0-extended and unmapped (`tb_system` asserts the Up arrow yields
no event). Silence for F-keys, Ctrl, Alt, Shift and Ins/Del/Home/End/PgUp/
PgDn is by design (keymap parity). Investigate later with a raw scan-code
trace of what the PIC24 bridge actually emits for Esc and the arrow keys.

## Commands

From the toolchain root:

```sh
make -C examples/keyboard
make -C examples/keyboard test
make -C examples/keyboard timing
make -C examples/keyboard load
```

`load` programs FPGA SRAM only; it does not write flash. The generated image
is `examples/keyboard/keyboard.bit`. Plug a USB keyboard into the **Host**
port (J13), JP11 open, and type: events appear as pixel pairs at the top
left, growing left to right, wrapping after 9600 events.

LEDs (LD5/LD7 share configuration pins and are unused):

| LED | Meaning |
|-----|---------|
| LD0 | CPU halted (this demo runs forever, stays low) |
| LD1 | ~2 Hz heartbeat |
| LD2 | all four clocks locked |
| LD3 | keyboard FIFO **overflow** (sticky until reset) |
| LD4 | PS/2 receive **error** (sticky: parity/start/stop rejected or truncated frame abandoned) |
| LD5 (pin P16) | PS/2 valid-byte activity (toggles per scan byte) |

The RESET button on T15 is active low.

## Implementation

- `ps2_receiver.vhd` — device-to-host PS/2 byte receiver on the 10 MHz CPU
  clock: two-flop synchronizers, 11-bit frames (start, 8 data LSB first,
  odd parity, stop), start/parity/stop validation, and a watchdog that
  abandons a truncated frame after 150 us of PS2_CLK idle high (longer than
  any legal bit high time, shorter than inter-frame gaps) and re-aligns.
- `fridge_keyboard.vhd` — scan-code-set-2 decoder (E0/F0 prefixes, Pause
  swallow, Shift/CapsLock state), the keymap above, and the 32-entry
  event FIFO with the defined overflow behavior and the `IIN 3` device
  interface (combinational show-ahead read data; one pop per `DEVICE_READ`).
- `keyboard.vhd` — `../gpu/gpu.vhd` clock chain and CPU/GPU/TMDS path plus
  the keyboard instance wired to the CPU device bus and the 6-LED status.
- `FridgeRAMBootImage.vhd` — tape demo program (hand assembled): VMODE
  TEXT then EGA, 19200-byte clear loop, VPRE MANUAL_11, poll/tape loop with
  a wrap check at 19200 (0x4B00).
- `FridgeCPU.vhd` — copy of `../gpu` with the device-bus fixes below; the
  GPU/TMDS sources are unchanged copies of `../gpu` (the Docker harness
  mounts only the example directory).

### CPU changes vs `examples/gpu`

1. `DEVICE_DATA` is now a real bidirectional bus: the CPU drives it only
   during `CPU_DEVICE_WRITE` (value `rA`, valid the whole state) and
   tri-states otherwise, so a device can drive it during `IIN`. Previously
   the CPU drove the bus constantly and read an undriven internal signal.
2. `ir_IIN` captures the resolved **port** value (`deviceDataIn` follows
   `DEVICE_DATA`); previously it read the internal `deviceData` signal,
   which never saw external drivers, so `IIN` could not work at all.
3. `DEVICE_READ` is a pure decode of the FSM state (high for exactly the one
   `CPU_DEVICE_READ` cycle); the old in-process assignment latched the level
   and held it until the next device write. Device contract: present read
   data from the start of the state (the CPU captures at the mid-state
   falling edge), advance the read side at the rising edge that ends the
   state.
4. `ir_VMODE` decodes `mode(7)` (A bit 0 per `fridge.h`), same fix as
   `../gpu`; `examples/keyboard/tb_system` locks the A=0x01 -> TEXT decode
   through the CPU.

The `IOUT` device-write timing (no write strobe exists yet) is **not**
validated by this stage; only the `IIN` read path is.

## Verification

- `tb_ps2_receiver.vhd` (receiver unit): valid frames at 15 kHz and at the
  slowest spec rate 10 kHz; parity/start/stop rejection; watchdog abandons a
  truncated frame and the next frame is clean; idle gaps are not errors; all
  256 byte values back-to-back at accelerated rate.
- `tb_keyboard.vhd` (decoder + FIFO, PS/2 pins to `IIN` protocol):
  make/break events; Shift and CapsLock folding and their invisibility;
  keypad Enter and dropped arrows; Pause swallow; digit and punctuation
  folding; empty reads return 0x00 without advancing; reads of other device
  IDs neither drive the bus nor pop; 32-event FIFO order with the 33rd event
  dropped, `OVERFLOW` sticky; sticky `RX_ERROR` on rejected frames.
- `tb_system.vhd` (CPU + RAM + GPU + keyboard, 10 MHz + 74.25 MHz): runs the
  real boot image; types real PS/2 traffic (`a` down/up, Shift+`b` down/up,
  `1` down, Up arrow, Enter down); verifies the VFSA stream (19200 zero fill
  bytes then the 6 event bytes `E1 61 C2 42 B1 8A` in order), exactly two
  VMODE with the TEXT-then-EGA A-bit-0 decode, one `VPRE MANUAL_11`, that
  the CPU keeps polling `IIN 3`, no overflow or receive errors, and the
  on-screen tape pixel pairs (palette colors of `E1 61 C2`) and blue margins.
- ISE build: synthesis, map, place/route, bitstream (0 DRC errors) and
  post-route timing with **zero errors, all constraints met**. Resources:
  558 registers, 8674 LUTs (31%), 3008 slices (44%), 64 RAMB16BWER (32 CPU
  RAM + 32 frame buffers), 2 DCM_CLKGEN, 1 PLL_ADV, 4 BUFG. Tightest
  constrained path: 11.84 ns against the 13.47 ns pixel period (74.25 MHz);
  the CPU domain closes at 57 ns against 100 ns.
- Regressions: `make -C examples/hdmi test`, `make -C examples/cpu test` and
  `make -C examples/gpu test` all pass unchanged.

### Constraints

`keyboard.ucf` is `../gpu/gpu.ucf` plus the PS/2 pins (P17/N15, LVCMOS33,
internal `PULLUP`, `TIG` — they only feed input synchronizers) and two more
LEDs. The `TS_cdc` exclusion for the CPU-to-pixel crossings is unchanged.

## Hardware

Bitstream builds and passes timing; the physical gate (PIC24 bridge behavior
on this board: PS/2 traffic on P17/N15 from a USB keyboard on J13) is
recorded separately in PORTING_PLAN.md — see the gate notes there.
