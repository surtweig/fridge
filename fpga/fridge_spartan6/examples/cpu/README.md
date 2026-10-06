# CPU/RAM Smoke Test

Brings up the Fridge CPU with BRAM on the Digilent Atlys (Spartan-6
`xc6slx45-3-csg324`). Exercises instruction fetch, register loads,
arithmetic, BRAM store/load, comparison, conditional branching, and
halt using a minimal test program.

## Test Program

| Address | Bytes | Instruction | Effect |
|---------|-------|-------------|--------|
| 0x0000 | 39 12 | MVI_A, 0x12 | A = 0x12 |
| 0x0002 | 3A 34 | MVI_B, 0x34 | B = 0x34 |
| 0x0004 | 3B 56 | MVI_C, 0x56 | C = 0x56 |
| 0x0006 | 51 | ADD_B | A = 0x46 |
| 0x0007 | 52 | ADD_C | A = 0x9C |
| 0x0008 | 46 00 80 | STA 0x0080 | mem[0x0080] = 0x9C |
| 0x000B | 39 00 | MVI_A, 0x00 | A = 0 |
| 0x000D | 45 00 80 | LDA 0x0080 | A = 0x9C |
| 0x0010 | 3D 9C | MVI_E, 0x9C | E = 0x9C |
| 0x0012 | B0 | CMP_E | Zero flag set (A == E) |
| 0x0013 | BF 00 19 | JZ 0x0019 | jump to HLT |
| 0x0016 | BD 00 16 | JMP 0x0016 | fail: infinite loop |
| 0x0019 | E6 | HLT | pass: halt (PC = 0x001A) |

## LED Indicators

| LED | Signal | Meaning |
|-----|--------|---------|
| 0 | HALTED | CPU halted (1 = pass) |
| 1 | heartbeat | ~2 Hz clock activity blink |
| 2 | DEBUG_STATE(0) | CPU state LSB (1 when halted) |
| 3 | DEBUG_STATE(1) | CPU state bit 1 (0 when halted) |

Pass condition: LED 0 and LED 2 on, LED 3 off, LED 1 blinking.

## Simulation

```
make test
```

Checks: CPU halts, PC = 0x001A, store of 0x9C to 0x0080, load from
0x0080 observed.

## Build

```
make          # synthesize, map, place/route, bitstream
make timing   # post-route timing report
make load     # program Atlys via djtgcfg
```

## Porting Notes

The Fridge CPU was written for the Terasic DE0-CV (Altera Cyclone V).
Changes made for Spartan-6 / ISE:

- **FridgeRAM**: registered read address with combinational data
  output (same as original). Xilinx BRAM inference works with the
  registered-address pattern; 32 RAMB16BWERs used for 64 KB.
- **FridgeCPU**: minimal changes only:
  - Process variables initialized (`buf_nextPC`, `buf_nextSP`,
    `buf_nextState`, `buf_currentIRCode`) for deterministic simulation.
  - `memAddrBuffer`, `memWriteBuffer` signal initial values added.
  - Combinational output assignments (`RAM_*`, `DEVICE_*`, `DEBUG_*`)
    moved outside the clocked process to concurrent assignments,
    fixing a simulation/synthesis mismatch where signal assignments
    inside the process body only updated at `CLK_MAIN` events.
  - `XCM2_LOW_WORD` return uses intermediate variable to match
    `unsigned(0 to 7)` index range (XST strictness).
- **Clock**: DCM_CLKGEN divides 100 MHz to 10 MHz (2/20).
  CPU uses dual-edge scheme (rising = state register, falling =
  decode/execute), so effective throughput is 20 MIPS at 10 MHz.
- **Reset**: active-low T15 button, polarity corrected from step 1
  lessons. DCM lock-loss asserts reset asynchronously.
