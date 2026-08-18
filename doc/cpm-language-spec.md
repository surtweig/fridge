# Frion (CPM) Language Specification — Draft

> Status: design in progress. Source of truth is the parser/codegen in
> `emulator/cpm/`. This document is a *target* spec, not a description of what
> the current code already implements. Where the two disagree, the intent is to
> bring the code in line with this document.

Frion is a small statically-typed language targeting the Fridge 8080 ISA. It is
Lisp-prefix in its expression layer and C-ish in its declaration layer. Code is
organized into **namespaces**; a program emits Fridge Assembly via the `falc`
assembler.

## 1. Lexical structure

- **Whitespace**: space, tab, CR, LF — separators only, never significant.
- **Comments**:
  - Line: `// …` to end of line.
  - Block: `/* … */`, may span lines, may not nest.
- **Literals**:
  - Decimal: `[0-9]+`                e.g. `42`
  - Hex:     `0x[0-9A-Fa-f]+`        e.g. `0x00A1`
  - Char:    single-quoted single character `'x'`. No escape sequences yet.
  - String:  double-quoted `"Hello, World!"`. No escapes yet. A string literal
    has type `string` (= `(char ptr)`); see §3.
- **Identifiers**: `[A-Za-z_][A-Za-z0-9_]*`. The `.` (CPM_SUBSCRIPT_DELIM)
  separates a namespace qualifier or struct-field access: `X2AL.Zuzu`,
  `myArray.data`.
- **Punctuation**:
  - `(` `)` — block and expression grouping
  - `;`    — statement / argument separator
  - `,`    — element separator inside array literals
  - `.`    — subscript / field access
  - `'`    — char delimiter
  - `"`    — string delimiter
- **Sigils**: none. `#`, `^`, `&` are **not** part of the language.
  (The parser's `PtrPrefix`/`RefPrefix`/`CPM_REF` are legacy and to be removed.)

## 2. Primitive types

| Type     | Size     | Range / notes                          |
|----------|----------|----------------------------------------|
| `void`   | 0        | function return only; no variables     |
| `bool`   | 1 byte   | `0` false, nonzero true                |
| `char`   | 1 byte   | ASCII; literal form `"x"`              |
| `uint8`  | 1 byte   | 0..255                                 |
| `int8`   | 1 byte   | -128..127                              |
| `uint16` | 2 bytes  | 0..65535  (also: pointer width)        |
| `int16`  | 2 bytes  | -32768..32767                          |
| `string` | 2 bytes  | pointer to null-terminated static text |

All multi-byte values are **big-endian** in memory, matching the Fridge CPU.

## 3. Type expressions

A type is described by a recursive **type-expr**. In source it appears either
bare (for a primitive or struct name) or parenthesized for compound forms.

```
type-expr ::=
    primitive-name
  | struct-name                       // qualified: NS.Name
  | "(" type-expr "ptr" ")"
  | "(" type-expr "array" expr ")"
```

Examples:
```
uint8
(uint8 ptr)
(uint8 array 10)
((uint8 array 10) array 20)           // 20-element array of 10-element arrays
((uint8 ptr) array 10)                // array of 10 pointers
(uint8 array intArraySize)            // size may be a const expr
```

`ptr` may be applied to any type-expr. `array n` requires `n` to be a compile-
time constant expression (a `const` symbol or literal). The total static size
of any type-expr must not exceed 65535 bytes.

`string` is a built-in alias for `(char ptr)` whose literal form is a
null-terminated block in static storage. It is not an array; `sizeof(string)`
is 2 bytes (the pointer width).

Every `"…"` string literal is therefore a **static compile-time resource**: the
text is laid down in the namespace's static buffer with a trailing `0`, and the
literal's value is the address of its first character. Identical literals are
deduplicated within a namespace. A `string` variable is just a 2-byte pointer;
it may be reassigned to point at another string literal, at a `(char array N)`
in writable memory (via `addr`), or at any `(char ptr)`.

There is **no implicit array↔pointer decay** and no implicit null termination
for arrays. A `(char array N)` initialized with an array literal `('a','b','c')`
holds exactly N characters — no trailing `0` is added. To pass a char array
to a `string`-taking function, take its address explicitly (`(addr buf)`) and
ensure it is null-terminated manually if the callee requires it.

Nesting is unrestricted; both `(T ptr ptr)` and `(T array 10 array 20)` are
grammatically legal and read as "pointer to pointer to T" and
"array of 10 of array of 20 of T" respectively. Avoiding such constructions
is a style choice, not a language rule.

## 4. Declarations

A declaration binds a name to a storage location. Its grammatical slot is the
same in namespaces, struct bodies, function argument lists, and function
bodies; the surrounding scope determines lifetime.

### 4.1 Variable declaration

```
decl ::= type-expr name [initializer] ";"
initializer ::= "(" init-expr ")"
              | "(" init-list ")"
init-list ::= init-elem ("," init-elem)*
init-elem ::= init-expr | initializer        // nested, for array-of-array
```

In any scope a bare type-expr (a single ID node or a parenthesized compound)
is followed by a name and an optional initializer. The legacy postfix form
`uint8 x array 10` is **removed**; use `(uint8 array 10) x`.

An initializer list is **comma-separated** (`,`). It is used for array and
array-of-struct initialization. Each element may itself be an initializer
(`(…)`-parenthesized) for nesting. The legacy `;`-separated list form in
the old test file is removed.

Examples:
```
uint8 x;
uint16 y (+ 54 12);
(uint8 array intArraySize) myArr (0, (+ 1 1), 2, 3, 4);
(char array 3) buf ('a', 'b', 'c');
((uint8 array 2) array 2) grid ((1, 2), (3, 4));
(uint8 ptr) px (addr x);
(uint8 ptr) px (addr x);
```

### 4.2 Storage class prefixes

A declaration may be prefixed by exactly one storage class keyword:

| Prefix    | Meaning                                                          |
|-----------|------------------------------------------------------------------|
| `static`  | allocated in the namespace's static buffer, lives for program    |
|           | lifetime. Zero-initialized if no initializer given.             |
| `const`   | like `static` but read-only; value folded at compile time when   |
|           | possible. Required for sizes used in array type-exprs.          |
| (none)    | local: stack-allocated on function entry, deallocated on exit.  |

`imports` is a *namespace-level* external-symbol directive (§6), not a
storage class.

### 4.3 Struct and union

```
"struct" name "(" field-decl field-decl … ")" ";"
"union"  name "(" field-decl field-decl … ")" ";"
field-decl ::= type-expr name ";"
```

- Fields are laid out in declaration order; `union` overlays all fields at
  offset 0.
- A struct's size is the sum of its field sizes. **No padding is inserted.**
  Field offsets are the running sum of preceding field sizes, and the struct
  occupies that sum exactly. The Fridge ISA addresses memory one byte at a
  time and imposes no alignment constraints; padding would only inflate
  layout with no runtime benefit. (The compiler reserves the right to
  introduce padding later if a future target requires it; that would be a
  layout ABI change, not a source-visible one.)
- Struct types are registered in their owning namespace and may be referenced
  qualified (`NS.Name`) or unqualified if `using NS` is in scope.
- Self-referential structs are illegal (no tagged pointers). Mutually
  recursive structs are illegal at present; the existing "forward by re-decl"
  hack in `test.cpm` is removed.

### 4.4 Function

```
func-decl ::= type-expr name "(" arg-list ")" body
func-prot ::= type-expr name "(" arg-list ")" ";"
arg-list  ::= /* empty */
            | arg-decl (";" arg-decl)*
arg-decl  ::= type-expr name
body      ::= "(" statement … ")"
            | ";"                    // prototype / extern declaration
```

- A declaration with a `;` for body is a **prototype**; the function is
  expected to resolve at link time (binary `imports`, intrinsic, or defined
  elsewhere in the same namespace).
- A declaration with a `(...)` body is a **definition**.
- The return type `type-expr` may be `void`.
- Functions are **overloaded by signature** (name + arg types + arg count +
  pointer-ness). Two definitions with identical signatures in the same
  namespace are an error.
- Argument names need not be unique (the `uint8 func(#uint8 b; uint8 x)` form
  in the old test file is unrelated; pointer-ness now lives in the type-expr).

### 4.5 Intrinsics

A function whose name begins with `__` and has only a prototype is an
**intrinsic**: the body is not a Fridge-Assembly call but a single opcode or
opcode sequence emitted inline. Intrinsics have no Fridge-Assembly binding;
they are resolved by the compiler's intrinsic table. Examples in the test
file: `__mvi_a`, `__mvi_b`, `__mvi_c`, `__lxi_hl`, `__callsub`.

Intrinsics are declared exactly like other prototypes; no extra syntax.

## 5. Expressions

Expressions are uniformly prefix: `(operator operand operand …)`. A bare
identifier, literal, or paren-less form is also an expression.

### 5.1 Operators

| Op          | Arity | Meaning                                       |
|-------------|-------|-----------------------------------------------|
| `+ - * /`   | n     | arithmetic, n-ary folds left                  |
| `%`         | 2     | modulo                                         |
| `== !=`     | n     | chained equality (a==b==c)                    |
| `< > <= >=` | n     | chained comparison                            |
| `&& \|\|`   | n     | short-circuit logical AND/OR                  |
| `!`         | 1     | logical NOT                                    |
| `& \| ^ ~`  | n     | bitwise AND/OR/XOR/NOT                        |
| `++ --`     | 1     | in-place increment / decrement                |
| `= += -= *= /= %=` | 2 | assignment, returns the assigned value   |
| `at`        | 2     | array index / pointer deref (see §5.2)        |
| `addr`      | 1     | address-of (yields `(T ptr)`)                 |
| `cast`      | 2     | `(cast type-expr expr)` numeric/ptr narrowing|

### 5.2 `at` — index and deref

`(at aggregate index)` reads element `index` of an array, or — if the
aggregate is a plain pointer — dereferences it. When `index` is given, the
aggregate must have array type; when omitted, it must have pointer type.

```
(at arr i)                  // arr: (T array n);  yields T
(at ptr)                    // ptr: (T ptr);      yields T
```

As an lvalue (left side of `=`, `+=`, …) the operator evaluates to the
address of the element. Elsewhere it evaluates to the value.

### 5.3 `addr` — address-of

`(addr lvalue)` yields a value of type `(T ptr)` where `T` is the lvalue's
type. The lvalue may be a local, a static, a struct field access, or an
`(at …)` expression. `addr` of a literal or temporary is an error.

### 5.4 Member access

`a.b` accesses field `b` of struct `a`. It chains: `screen.buf.data`.
Member access is an lvalue when its receiver is.

### 5.5 Function call

`(name arg arg …)` calls a function. Overload resolution picks the unique
function in scope whose argument count and (possibly coerced) types match;
ambiguity is a compile error. Passing a value where a pointer is expected
requires an explicit `(addr …)`.

### 5.6 Literals as expressions

A number, char, or string literal is an expression of its natural type:
`42 : uint8` (narrowed on assignment if needed), `'x' : char`, `"…" : string`.
Array-literal initializers `(1, 2, 3)` are **not** expressions in their own
right — they appear only as the initializer of a `(T array N)` declaration,
not as a value inside an expression. To get an array value into a variable
later, use `(at …)` element-by-element.

## 6. Namespaces and modules

```
using NAME;                            // bring NAME's symbols into scope
"namespace" name ["imports" string] "(" decl … ")" ";"
```

- A namespace owns a set of structs, statics, `const`s, and functions.
- `using NS;` makes `NS`'s public names referenceable without qualification.
  It is **not** transitive; only direct members of `NS` are visible.
- `imports "file.bin"` marks the namespace as externally provided: its
  prototypes and `static … imports 0xADDR;` bindings are resolved against a
  binary ROM image rather than emitted here. The compiler emits no code for
  such namespaces; they only influence type-checking and call lowering.
- Individual `static T name imports 0xADDR;` declares a static whose address
  is fixed to `0xADDR` (a symbol imported from a ROM image). This is the
  linkage to the platform's pre-baked ROM tables and routines (`vtext`,
  `arithm`, etc.).

## 7. Statements

A function body is a sequence of statements. Statements end with `;`.

```
statement ::=
    declaration          // local var
  | assignment           // (= lvalue expr) etc.
  | expression ";"      // for side effects, e.g. (putstr 0 0 "…");
  | "return" [expr] ";"
  | "if" chain
  | "for" "(" init ";" cond ";" step ")" body
  | block
block ::= "(" statement … ")"
```

- `return` with no expression is legal only in a `void` function; with an
  expression the type must match the function's declared return type.
- `if` is a chain (as in the existing test file):
  ```
  if (cond) body
  else if (cond) body
  else body
  ```
  The trailing `else` clause is optional; each `body` is a single statement
  (typically a block).
- `for` has three `;`-separated syntactic slots: an initialization (a local
  declaration or an assignment), a condition expression, and a step
  expression. Any may be empty. The body is a single statement (typically a
  block). `break` and `continue` are **not** provided in v1 — exit a loop via
  a flag and an outer `if`; this keeps codegen small.

## 8. Storage and lifetime

- **Static storage**: `static` and `const` declarations live in their
  namespace's contiguous static buffer for the entire program lifetime. They
  are zero-initialized if no initializer is given. `const` symbols used as
  sizes or in `array N` type-exprs must fold to an integer at compile time.
- **Local storage**: locals are stack-allocated on function entry (the
  Fridge stack ascends, see `FRIDGE_ASCENDING_STACK`). An optional
  initializer lowers to a store after allocation. Locals are deallocated
  on function exit; there is no block scope inside a function — all locals
  declared anywhere in a function body share one frame and live for the whole
  call. (Simpler codegen; revisit if shadowing becomes a pain.)
- **Temporary storage**: expression evaluation uses the stack above the
  frame. The codegen must balance allocations across branches.

## 9. Calling convention

- Arguments are pushed right-to-left onto the stack (current intermediate
  convention).
- A function allocates its frame by advancing SP past the sum of its local
  sizes, then writes argument slots into named locals.
- Return value ABI depends on the return type's static size:
  - `sizeof(T) <= 2` (primitives, pointers, small structs): returned in the
    HL register pair, as today.
  - `sizeof(T) > 2` (larger structs): the caller allocates space for the
    result in its own frame, pushes the result's address as a **hidden
    leading argument** (before any user arguments), and the callee writes
    the result through that pointer using ordinary stores — the same store-
    through-pointer path used for `(at …)` writes. The callee leaves HL
    unused (or zeroed) on return; the caller reads the result from the
    reserved frame slot.
  - This uniform rule keeps the return mechanism straightforward: small
    returns travel in HL, large returns travel through an implicit `(T ptr)`
    out-argument. No stack juggling above the return address, no special
    callee-frame convention for the result.
- Caller pops the arguments off the stack on return. The hidden out-arg
  pointer is popped together with the user arguments.
- The entry point is `void main()` in the `global` namespace; it takes no
  arguments.

## 10. Scoping and name resolution

- Names are looked up in, in order:
  1. The current function's locals (and arguments).
  2. The current namespace's functions, structs, and statics.
  3. Every namespace named in a `using` directive, top-to-bottom.
  4. The `global` namespace.
- Two visible names may not collide; ambiguity is a compile error, not a
  silent override.
- Struct field names live only inside their struct's field list and via
  member access; they do not pollute any scope.

## 11. Reserved keywords

```
namespace using imports struct union static const
ptr array
if else for return
at addr cast
void bool char uint8 int8 uint16 int16 string
```

`null` is reserved for a future null-pointer literal (currently no syntax).
Identifiers beginning with `__` are reserved for intrinsics and must not be
defined by user programs.

## 12. Preprocessing / includes

```
include "path";      // textual include of another .cpm file at parse time
```

Includes are path-relative to the file being parsed, then to the compiler's
`-I` folders. Each file is parsed once; circular includes are an error.
There is no `#define`/`#ifdef` preprocessor. If a feature is needed, it
should be a language feature, not a textual one.

## 13. Errors and panics

- Compile-time: type errors, undeclared names, ambiguous overloads, array
  sizes not const-foldable, `addr` of a non-lvalue, etc. The compiler logs to
  its `Logger` and emits no binary on error.
- Runtime: the Fridge CPU has a Panic flag (§3 of `AGENTS.md`) used to halt
  on stack corruption. Frion does not yet define any construct that *raises*
  panic; a future `panic` operator may be added. Dereferencing a null pointer
  is undefined behavior (no implicit check).

## 14. Open questions

1. **`for` step side effects.** Allow `(= i (+ i 2))` as a step, or only
   `++`/`--`/assignment-operators? Current intermediate only shows `++ i`.

2. **Break/continue.** Confirm v1 ships without them, or add them now while
   the loop codegen is still small.