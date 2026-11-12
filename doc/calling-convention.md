# CPM calling convention (Fridge)

Status: consolidated target ABI, 2026-11-10. Source-language rules are defined
in [cpm-language-spec.md](cpm-language-spec.md), especially §§5, 8, and 9.
This stack-only ABI replaces the older small-results-in-HL/hidden-output-pointer
scheme. Implementation is still pending; this document is not a claim that the
current compiler emits these sequences.

## Values and stack direction

The stack descends, with no frame pointer register. Every argument, local, and
result is addressed relative to SP with compile-time offsets adjusted for live
temporaries. A slot occupies exactly `sizeof(type)` bytes, without padding or
alignment. Values larger than one byte are big-endian: the high byte is at the
lower address, matching Fridge `PUSH`/`POP` and `CALL`/`RET`.

Every non-void CPM result uses a caller-reserved stack slot, including one-byte
values, two-byte values, and aggregates. Void has no result slot. Arguments are
passed by value with the complete recursive type preserved. Copying a pointer
or string value copies its two-byte address; copying an aggregate copies its
representation, including those addresses.

The target header currently leaves `FRIDGE_ASCENDING_STACK` undefined. A future
ascending-stack target would need a separately specified layout and offsets;
these formulas are for the descending convention.

## Frame layout

From higher addresses (reserved first) to lower addresses:

```
[return buffer: R bytes]    reserved by caller, omitted for void
[arg1]                     argument layout corresponds to left-to-right pushes
[arg2]
...
[argN]
[return PC: 2 bytes]        pushed by CALL
[locals: L bytes]          reserved by callee <- SP at body entry
[temporaries: d bytes]     below the frame   <- current SP
```

Let `R = sizeof(return type)`, `A = sum(sizeof(arg[i]))`, and `L` be the
function's reserved local storage. The callee needs no saved SP/frame-pointer
slot: it tracks temporary depth `d` and knows `L` at compile time.

## Call sequence and evaluation

A call may reserve its complete argument area before evaluating operands, then
write each argument into its assigned slot. This produces the same final layout
as left-to-right pushes while preserving left-to-right evaluation across nested
calls. Each argument is snapshotted when evaluated: a later argument may modify
its source, so aggregate copies cannot all be deferred until the end.

```
caller:  SP -= R                 ; omit for void
         SP -= A                 ; reserve final argument slots
         evaluate arg1 .. argN left to right, writing each value to its slot
         CALL f                  ; pushes two-byte return PC
callee:  SP -= L                 ; reserve local frame
         ... body, tracking temporary depth d ...
         store result -> [SP + d + L + A + 2]  ; omit for void
         SP += d                 ; release any remaining expression temporaries
         SP += L                 ; return PC is now at SP
         RET
caller:  SP += A                 ; result slot is now at SP
         consume result, or retain it as an enclosing expression temporary
```

A nested call's own temporary slots must not interleave the final argument
layout. Registers may hold intermediate values internally, but arguments and
results cross the CPM call boundary in their stack slots. The callee releases
locals/temporaries; the caller removes arguments. Result storage remains live
until the enclosing expression consumes it. Every branch join, loop backedge,
and return/break/continue cleanup path must have the expected temporary depth.

## Addressing inside the callee

| Item | Address from current SP |
|---|---|
| local at byte offset `o` | `SP + d + o` |
| return PC | `SP + d + L` |
| argument `i` (1-based) | `SP + d + L + 2 + sum(sizeof(arg[j]) for j > i)` |
| return buffer | `SP + d + L + 2 + A` |

The argument formula handles mixed byte, word, and aggregate arguments. With
all two-byte arguments it reduces to `SP + d + L + 2 + 2*(N-i)`.

For example, a function returning a five-byte `Packet`, with arguments
`uint8 tag; Packet value; uint16 count`, has `R=5` and `A=8`. If `L=3`, the
layout relative to body-entry SP (`d=0`) is:

| Storage | Byte offsets |
|---|---|
| locals | 0–2 |
| return PC | 3–4 |
| count | 5–6 |
| value | 7–11 |
| tag | 12 |
| result | 13–17 |

If two temporary bytes are subsequently pushed, add two to every offset in
this table when addressing from the new current SP.

## Blocks, storage, and lifetime

Locals from nested blocks can share one reserved function frame. Storage
reservation and name visibility are independent: names become visible at their
declarations within their blocks; initializers execute only when reached and
execute again on subsequent loop iterations. Entering/leaving a block requires
no extra SP adjustment for an already reserved frame, but its initialization
statements still emit code.

Frame-resident character buffers for local string literal initializers add their
character bytes plus terminator to `L`, separately from the two-byte string slot.
String copies share the referenced characters. The final placement/lifetime and
escape rules for such buffers are still pending in language decision D4.

Slot reuse between disjoint scopes is optional. A correct first implementation
can reserve separate slots. Reuse must respect the eventual address-lifetime
rule and must not overwrite storage that remains validly reachable.

## Startup and external interfaces

Under the existing loader contract, the executable begins at `0x0200` with
SP set to `0xFFFF`. The loader jumps to the image rather than calling a function.
The compiler's entry stub calls `void global.main()` and jumps to `0x0000` after
it returns. Static data and other function bodies must not be entered through
fallthrough. No exit-code byte is assigned by this ABI.

An imported raw routine may use a register ABI. An emitted CPM wrapper loads
its stack arguments into the documented registers, invokes the routine through
recognized intrinsics, and marshals its result back to a CPM stack result slot.
Intrinsic clobbers and external binary placement must be specified explicitly.

## Remaining ABI decisions

Language decision D8 records register/flag preservation, intrinsic signatures
and effects, raw binary placement/relocation, unresolved prototypes, and the
Panic helper/trap mechanism. Void fallthrough uses the ordinary return sequence.
A reachable end in a non-void function is a compile error (language spec §7). No preservation promise
is implied by the examples. A simple candidate is caller-clobbered general
registers/flags with SP restored by the complete call sequence; this is a
proposal, not yet an adopted rule.

## Fridge implementation

Fridge already has everything this convention needs. The relevant instructions are `CALL`/`RET`, `PUSH`/`POP` of register pairs, `DAD_SP` (HL += SP), `HLSP` (HL = SP), `SPHL` (SP = HL), `DAI` (HL += immediate DWORD), `XTHL`, and loads/stores through `M` = memory[HL].

| Step | Fridge code |
|------|-------------|
| Reserve a two-byte return buffer | `DCX_SP` / `DCX_SP` (or `PUSH` any pair); use the actual result size in general |
| Push an argument held in DE | `PUSH_DE` |
| Allocate L bytes of locals | `LXI_HL -L` / `DAD_SP` / `SPHL` (or `DCX_SP` repeated, for small L) |
| Address of slot at offset k | `LXI_HL k` / `DAD_SP` (or `HLSP` / `DAI k`) |
| Load 16-bit slot into DE | address in HL, then `MOV_DM` / `INX_HL` / `MOV_EM` |
| Store DE to 16-bit slot | address in HL, then `MOV_MD` / `INX_HL` / `MOV_ME` |
| Load or store 8-bit slot | address in HL, then `MOV_AM` or `MOV_MA` |
| Free locals and return | `LXI_HL L` / `DAD_SP` / `SPHL` / `RET` |
| Caller drops args | `POP_BC` once per 2-byte arg (or `LXI_HL A` / `DAD_SP` / `SPHL`) |
| Push a 1-byte value in A | `DCX_SP` / `HLSP` / `MOV_MA` |
| Pop a 1-byte value into A | `HLSP` / `MOV_AM` / `INX_SP` |
| Caller takes result | One byte: load and advance SP; two bytes: `POP_HL`; aggregate: copy its exact size; or retain the slot as a temporary |

Because of the big-endian order, the high byte of a DWORD slot is at offset k and the low byte at k+1.

Freeing locals with `SPHL` overwrites HL, so a result held in HL has to be stored into the return buffer first. The stored-result design handles that naturally.

## Optional ISA additions

These instructions are optional future work and are not required for this ABI.
`IR248`–`IR255` are placeholders; opcode 247 is also available only when
`FRIDGE_POSIT16_SUPPORT` is disabled, otherwise it is `PAM16C`. Candidates:

1. **`LHSP k` / `SHSP k`**: load or store HL at memory[SP+k, SP+k+1]. This turns the 5-instruction "compute address, then move two bytes" pattern into one instruction, the Fridge counterpart of the Z80's `LD L,(IX+d)` / `LD H,(IX+d+1)`.
2. **`ADSP k`**: SP += k (immediate, signed). This allocates and frees locals without clobbering HL.
3. **`RET n`**: pop PC, then SP += n, for callee-side argument cleanup.

## Background

- **8080:** no frame pointer and no indexed addressing. C compilers such as Small-C used this same SP-relative scheme with compile-time offsets, computing addresses with `LXI H,off / DAD SP`.
- **Z80:** IX and IY with `(IX+d)` addressing make a real frame pointer practical, at roughly 19 cycles per access.
