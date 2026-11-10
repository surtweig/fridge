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

- **Whitespace**: space, tab, CR, LF. Whitespace is required between an
  operator/callee and its first operand, after each operand comma, and between
  a type and its declaration name. It is otherwise insignificant. A comment
  alone does not supply required whitespace; comments never join tokens.
- **Comments**:
  - Line: `// …` to end of line.
  - Block: `/* … */`, may span lines, may not nest.
- **Literals**:
  - Decimal: `[+-]?[0-9]+d?`. An adjacent sign selects a signed type;
    a lowercase `d` suffix selects 16 bits, otherwise 8 bits. Examples:
    `42`, `256d`, `+10`, `-10d`. The value must fit that type (§5.6.2).
  - Hex: `0x` followed by exactly two or four `[0-9A-Fa-f]` digits, selecting
    `uint8` or `uint16` respectively. Examples: `0xFF`, `0x00A1`. Hex literals
    have no sign or suffix; signed interpretation requires `cast` (§5.6.2).
  - Char:    single-quoted single character `'x'`. No escape sequences yet.
  - String:  double-quoted `"Hello, World!"`. No escapes yet. A string literal
    has the distinct built-in type `string`; see §3.
  - Null pointer: `null`, contextually typed as `<T ptr>`; see §5.6.1.
- **Identifiers**: `[A-Za-z_][A-Za-z0-9_]*`. The `.` (CPM_SUBSCRIPT_DELIM)
  separates a namespace qualifier or struct-field access: `X2AL.Zuzu`,
  `myArray.data`.
- **Punctuation**:
  - `(` `)` — block and value-expression grouping
  - `<` `>` — compound-type grouping (also comparison operators in calls)
  - `;`    — statement, parameter-declaration, and initializer-element separator
  - `,`    — separator between operator/function-call operands
  - `.`    — subscript / field access
  - `'`    — char delimiter
  - `"`    — string delimiter
- **Pointer/reference sigils**: none. Use `<T ptr>`, `at`, and `addr`.
  `^` and `&` remain bitwise operators. `CPM_REF` only joins dot-qualified names.

## 2. Primitive types

| Type     | Size     | Range / notes                          |
|----------|----------|----------------------------------------|
| `void`   | 0        | function return only; no variables     |
| `bool`   | 1 byte   | `0` false, nonzero true                |
| `char`   | 1 byte   | ASCII; literal form `'x'`              |
| `uint8`  | 1 byte   | 0..255                                 |
| `int8`   | 1 byte   | -128..127                              |
| `uint16` | 2 bytes  | 0..65535  (also: pointer width)        |
| `int16`  | 2 bytes  | -32768..32767                          |
| `string` | 2 bytes  | pointer to null-terminated static text |

All multi-byte values are **big-endian** in memory, matching the Fridge CPU.

## 3. Type expressions

A type is described by a recursive **type-expr**. Primitive and struct names
may be bare; compound types use dedicated angle delimiters, not value-expression
parentheses.

```
type-expr ::=
    primitive-name
  | struct-name                       // qualified: NS.Name
  | "<" type-expr modifier modifier* ">"
modifier ::= "ptr" | "array" const-expr
```

There is no whitespace immediately after the opening `<`. Whitespace separates
the inner type, each modifier, and each array count. A count is a literal,
constant name, or parenthesized constant expression. Whitespace before the
closing `>` is optional; whitespace between a completed type and its declaration
name is required. Commas belong to value calls, not type components.

Examples:
```
uint8
<uint8 ptr>
<uint8 array 10>
<<uint8 array 10> array 20>           // 20 arrays, each containing 10 bytes
<<uint8 ptr> array 10>                // array of 10 pointers
<uint8 array intArraySize>
<uint8 array (+ intArraySize, 2)>
<uint8 ptr ptr>                       // equivalent to <<uint8 ptr> ptr>
```

`<` followed by whitespace is a comparison operator; `<=` is a single operator.
They require the same whitespace/comma separators as other calls. Inside a
parenthesized size expression, `>` / `>=` are also comparison operators and do
not close the surrounding type. Quoted literals and comments are opaque to
angle recognition.

The lexer and all declaration consumers use this syntax. Statics/constants,
struct fields, function returns/parameters, and locals preserve the recursive
`CPMType`; postfix array declarations and pointer sigils are rejected. Nested
static initializers use recursive element strides, including one-element arrays.
Local allocation uses recursive sizes; executable initializer, call, control-flow,
and aggregate-return lowering remain separate codegen work. Cast type operands
are resolved in constant expressions and validated throughout function bodies.

`ptr` may be applied to any type-expr. `array n` requires `n` to be a compile-
time constant expression (a `const` symbol or literal). The total static size
of any type-expr must not exceed 65535 bytes.

`string` is a distinct built-in type, represented by a two-byte character
address. Its literal form is a null-terminated block in static storage. It is
neither an array nor a type alias for `<char ptr>`; `sizeof(string)` is 2 bytes.
A `<char ptr>` must be explicitly converted with `(cast string, pointer)` when
assigning to a string or supplying a string parameter. The built-in `at`
operation on strings (§5.2) does not introduce an implicit pointer conversion.

Every `"…"` string literal is therefore a **static compile-time resource**: the
text is laid down in the namespace's static buffer with a trailing `0`, and the
literal's value is the address of its first character. Identical literals are
deduplicated within a namespace. A `string` variable is just a 2-byte pointer;
it may be reassigned to another string literal or to a character address
explicitly converted with `cast`. A character pointer can refer to writable
array storage; string character write permissions remain a separate design
question.

There is **no implicit array↔pointer decay** and no implicit null termination
for arrays. A `<char array N>` initialized with an array literal `('a'; 'b'; 'c')`
holds exactly N characters — no trailing `0` is added. To pass a character
array to a string-taking function, explicitly convert the address of its first
character: `(cast string, (addr (at buf, 0)))`. `(addr buf)` preserves the entire
array type and yields `<<char array N> ptr>`. Ensure the buffer is manually
null-terminated if the callee requires it.

Nesting is unrestricted. Modifiers wrap the preceding type from left to right:
`<T ptr ptr>` equals `<<T ptr> ptr>`, and `<T array 10 array 20>` equals
`<<T array 10> array 20>`. The latter is 20 arrays of 10 elements, with the same
layout as explicitly nested syntax. A redundant `<T>` wrapper with no modifier
is not a semantic type expression.

### 3.1 Pointer representation and conversions

A value of type `<T ptr>` occupies two bytes and stores a 16-bit address in
big-endian order, with the same bit representation as a `uint16`. Address zero
is the null pointer for every pointer type. This representation does not make
pointers integer types: integer-to-pointer and pointer-to-integer conversions
require an explicit `cast`, including conversion of the integer literal `0`.
There is no assignment-only exception for zero; use `null` when a null pointer
is intended (§5.6.1).

```cpm
uint16 address (0x1234);
<char ptr> p (cast <char ptr>, address);
uint16 stored_address (cast uint16, p);
= p, 0;                              // ERROR: integer is not a pointer.
= p, (cast <char ptr>, 0);            // Explicit zero-address pointer.
= p, null;                           // Context supplies <char ptr>.
```

## 4. Declarations

A declaration binds a name to a storage location. Its grammatical slot is the
same in namespaces, struct bodies, function argument lists, and function
bodies; the surrounding scope determines lifetime.

### 4.1 Variable declaration

```
decl ::= type-expr name [initializer] ";"
initializer ::= "(" init-expr ")"
              | "(" init-list ")"
init-list ::= init-elem (";" init-elem)* [";"]
init-elem ::= init-expr | initializer        // nested, for array-of-array
```

In any scope a bare type-expr (a single ID node or a angle-delimited compound)
is followed by a name and an optional initializer. The legacy postfix form
`uint8 x array 10` is **removed**; use `<uint8 array 10> x`.

An initializer list is **semicolon-separated** (`;`), preserving the original
language design. A trailing semicolon is optional; empty elements between
semicolons are invalid. Each element may itself be an initializer for nesting.
Commas belong to operator calls inside an element, not to the element list.
A declaration's type distinguishes an initializer from an executable block.
For a single-element array, use `(value;)` to make the list explicit.

Struct initializers use named bindings: `(field value; field value)` with an
optional trailing semicolon. A nested struct uses the same syntax recursively.
A single binding `(field value)` is interpreted as an initializer in a struct
initialization context. There is no comma between a field name and its value.
Parameter declarations likewise remain semicolon-separated (§4.4).

Examples:
```
uint8 x;
uint16 y (+ 54, 12);
<uint8 array intArraySize> myArr (0; (+ 1, 1); 2; 3; 4);
<char array 3> buf ('a'; 'b'; 'c');
<<uint8 array 2> array 2> grid ((1; 2); (3; 4));
<uint8 ptr> px (addr x);
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

Expressions are uniformly prefix: `(operator operand, operand, …)`. There
is whitespace, not a comma, before the first operand. Each subsequent operand
requires a comma followed by whitespace; whitespace-only multi-operand calls,
empty operands, and trailing commas are invalid. Zero-operand calls use
`(name)`. A bare identifier or literal is also an expression.

A paren-less call uses the same separators and ends at `;` (or the closing
parenthesis of a surrounding semicolon-separated list). Nested calls must be
parenthesized, so their commas cannot be confused with those of the outer call:

```cpm
= x, (func a, b, c);
(= x, (func a, b, c));
(< (+ 1, 2), 4);
```

Declaration components, type modifiers, `return expr`, and named struct-field
bindings are not call operand lists and do not gain commas.

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
| `at`        | 1 or 2 | array/pointer/string element access (§5.2)   |
| `addr`      | 1     | address-of (yields `<T ptr>`)                 |
| `cast`      | 2     | `(cast type-expr, expr)` numeric/ptr narrowing|

#### 5.1.1 Pointer arithmetic

`+` and `-` have built-in pointer forms. The first operand must have type
`<T ptr>`, followed by one or more integer offsets. Each offset counts elements
of `T`, so it is multiplied by `sizeof(T)`. Evaluation folds left, and the result
retains the exact pointer type of the first operand. These operator forms do
not introduce an implicit pointer/integer conversion.

For `p : <T ptr>` and integer offsets `a` and `b`:

| Expression | Result |
|---|---|
| `(+ p, a)` | address `p + a * sizeof(T)`, type `<T ptr>` |
| `(- p, a)` | address `p - a * sizeof(T)`, type `<T ptr>` |
| `(+ p, a, b)` | equivalent to `(+ (+ p, a), b)` |
| `(++ p)` | updates the pointer lvalue by `+sizeof(T)` bytes |
| `(-- p)` | updates the pointer lvalue by `-sizeof(T)` bytes |

Pointer addition and subtraction wrap modulo 65536, including the scaled
offset, just as unsigned 16-bit arithmetic does. Constant folding and runtime
evaluation must agree. Wrapping can produce address zero; arithmetic itself
does not check whether the result points to valid storage.

```cpm
<uint16 ptr> p (cast <uint16 ptr>, 0xFFFE);
= p, (+ p, 1);                       // Wraps to address 0, still <uint16 ptr>.
++ p;                               // Advances to address 2.
-- p;                               // Returns to address 0.
```

An integer-first expression such as `(+ 1, p)` is invalid. Pointer operands
cannot be used as integer offsets; pointer-pointer subtraction is not provided.
Multiplication, division, remainder, and bitwise operations require an explicit
conversion to an integer before operating on an address.

`string` does not participate in pointer arithmetic: `(+ text, 1)`, `(- text, 1)`,
`(++ text)`, and `(-- text)` are invalid when `text` has type `string`. Explicitly
cast to a pointer type to perform pointer arithmetic. A `<string ptr>` does
participate, with offsets scaled by `sizeof(string)` (two bytes).

### 5.2 `at` — typed element access and dereference

`at` accesses contiguous elements by a zero-based integer index. An array
requires an explicit index. Pointers and strings accept an optional index;
omitting it is equivalent to supplying zero.

| Operand type | Form | Element type |
|---|---|---|
| `<T array N>` | `(at arr, i)` | `T` |
| `<T ptr>` | `(at p)` or `(at p, i)` | `T` |
| `string` | `(at text)` or `(at text, i)` | `char` |

For a pointer `p : <T ptr>`, element `i` starts at address
`p + i * sizeof(T)`. The index counts elements, not bytes: `<uint16 ptr>`
advances two bytes per index, a pointer to a struct advances by its full size,
and a pointer to an array advances by that array's full size. This applies to
any legal pointer target type. Indexing itself does not change the pointer.
Pointer address calculation follows the same scaling and wrapping rules as
pointer arithmetic (§5.1.1): `(at p, i)` accesses the same element as
`(at (+ p, i))`.

A string has an explicit built-in `at` operation accessing its characters.
It remains a distinct type for assignment, casts, and overload resolution.
A `<string ptr>` instead accesses a contiguous sequence of two-byte string
values; it does not access the characters of a single string:

```cpm
(at text, i)                // text: string; yields char
(at string_pointers, i)     // string_pointers: <string ptr>; yields string
```

Reading an element yields its element type; as an assignment destination,
`at` designates that element's storage. Existing aggregate-value restrictions
still apply when `T` is an aggregate; this rule does not settle aggregate
copying. Write permissions, including writes through strings or into literal
storage, are a separate mutability decision.

An array has a statically known length. Ordinary pointers and string values
carry no buffer length, and character indexing does not search for a null
terminator. Bounds-checking policy is not established by this indexing rule.
Dereferencing null remains undefined behavior (§13).

### 5.3 `addr` — address-of

`(addr lvalue)` yields a value of type `<T ptr>` where `T` is the lvalue's
type. The lvalue may be a local, a static, a struct field access, or an
`(at …)` expression. `addr` of a literal or temporary is an error.

### 5.4 Member access

`a.b` accesses field `b` of struct `a`. It chains: `screen.buf.data`.
Member access is an lvalue when its receiver is.

### 5.5 Function call

`(name arg, arg, …)` calls a function. Overload resolution picks the unique
function in scope whose argument count and (possibly coerced) types match;
ambiguity is a compile error. Passing a value where a pointer is expected
requires an explicit `(addr …)`. A `string` parameter requires a string-typed
argument; a character pointer must be explicitly converted with `cast`.
A `null` argument can match a pointer parameter contextually (§5.6.1); integer
`0` cannot.

### 5.6 Literals as expressions

A number, char, or string literal is an expression of its explicit literal type:
`42 : uint8`, `42d : uint16`, `+42 : int8`, `+42d : int16`, `'x' : char`,
`"…" : string`. Integer literal spelling fixes the type before assignment or
overload resolution; context does not change it. Narrowing requires an explicit
`cast` (§5.6.2).

Array-literal initializers `(1; 2; 3)` are **not** expressions in their own
right — they appear only as the initializer of a `<T array N>` declaration,
not as a value inside an expression. To get an array value into a variable
later, use `(at …)` element-by-element.

#### 5.6.1 `null` — contextual null-pointer literal

`null` denotes address zero for any pointer type. It is a dedicated literal,
not an integer constant or a runtime universal pointer type. Its concrete
`<T ptr>` type comes from the expected type in a declaration initializer,
assignment, function argument, return expression, or explicit pointer cast.

```cpm
void process_point(<Point ptr> value);

<Point ptr> find_point()
(
    <Point ptr> p (null);             // Initializer supplies <Point ptr>.
    = p, null;                       // Assignment destination supplies the type.
    (process_point null);            // Parameter supplies <Point ptr>.
    return null;                     // Return type supplies <Point ptr>.
);
```

Numeric `0` remains an integer in every context. Assigning, initializing,
passing, or returning it as a pointer requires `(cast <T ptr>, 0)`. Conversely,
`null` does not implicitly convert to an integer, `bool`, or `string`.

When resolving overloaded calls, `null` can match any pointer parameter, but
does not prefer one pointee type over another. If multiple signatures remain
viable, the call is ambiguous and must use an explicit pointer cast:

```cpm
void inspect(<Point ptr> value);
void inspect(<char ptr> value);

(inspect null);                      // ERROR: ambiguous pointer overloads.
(inspect (cast <Point ptr>, null));   // Selects the <Point ptr> overload.
(process_point 0);                   // ERROR: no implicit integer conversion.
(process_point (cast <Point ptr>, 0));
```

An uncontextualized `null` has no concrete pointer type; operations requiring
one, such as `(at null)` or `(+ null, 1)`, are errors. Use an explicit pointer
cast to supply a type. Typed null pointers still obey the ordinary pointer
arithmetic rules, and dereferencing a null pointer is undefined behavior (§13).

This is target behavior. The compiler's existing `uint16` constant named
`null` must be replaced by contextual literal handling during implementation.

#### 5.6.2 Integer literals and explicit integer casts

Decimal literals select their type through an optional adjacent sign and a
lowercase `d` suffix. There is no automatic promotion when a literal exceeds
its type's range:

| Decimal form | Type | Range | Examples |
|---|---|---|---|
| digits | `uint8` | 0…255 | `0`, `10`, `255` |
| digits followed by `d` | `uint16` | 0…65535 | `10d`, `256d`, `65535d` |
| `+` or `-` followed by digits | `int8` | −128…127 | `+10`, `-10`, `-128` |
| `+` or `-` followed by digits and `d` | `int16` | −32768…32767 | `+10d`, `-10d`, `-32768d` |

The sign, digits, and suffix must be contiguous. An adjacent sign is part of
the literal; a whitespace-separated sign is an operator call. These forms are
syntactically distinct and do not have to yield the same type:

```cpm
-10                                  // int8 literal.
(- 10)                               // Unary operator call with a uint8 operand.
(+ -10, +2)                          // Operator call with two int8 operands.
```

Validate the complete signed value directly. In particular, `-128` and
`-32768d` are valid literals; they are not formed by negating an out-of-range
positive signed literal. `256`, `+128`, `-129`, `65536d`, `+32768d`, and
`-32769d` are compile-time errors. An enclosing cast cannot rescue an invalid
literal: `(cast uint8, 256)` is invalid because `256` itself is invalid.

Hexadecimal literals are always unsigned. Their width is determined by exactly
two or four digits after `0x`, including leading zeros:

```cpm
0x01                                 // uint8, value 1.
0x0001                               // uint16, value 1.
0xFF                                 // uint8, value 255.
0x0100                               // uint16, value 256.
0x100                                // ERROR: use 0x0100.
0x1                                  // ERROR: use 0x01.
-0x80                                // ERROR: signed hex literals are not provided.
```

The `d` suffix applies only to decimal literals. In a hex literal, `d` and `D`
are hexadecimal digits and count toward its two- or four-digit width.

Explicit narrowing to an integer type retains the low bits at the destination
width. A signed destination interprets those bits as two's complement.
Equal-width signed/unsigned integer casts preserve the bit pattern. These
rules apply equally during constant folding and runtime evaluation:

```cpm
(cast uint8, 256d)                   // uint8, value 0: retains low 8 bits.
(cast uint8, 0x01FF)                 // uint8, value 255.
(cast int8, 0xFF)                    // int8, value -1: same 8-bit pattern.
(cast int16, 0xFFFF)                 // int16, value -1: same 16-bit pattern.
(cast uint8, -1)                     // uint8, value 255: same 8-bit pattern.
```

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
  | expression ";"      // for side effects, e.g. (putstr 0, 0, "…");
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
    returns travel in HL, large returns travel through an implicit `<T ptr>`
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
null
```

`null` is the null-pointer literal described in §5.6.1.
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

1. **`for` step side effects.** Allow `(= i, (+ i, 2))` as a step, or only
   `++`/`--`/assignment-operators? Current intermediate only shows `++ i`.

2. **Break/continue.** Confirm v1 ships without them, or add them now while
   the loop codegen is still small.
