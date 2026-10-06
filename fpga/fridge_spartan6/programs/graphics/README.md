# Framebuffer access and sprites demo

Build and load from `fpga/fridge_spartan6/`:

```bash
make PROGRAM=graphics all timing
make PROGRAM=graphics load
```

Bitstream: `.local/build/graphics/fridge.bit`.
The default `PROGRAM=integration` demo remains available separately.

At boot, the program checks immediate framebuffer writes and HL advancement,
byte/pixel readback, neighboring-pixel preservation, sprite readback, the last
sprite-memory address, and sprite-to-frame copy. Successful startup shows
**FRAMEBUFFER + SPRITES: PASS** and the controls in TEXT. A failed self-test
shows **GPU SELF TEST FAILED** and halts the CPU (HALTED LED).

- **Space:** switch TEXT/EGA.
- **W/A/S/D:** move sprite 0 up/left/down/right, one pixel per key press/repeat.
- **M:** cycle sprite 0 through modes 0..7, starting at transparent-zero (2).
- **Reset:** rerun initialization and self-tests.

EGA shows a grey background, a red diagonal, and a movable black/white patterned
sprite initially at (112,56). Seven fixed patterned sprites at y=104 show modes
1..7 from left to right: opaque, transparent-zero, additive, subtractive,
AND, OR and XOR. Move the sprite across the diagonal and to the right/bottom
edges to inspect blending and clipping. In mode 0 it is invisible; press M
again to restore it. TEXT ignores sprites.

For the board gate, confirm the PASS screen, both video modes, movement, mode
cycling, edge clipping and reset. This demo's hardware gate is pending.
Software behavior and the known differences from the emulator are specified
in [GPU.md](../../GPU.md).

Verified in simulation: startup self-tests, pixel-exact help and EGA scenes,
Space/A/M PS/2 controls, return to TEXT and warm reset. The graphics build
passes all post-route constraints with zero setup/hold errors; resources and
reports are recorded in [GPU.md](../../GPU.md). CPU remains 10 MHz.
