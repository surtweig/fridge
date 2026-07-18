# Fridge Assembly Language Reference

Fridge Assembly is the native assembly language for the Fridge fantasy 8-bit game console.
The canonical assembler is **falc** (`falc/`). Source files use the `.x2al` extension
(legacy) or `.falc` extension (current).

This document is based on the falc compiler source (`falc/FridgeAssemblyLanguageCompiler.cpp`)
and the emulator core (`fridgemulib/fridgemulib.c`).

---

## 1. Architecture Overview

| Property          | Value                                                |
|-------------------|------------------------------------------------------|
| CPU               | Intel 8080 ISA, extended with graphical instructions |
| Word size         | 8 bits (`FRIDGE_WORD`)                               |
| Address size      | 16 bits (`FRIDGE_RAM_ADDR`)                          |
| Endianness        | **Big-endian** (16-bit values stored high-byte first)|
| RAM               | 64 KiB (code, data, stack, video aperture)           |
| Executable origin | `0x0200` (`FRIDGE_EXECUTABLE_OFFSET`)                |
| Stack direction   | Ascending (`FRIDGE_ASCENDING_STACK`)                 |
| Clock             | 10 MHz (emulated)                                    |

### Registers

| 8-bit  | 16-bit pair | Notes                  |
|--------|-------------|------------------------|
| `A`, `F` | `AF`     | Accumulator + Flags    |
| `B`, `C` | `BC`     | General purpose        |
| `D`, `E` | `DE`     | General purpose        |
| `H`, `L` | `HL`     | Memory pointer (M)     |
| -        | `SP`      | Stack pointer          |
| -        | `PC`      | Program counter        |

`M` is a pseudo-register that refers to `memory[HL]`.

### Flags (register `F`)

| Bit | Mask  | Name    |
|-----|-------|---------|
| 7   | 0x80  | Sign    |
| 6   | 0x40  | Zero    |
| 5   | 0x20  | Panic   |
| 4   | 0x10  | Aux     |
| 2   | 0x04  | Parity  |
| 0   | 0x01  | Carry   |

---

## 2. Source File Structure

### File extensions

- `.x2al` — legacy extension, still fully supported
- `.falc` — current extension for falc-assembled source

### General syntax

```
// Comments start with two forward slashes

instruction arg1, arg2

directive arg1 arg2
```

- Lines are separated by newlines.
- Operands and arguments are separated by spaces, tabs, commas, or semicolons.
- String literals use double quotes: `"Hello, World!"`.
- Character literals use single quotes: `'A'` (value is the ASCII code).
- Case-sensitive: instruction mnemonics and register names are **uppercase**.

---

## 3. Directives

Directives control the assembly process and are not emitted as instructions.

### `include "filename"`

Includes another assembly file. The assembler searches the source file's
directory first, then each include folder, and finally `../x2al_std/`.
Duplicate includes of the same file are silently skipped.

```asm
include "stdapp.inc"
include "vtext.inc"
```

### `alias name value`

Defines a textual substitution. During the dealias pass, every occurrence of
`name` is replaced with `value` before further processing.

```asm
alias EXECUTABLE_OFFSET 0x0200
alias VTEXT_COLUMNS 40
```

Built-in aliases are automatically defined for every instruction opcode
(prefixed with `#`) and all `FRIDGE_*` constants from `fridge.h`:

```asm
MVI A, #JMP         // #JMP resolves to the numeric opcode of JMP
OUT FRIDGE_DEV_ROM_ID   // resolves to 0x02
```

### `offset addr`

Sets the base address where the program will be loaded. Defaults to 0.

```asm
offset 0x0000   // boot loader starts at address 0
offset 0x0200   // user executable starts at the standard offset
```

### `entry name`

Declares a named label at the current address. Entries can be referenced as
jump/call targets by name.

```asm
entry loop
    // ... instructions ...
    JMP loop
```

### `main name`

Declares a named entry that serves as the program entry point. Exactly one
`main` should be declared. If `main` is not at `offset`, the assembler
automatically inserts a `JMP main` instruction at the program start (+3 bytes
to all addresses).

```asm
main start
    VMODE, 1
    // ...
```

### `subroutine name` / `endsub`

Declares a **subroutine**. Subroutines differ from entries:
- Subroutines are intended to be entered via `CALL` and exited via `RET`.
- Entries are intended for `JMP` flow.
- The compiler enforces this by default: `CALL` to an entry or `JMP` to a
  subroutine produces an error (unless `unsafe_flow` is set).
- `endsub` is replaced with a `RET` instruction automatically.

```asm
subroutine mul_dc
    MVI E, 8
    // ...
    entry mul_dc.end
endsub
```

### `static name value`

Declares a static resource (byte array) embedded in the program binary.
The assembler places all static data at the end of the output, after a
`JMP 0x0000` guard (to prevent execution of data as code).

Values can be:
- A string literal: `static msg "Hello, World!"` (null-terminated)
- A byte (decimal, hex, or char): `static timer 0`
- A double-word hex: `static addr 0x1234` (2 bytes)

The resource's address can be referenced by name. Its size is available via
the automatic alias `<name>_SIZE`.

```asm
static HelloWorld "Hello, World!"
// HelloWorld is now a label pointing to the string's address
// HelloWorld_SIZE is an alias for the string's length
```

The `MEM_ORIGIN` alias resolves to the address just past all static resources
(useful for a heap pointer).

### `unsafe_flow`

Disables the compiler's safety checks that prevent `CALL` to entries and
`JMP` to subroutines. Strongly discouraged except in boot loader code.

```asm
unsafe_flow
// now CALL can target entries, JMP can target subroutines
```

---

## 4. Number Formats

| Format        | Example    | Width | Where used                    |
|---------------|------------|-------|-------------------------------|
| Decimal       | `100`      | 8-bit | `MVI`, `ADI`, `ANI`, etc.     |
| Hexadecimal   | `0x0055`   | 8-bit | Same as decimal               |
| Double hex    | `0x1234`   | 16-bit| `LXI`, `JMP`, `CALL`, `LDA`, etc. |
| Character     | `'A'`      | 8-bit | `MVI`, `ADI`, etc.            |

- Decimal numbers: 1–3 digits, parsed as `strtol(..., 10)`.
- Single hex: exactly 4 characters (`0x` + 2 hex digits), parsed as `strtol(..., 16)`.
- Double hex: exactly 6 characters (`0x` + 4 hex digits), parsed as `strtol(..., 16)`.
- Character: exactly 3 characters (`'` + 1 ASCII char + `'`), value is the character's ASCII code.

---

## 5. Instruction Set

Each instruction is 1 byte (opcode) plus 0–2 bytes of operands. Fridge extends
the classic 8080 ISA with video instructions and an optional PAM16 posit
coprocessor.

### 5.1 Data Transfer

#### MOV — register-to-register copy

```
MOV dst, src
```

`dst` and `src` are any of: `A, B, C, D, E, H, L, M`. Cannot both be `M`.

```
MOV A, B     ; A = B
MOV M, C     ; memory[HL] = C
MOV D, H     ; D = H
```

#### MVI — load immediate to register

```
MVI reg, value
```

Loads an 8-bit immediate into a register or memory[HL].

```
MVI A, 0x55
MVI B, 100
MVI M, 'X'
```

#### LXI — load immediate to register pair

```
LXI pair, value
```

Loads a 16-bit immediate into `BC, DE, HL` or `SP`.

```
LXI BC, 0x1234      ; B=0x12, C=0x34
LXI SP, 0xFFFF      ; initialize stack pointer
LXI HL, HelloWorld  ; load address of a static/entry
```

#### LDA / STA — direct memory load/store

```
LDA addr    ; A = memory[addr]
STA addr    ; memory[addr] = A
```

#### LHLD / SHLD — direct 16-bit load/store

```
LHLD addr   ; L = memory[addr], H = memory[addr+1]
SHLD addr   ; memory[addr] = L, memory[addr+1] = H
```

#### LDAX / STAX — indirect load/store via register pair

```
LDAX pair   ; A = memory[pair]   (pair = BC, DE, or HL)
STAX pair   ; memory[pair] = A
```

#### XCNG — exchange DE and HL

```
XCNG        ; swaps DE <-> HL
```

### 5.2 Arithmetic

#### ADD / ADI — add (without carry)

```
ADD src     ; A = A + src    (src = A, B, C, D, E, H, L, M)
ADI value   ; A = A + value
```

#### ADC / ACI — add with carry

```
ADC src     ; A = A + src + Carry
ACI value   ; A = A + value + Carry
```

#### SUB / SUI — subtract (without borrow)

```
SUB src     ; A = A - src
SUI value   ; A = A - value
```

#### SBB / SBI — subtract with borrow

```
SBB src     ; A = A - src - Carry
SBI value   ; A = A - value - Carry
```

#### INR / DCR — increment/decrement

```
INR reg     ; reg = reg + 1    (reg = A, B, C, D, E, H, L, M)
DCR reg     ; reg = reg - 1
```

#### INX / DCX — increment/decrement pair

```
INX pair    ; pair = pair + 1    (pair = BC, DE, HL, SP)
DCX pair    ; pair = pair - 1
```

#### DAD — double add (HL += pair)

```
DAD pair    ; HL = HL + pair    (pair = BC, DE, HL, SP)
```

#### DAI — decimal adjust immediate (double-word)

```
DAI value   ; 16-bit immediate; used for adjusting HL to a string position
```

### 5.3 Logical

#### ANA / ANI — bitwise AND

```
ANA src     ; A = A & src
ANI value   ; A = A & value
```

#### ORA / ORI — bitwise OR

```
ORA src     ; A = A | src
ORI value   ; A = A | value
```

#### XRA / XRI — bitwise XOR

```
XRA src     ; A = A ^ src
XRI value   ; A = A ^ value
```

#### CMP / CPI — compare (subtract without storing result)

```
CMP src     ; sets flags based on A - src
CPI value   ; sets flags based on A - value
```

### 5.4 Rotate and Bit

```
RLC     ; rotate A left through carry
RRC     ; rotate A right through carry
RAL     ; rotate A left
RAR     ; rotate A right
CMA     ; complement A (bitwise NOT)
CMC     ; complement carry flag
STC     ; set carry flag
RTC     ; clear carry flag (Fridge-specific)
```

### 5.5 Control Flow

#### JMP / conditional jumps

```
JMP addr    ; unconditional jump
JNZ addr    ; jump if not zero (Z=0)
JZ addr     ; jump if zero (Z=1)
JNC addr    ; jump if no carry (C=0)
JC addr     ; jump if carry (C=1)
JPO addr    ; jump if parity odd (P=0)
JPE addr    ; jump if parity even (P=1)
JP addr     ; jump if positive (S=0)
JM addr     ; jump if minus (S=1)
```

#### CALL / conditional calls

Push return address to stack, then jump.

```
CALL addr   ; unconditional call
CNZ addr    ; call if not zero
CZ addr     ; call if zero
CNC addr    ; call if no carry
CC addr     ; call if carry
CPO addr    ; call if parity odd
CPE addr    ; call if parity even
CP addr     ; call if positive
CM addr     ; call if minus
```

#### RET / conditional returns

Pop return address from stack, then jump.

```
RET         ; unconditional return
RNZ         ; return if not zero
RZ          ; return if zero
RNC         ; return if no carry
RC          ; return if carry
RPO         ; return if parity odd
RPE         ; return if parity even
RP          ; return if positive
RM          ; return if minus
```

#### Other control flow

```
PCHL    ; PC = HL (jump to address in HL)
```

### 5.6 Stack Operations

```
PUSH pair   ; push pair onto stack (pair = AF, BC, DE, HL)
POP pair    ; pop pair from stack
XTHL        ; exchange HL with top of stack
SPHL        ; SP = HL
HLSP        ; HL = SP (Fridge-specific)
```

In **ascending stack** mode, `SP` grows upward. `PUSH` increments SP by 2 and
stores the pair; `POP` reads the pair and decrements SP by 2.

### 5.7 I/O and System

```
IN port     ; A = input(port)       (Fridge: IIN in the ISA enum)
OUT port    ; output(port) = A      (Fridge: IOUT in the ISA enum)
HLT         ; halt CPU until next interrupt
EI          ; enable interrupts
DI          ; disable interrupts
```

I/O ports are device IDs:

| Port | Device               | Constant alias           |
|------|----------------------|--------------------------|
| 0x01 | ROM reset            | `FRIDGE_DEV_ROM_RESET_ID`|
| 0x02 | ROM data/stream      | `FRIDGE_DEV_ROM_ID`      |
| 0x03 | Keyboard controller  | `FRIDGE_DEV_KEYBOARD_ID` |

### 5.8 Video Instructions

Fridge has a dedicated GPU with dual framebuffers and sprite memory. Video
instructions communicate through the `gpubus` (4-byte bus between CPU and GPU).
Parameters are placed in registers **before** the video instruction is executed.

#### VPRE — present video frame

Swaps active and visible buffers according to the swap mode in A, with buffer
offset in HL.

```
MVI A, FRIDGE_GPU_VIDEO_SWAP_AUTO    ; A = 1: auto-swap buffers
VPRE
```

| A | Mode          | Behavior                                  |
|---|---------------|--------------------------------------------|
| 0 | SWAP_NONE     | Leaves buffer indices unchanged            |
| 1 | SWAP_AUTO     | Alternates (vis=0,act=1) <-> (vis=1,act=0)|
| 2 | SWAP_MANUAL_00| Sets visible=0, active=0                   |
| 3 | SWAP_MANUAL_01| Sets visible=0, active=1                   |
| 4 | SWAP_MANUAL_10| Sets visible=1, active=0                   |
| 5 | SWAP_MANUAL_11| Sets visible=1, active=1                   |

#### VMODE — set video mode

```
MVI A, 0    ; 0 = EGA graphics mode
VMODE
MVI A, 1    ; 1 = text mode
VMODE
```

#### VPAL — update palette color

A = color index (0–15), B = red (4-bit), C = green (4-bit), D = blue (4-bit).
RGB values are 4-bit (0–15), producing 12-bit RGB4096 color.

```
MVI A, 0       ; color index 0
MVI B, 0x0F    ; max red
MVI C, 0x00    ; no green
MVI D, 0x00    ; no blue
VPAL             ; set color 0 to bright red
```

#### Back Buffer Instructions

The back buffer is the currently-active framebuffer (not the visible one).

```
VFSA        ; store A (byte) on back buffer at address HL
VFSI w1, w2 ; store two immediate bytes at HL, then HL += 2
VFSAC       ; store A as a color byte (two 4-bit pixels) at position HL
VFLA        ; load from back buffer at address HL into A
VFLAC       ; load a color byte from back buffer at position HL into A
VS2F        ; copy one byte from sprite memory[HL] to back buffer[BC]
```

#### Sprite Memory Instructions

```
VSSA        ; store A into sprite memory at HL
VSSI w1, w2 ; store two immediate bytes into sprite memory at HL, HL += 2
VSLA        ; load from sprite memory at HL into A
```

#### Sprite Control

```
VSS         ; set sprite: A=index, B=width, C=height, HL=address
VSD         ; draw sprite: A=index, B=mode, HL=position (x=L, y=H)
```

Sprite draw modes (in register B):

| Mode | Name        |
|------|-------------|
| 0    | Invisible   |
| 1    | Opaque      |
| 2    | Transparent0|
| 3    | Additive    |
| 4    | Subtractive |
| 5    | Bitwise AND |
| 6    | Bitwise OR  |
| 7    | Bitwise XOR |

### 5.9 PAM16C — Posit Arithmetic Coprocessor (optional)

When `FRIDGE_POSIT16_SUPPORT` is defined, opcode 247 dispatches to the PAM16
posit coprocessor. The low nibble of A selects the command:

```
MVI A, FRIDGE_PAM16_PUSH    ; A = low nibble command
PAM16C
```

| Command | Value | Description                         |
|---------|-------|-------------------------------------|
| NOP     | 0     | No operation                        |
| RESET   | 1     | Reset PAM16, ES = (A >> 4) if > 0   |
| PUSH    | 2     | Push HL onto posit stack            |
| POP     | 3     | Pop from posit stack to HL          |
| ADD     | 4     | Add top two                         |
| SUB     | 5     | Subtract top two                    |
| MUL     | 6     | Multiply top two                    |
| DIV     | 7     | Divide top two                      |
| FMADD   | 8     | Fused multiply-add                  |
| PACK    | 9     | Pack sign(B), regime(C), exp(DE), fraction(HL) |
| UNPACK  | 10    | Unpack to sign(B), regime(C), exp(DE), fraction(HL) |

---

## 6. Labels, Entries, and Subroutines

### Naming rules

Entry and subroutine names can contain letters, digits, periods (`.`) and
underscores. Periods are commonly used for namespacing:

```asm
entry myapp.start
subroutine myapp.calculate
entry myapp.calculate.loop
```

### Flow control discipline

The assembler distinguishes between **entries** (jump targets) and
**subroutines** (call targets):

| From  | To entry   | To subroutine |
|-------|------------|---------------|
| `JMP` | Allowed    | Error*        |
| `CALL`| Error*     | Allowed       |
| `RET` | -          | Allowed**     |

\* Allowed with `unsafe_flow`
\** `RET` outside a subroutine is an error unless `unsafe_flow` is set

### How addresses work

When `main` is not at `offset`, falc injects a 3-byte `JMP main` at the
beginning of the binary and shifts all entry/subroutine addresses by +3.

Static resources are placed at the end of the binary after a 3-byte
`JMP 0x0000` guard.

---

## 7. Interrupts

Fridge has three interrupt vectors at fixed addresses in low memory:

| Vector | Address | Source              |
|--------|---------|---------------------|
| IRQ 0  | 0x0004  | System timer        |
| IRQ 1  | 0x0007  | Keyboard press      |
| IRQ 2  | 0x000A  | Keyboard release    |

When an interrupt fires, the CPU reads a 2-byte vector from the interrupt source
and executes it. In the boot loader, these slots typically contain:

```asm
; at offset 0x0000
main BootLoader
    JMP BootLoader.start
    NOP                    ; 0x0003: BOOT_SECTION_INDEX_ADDRESS
    
    ; System timer IRQ (0x0004)
    CALL 0x0000            ; or RET for no handler
    
    ; Keyboard IRQ (0x0007)
    CALL 0x0000            ; 3 bytes per slot (call + 2-byte addr)
```

To handle interrupts, place your handler address at the corresponding vector
and enable interrupts with `EI`. Interrupt handlers must save/restore registers
and end with `RET`.

```asm
subroutine MyTimerIRQ
    DI
    PUSH AF
    PUSH HL
    ; ... handler logic ...
    POP HL
    POP AF
    EI
endsub
```

---

## 8. Standard Library (`x2al_std/`)

### `stdapp.inc`

Defines standard addresses and I/O port aliases:

```
alias EXECUTABLE_OFFSET          0x0100   (legacy) / 0x0200 (fridge.h)
alias BOOT_SECTION_INDEX_ADDRESS 0x0003
alias IRQ_SYS_TIMER              0x0004
alias IRQ_SYS_KEYBOARD           0x0007
alias XCM2_ROM_DEVICE_ID         0x01
alias XCM2_ROM_DEVICE_RESET_ID   0x02
alias XCM2_ROM_DEVICE_MODE_LOAD  0x02
```

Note: newer builds use `FRIDGE_*` aliases automatically defined by falc from
`fridge.h` constants.

### `vtext.inc`

Text mode helpers. Depends on `arithm.inc`.

- `vtext_putstr`: writes a null-terminated string at position (B=column, C=row).
  HL = string address.
- `VTEXT_FORE_COLOR` and `VTEXT_BACK_COLOR`: static bytes controlling text palette.

Text cells are 2 bytes each: ASCII code followed by a color byte
(foreground in low nibble, background in high nibble).

### `arithm.inc`

- `mul_dc`: unsigned 8-bit multiplication. D = multiplicand, C = multiplier.
  Result in HL.

### `string.inc`

- `string_hex`: writes 2 hex digits of B to string at HL, advances HL by 2.
- `string_dhex`: writes 4 hex digits of BC to string at HL, advances HL by 4.
- `string_hex_digit`: converts a digit value in A to ASCII char code.

### `posit.inc`

Posit16 support wrappers (requires `FRIDGE_POSIT16_SUPPORT`):

- `POSIT_RESET`: calls PAM16 RESET command with ES from A (shifts A left 4,
  ORs with `FRIDGE_PAM16_RESET`, then `PAM16C`).

---

## 9. Compilation with falc

### Build falc

```bash
cmake -S falc -B build-falc && cmake --build build-falc -j
```

### Compile an assembly file

```bash
./build-falc/falc source.x2al output.bin
```

Options:
- `-vhdl`: also emit a VHDL ROM initializer file (`output.bin.vhd`)
- Include folders can be added to the search path

### Compilation pipeline

1. **Preprocess**: read source file, expand `include` directives, parse each
   line into words, build the initial `ParsedLine` list.
2. **Read resources**: parse `static` declarations, allocate data, set up
   `<name>_SIZE` aliases.
3. **Dealias**: apply `alias` substitutions (including built-in `#OPCODE` and
   `FRIDGE_*` aliases) to every word in every line.
4. **Address markup**: walk the parsed lines, assign addresses, resolve
   `entry`/`subroutine` names to addresses, validate flow control, replace
   label references with hex addresses.
5. **Code generation**: emit `JMP main` prefix if needed, match each
   instruction mnemonic+operands against the signature table, emit opcode +
   operands, append static resources with a `JMP 0x0000` guard.

### Output format

The output is a flat binary blob of raw Fridge machine code bytes, suitable
for loading directly into emulator RAM at the specified offset.

---

## 10. Memory Layout

```
0x0000  +-----------------------+
        | Boot loader           |  (loaded at power-on)
        | (size varies)         |
        |                       |
        | IRQ vectors:          |
        |   0x0004: timer IRQ   |
        |   0x0007: kbd press   |
        |   0x000A: kbd release |
0x0200  +-----------------------+
        | User executable       |  (FRIDGE_EXECUTABLE_OFFSET)
        |   JMP main (3 bytes)  |  -- if main != offset
        |   Code                |
        |   ...                 |
        |   JMP 0x0000 (3 bytes)|  -- if static data present
        |   Static resources    |
MEM_ORIGIN +-----------------+  -- alias for heap start
        |   Stack (grows up)   |  -- SP typically starts at 0xFFFF
0xFFFF  +-----------------------+
```

Note: in ascending stack mode, the stack grows from lower to higher addresses.
The initial SP in the emulator test bench is `0x1000`; the boot loader
initializes it to `0xFFFF`.

---

## 11. Video Memory Layout

### EGA mode (240x180, 4-bit color)

Each byte stores two horizontally adjacent pixels (4 bits per pixel).
The framebuffer is `FRIDGE_GPU_FRAME_BUFFER_SIZE` = 21,600 bytes.
Two framebuffers (A and B) support double-buffering.

### Text mode (40 columns x 22 rows)

Each character cell is 2 bytes:
- Byte 0: ASCII character code (0–255)
- Byte 1: color info (`(background << 4) | foreground`)

Text mode uses a built-in 6x8 glyph bitmap with 256 characters including
semigraphics and box-drawing symbols.

---

## 12. Big-Endian Convention

Fridge is **big-endian** — 16-bit values are stored high byte first:

```asm
LXI HL, 0x1234
; H = 0x12, L = 0x34

LDA 0x0100
; loads byte at address 0x0100 (the high byte) into A

STA 0x0100
; stores A at address 0x0100

LHLD 0x0100
; L = memory[0x0100], H = memory[0x0101]
```

This applies to all 16-bit operations: `LXI`, `LDA`, `STA`, `LHLD`, `SHLD`,
`JMP`, `CALL`, and all memory-mapped addresses. It is the most significant
divergence from the real Intel 8080 (which is little-endian).

---

## 13. Known Issues

- The **parity flag** implementation does not match the i8080 specification
  (uses even/odd instead of even-parity-of-low-byte).
- The `#OPCODE` alias form (e.g., `#JMP`) is specific to falc; the legacy
  x2al assembler handles it differently.
- `VFSI` and `VSSI` are the only video instructions that take immediate
  operands in the assembly syntax. Most video instructions read their
  parameters from registers set up before the instruction executes.
