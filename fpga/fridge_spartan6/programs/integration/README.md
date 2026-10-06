# Combined ROM, keyboard and GPU demo

Build/load from the board root with `make all timing` and `make load`.
Connect HDMI and the USB keyboard to Atlys J13 with JP11 open, as in the
keyboard milestone demo.

After startup the TEXT display shows:

```
FRIDGE: ROM + KEYBOARD + GPU

SPACE: TEXT/EGA   R: PALETTE

TYPE BELOW:
```

- Ordinary mapped keys echo on the row below the prompt. The echo wraps after
  38 cells. Releases are ignored.
- Space alternates TEXT (frame 1) and EGA (frame 0).
- Lowercase `r` increments the red channel of palette entry 15 by 0x40,
  wrapping at 0xFF; green/blue become 0x80/0x40. This recolors both TEXT and
  EGA without rewriting either framebuffer. Entry 15 initially is 0xFFD040.
- Typing while EGA is visible updates the hidden TEXT frame. Returning to
  TEXT shows the characters.
- The reset button restarts the program and restores its palette, echo cursor,
  text cells and ROM protocol state.

EGA shows the four ROM-paint patterns in the first 1024 framebuffer bytes:
ascending ramp, descending ramp, alternating 0xAA/0x55 and constant 0x42.
These occupy the top of the centered video window; the rest is black with
blue margins. The demo selects each segment through OUT 1 and streams its
256 bytes through IN 1. It polls IN 3 between ROM reads. During this short
startup phase it retains one pending keyboard event; normal polling/echo
begins after ROM loading finishes.

| Port | Board LED | Meaning |
| --- | --- | --- |
| led(0) | LD0 | CPU halted; should stay off |
| led(1) | LD1 | Oscillator heartbeat |
| led(2) | LD2 | CPU/video clock chain locked |
| led(3) | LD3 | Sticky ROM error; should stay off |
| led(4) | LD4 | Keyboard receive error or FIFO overflow; should stay off |
| led(5) | LD6 | Keyboard receive activity |

The existing keyboard mapping and its deferred Esc/arrow bridge investigation
are unchanged. This demo exercises the implemented system and directly boots
from bitstream-initialized CPU RAM; it is not the deferred flash/interrupt
boot-loader path.
