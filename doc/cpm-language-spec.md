# Frion (CPM) Language Specification — Draft

> Status: target language contract with explicit pending choices (§14).
> The code in `emulator/cpm/` determines current implementation behavior;
> this document defines the settled target behavior. Where the two disagree,
> the intent is to bring the code in line with this document.
>
> Contract consolidated from the Q01–Q30 answers and subsequent clarifications
> through 2026-11-11. This specification defines source-language behavior;
> [calling-convention.md](calling-convention.md) defines the stack ABI.
> [cpm-design-questions.cpm](cpm-design-questions.cpm) retains the discussion and
> intentionally invalid examples. Unsettled decisions are listed in §14;
> recommendations there are not adopted language rules. Implementation status
> belongs in [cpm-pre-codegen-review.md](cpm-pre-codegen-review.md).

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
  - Quoted text: double-quoted `"Hello, World!"`. It has an immutable character
    array value whose length includes a terminating zero (§3). Escapes are
    reserved for future use.
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
| `string` | 2 bytes  | address of null-terminated character storage |

All multi-byte values are **big-endian** in memory, matching the Fridge CPU.

## 3. Type expressions

A type is described by a recursive **type-expr**. Primitive and struct names
may be bare; compound types use dedicated angle delimiters, not value-expression
parentheses.

```
type-expr ::=
    primitive-name
  | aggregate-name                    // struct/union; qualified: NS.Name
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

`ptr` may be applied to any non-void type-expr. `array n` requires a positive
compile-time integer constant expression (a `const` symbol, literal, or grouped
constant expression). Pointers to void, arrays of void, zero-length arrays, and
empty aggregate definitions are invalid. The total size of any type-expr must
not exceed 65535 bytes; allocated objects also obey the target memory budget.

`string` is a distinct two-byte value containing the address of null-terminated
character storage. A pointer can be explicitly reinterpreted as a `string` with
`cast`. The address must designate valid character storage when used.

A quoted literal denotes read-only `<char array N>` storage. `N` is the
character byte count plus one for the terminating zero. For example, `"cat"`
has type `<char array 4>` and `""` has type `<char array 1>`. The elements can
be read or copied by value. Direct assignment to a literal element and taking
its address as a writable pointer are compile errors.

A quoted literal can initialize a `string` in a declaration. A writable
`string` receives a distinct character buffer containing a copy of the
literal's bytes, including its terminator. Local initialization copies these
bytes each time control reaches the declaration. The `string` value continues
to occupy two bytes. The character buffer requires additional storage.

```cpm
<char array 4> letters ("cat");
string text ("cat");
string alias (text);
= (at alias, 0), 'b';                // text and alias now read "bat".
char first (at "cat", 0);             // Reads 'c'.
```

The literal-to-`string` initialization rule applies only to declarations.
In calls, returns, and assignments a quoted literal has its array type. A
`string` copied from an existing `string` shares its character storage through
the copied address. This also applies to string fields in aggregate copies.
Writable-buffer placement and lifetime, and storage for `const string`
initializers, remain D4 decisions (§14).

Character arrays use exact types and lengths. A quoted literal includes its
zero element, while an ordinary initializer list such as `('a'; 'b'; 'c')`
supplies exactly the three elements shown. To reference a writable,
null-terminated array with a `string`, use
`(cast string, (addr (at buf, 0)))`. The expression `(addr buf)` instead yields
a pointer to the entire array, preserving its type and length.

Nesting is unrestricted. Modifiers wrap the preceding type from left to right:
`<T ptr ptr>` equals `<<T ptr> ptr>`, and `<T array 10 array 20>` equals
`<<T array 10> array 20>`. The latter is 20 arrays of 10 elements, with the same
layout as explicitly nested syntax. A redundant `<T>` wrapper with no modifier
is not a semantic type expression.

### 3.1 Pointer representation and conversions

A value of type `<T ptr>` occupies two bytes and stores a 16-bit address in
big-endian order, with the same bit representation as a `uint16`. Address zero
is the null pointer for every pointer type. Use `cast` to reinterpret a
two-byte integer as a pointer or vice versa. Use `null` when a null pointer is
intended (§5.6.1).

```cpm
uint16 address (0x1234);
<char ptr> p (cast <char ptr>, address);
uint16 stored_address (cast uint16, p);
= p, 0;                              // ERROR: integer is not a pointer.
= p, (cast <char ptr>, 0d);           // Explicit zero-address pointer.
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

An array initializer must supply exactly its declared number of elements at
every level; there is no partial initializer zero-fill. Struct initialization
is named, never positional. A union initializer selects one view. Missing or
repeated struct bindings and precise union-view selection are pending (§14, D3).

Examples:
```
uint8 x;
uint16 y (+ 54d, 12d);
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

The `imports` clause gives a namespace a library file or a static a fixed
address (§6).

### 4.3 Struct and union

```
struct-decl ::= "struct" name view ";"
union-decl  ::= "union" name view view* ";"
view        ::= "(" field-decl field-decl* ")"
field-decl  ::= type-expr name ";"
```

A struct is one packed sequence of fields. Field offsets follow declaration
order, and its size is the sum of field sizes. No padding or alignment bytes
are inserted. Empty structs and empty union views are invalid.

A union contains one or more packed views. Each view starts at offset zero;
fields within a view have consecutive offsets in declaration order. All views
must have the same total size, and field names must be unique across the entire
union. Individual fields within a view may have different sizes:

```cpm
union WordView
(
    uint8 high;                    // Offset 0.
    uint8 low;                     // Offset 1.
) (
    uint16 word;                   // Offset 0; this view is also two bytes.
);
```

Aggregate names are owned by their namespace and may be qualified (`NS.Name`)
or found through `using`. A field may refer to a later aggregate definition.
Direct or mutual by-value cycles, including cycles through array elements, are
invalid. Pointers break that recursion, so `<Node ptr>` fields are legal in
`Node`; pointed-to array types must still satisfy the type-size bound once
layouts are known. Cross-view union reading rules remain pending (§14, D3).

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

- A declaration with a `;` for body is a **prototype**. A call must resolve to
  a definition in the same namespace or to a compiler intrinsic.
- A declaration with a `(...)` body is a **definition**.
- Returns may use any legal value type, including structs and arrays, or `void`.
  Parameters use legal non-void types and are passed by value (§5.7, §9).
- Functions are **overloaded by signature**: name and the ordered list of full
  recursive parameter types. Array dimensions and nested pointer/array shapes
  are part of those types. Return types and parameter names do not distinguish
  overloads. Two bodies for the same signature are an error.
- Parameter names must be unique within a function. A prototype and definition
  for the same signature must have matching return types and parameter names;
  disagreement is an error. Compatible prototypes may refer to the same function.

### 4.5 Intrinsics

CPM emits intrinsics inline. The built-in names can be used directly or
declared with a matching prototype. Each intrinsic returns `void`.

| Intrinsic | Parameter | Effect |
|-----------|-----------|--------|
| `__mvi_a`, `__mvi_b`, `__mvi_c` | `uint8` | Load A, B, or C. |
| `__lxi_hl` | `uint16` | Load HL. A `string` can be bit-cast to `uint16`. |
| `__callsub` | Constant `uint16` | Call a routine using the register ABI and the address rules in §6. The callee can clobber general registers and flags. |

Register loads from literals and named values preserve the other registers.
Evaluate complex arguments into local variables before loading registers for
a foreign call.

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
| `+`         | 1+    | identity for one operand; otherwise add left to right |
| `-`         | 1+    | negate one operand; otherwise subtract left to right |
| `* /`       | n     | arithmetic left folds; minimum arity pending (§14) |
| `%`         | 2     | modulo                                         |
| `==`        | n     | all operands equal                            |
| `!=`        | n     | all operands pairwise distinct                |
| `< > <= >=` | n     | chained comparison                            |
| `&& \|\|`   | n     | short-circuit logical AND/OR                  |
| `!`         | 1     | logical NOT                                    |
| `& \| ^`    | n     | bitwise AND/OR/XOR                            |
| `~`         | 1     | bitwise NOT                                   |
| `++ --`     | 1     | in-place increment / decrement                |
| `= += -= *= /= %=` | 2 | assignment, returns the assigned value   |
| `at`        | 1 or 2 | array/pointer/string element access (§5.2)   |
| `addr`      | 1     | address-of (yields `<T ptr>`)                 |
| `cast`      | 2     | reinterpret bytes as an equal-size value (§5.6.3) |
| `convert`   | 2     | explicitly convert a scalar value (§5.6.3)    |

Numeric compatibility requires **exact types**. Initializers, assignments,
function arguments, and return expressions do not implicitly widen, narrow, or
change numeric signedness. Ordinary numeric operator operands must also have
matching types. Literal spelling fixes the type before these checks. Use
`convert` for numeric widening and other value conversions, or `cast` to
reinterpret an equal-size representation.

```cpm
uint16 wide (42d);
uint16 invalid (42);                 // ERROR: uint8 is not uint16.
uint16 explicit_wide (convert uint16, 42);
int16 signed_value (-1d);
int16 invalid_sum (+ signed_value, 1); // ERROR: int16 and uint8 differ.
int16 sum (+ signed_value, (convert int16, 1));
```

The pointer-first arithmetic forms below retain their explicitly defined
pointer/offset operand roles. Operator result and arity details are tracked
under D2 (§14).

`if` and `for` conditions, and every operand of `!`, `&&`, and `||`, must have
type `bool`. Logical operators return `bool`. Integers, pointers, strings, and
`null` are not implicitly treated as conditions; use a typed comparison or a
permitted explicit conversion. For example, `if (0)` is invalid, while
`if (false)` and `if (== counter, 0)` are valid when `counter` is `uint8`.

Operands are evaluated left-to-right. A call snapshots each by-value argument
when that argument is evaluated, including aggregate contents. `&&` stops at
the first false operand and `||` at the first true operand; remaining operands
are not evaluated. Compound assignments compute their destination address,
including any index/receiver effects, exactly once before evaluating the right
operand and storing the result.

Unsigned integer arithmetic wraps modulo the width of its type: `(+ 255, 1)`
has type `uint8` and value zero. This behavior must agree in constant folding
and runtime evaluation. Signed overflow and shift edge cases remain pending.
Division follows Python-style floor division and remainder satisfies
`a = b * floor(a/b) + remainder`. For example, `(/ -7d, +3d)` is `-3d` and
`(% -7d, +3d)` is `+2d`. Constant division/remainder by zero is a compilation
error; runtime division/remainder by zero sets Panic and halts (§13).

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

`at` accesses contiguous elements by a zero-based `uint8` or `uint16` index.
An array requires an explicit index. Pointers and strings accept an optional
index that defaults to zero.

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

Reading an element yields its element type. For writable storage, `at` also
designates an assignment destination. Elements of quoted literals are
read-only (§3). Aggregate element copies follow §5.7.

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
Member access is an lvalue when its receiver is. Computed lvalue member access
through `at` is intended for indexed arrays and dereferenced pointers. Its exact
spelling, such as `(at points, 0).x`, and treatment of other temporary receivers
remain pending (§14, D5).

### 5.5 Function call

`(name arg, arg, …)` calls a function. An unqualified call searches the merged
overload set (§10). A qualified call searches the named namespace. The call
must match exactly one candidate by argument count and type. Passing an
object's address requires `(addr …)`. A `string` parameter requires a
string-typed expression. A quoted literal is an array argument, and an
explicit `cast` can reinterpret a pointer as a `string`. Contextual `null`
can match a pointer parameter (§5.6.1).

### 5.6 Literals as expressions

Literal spelling determines the expression type. For example, `42` is
`uint8`, `42d` is `uint16`, `+42` is `int8`, `+42d` is `int16`, and `'x'`
is `char`. A quoted literal such as `"cat"` is a read-only `<char array 4>`.
Overload resolution uses these types. Use the explicit operations in §5.6.3
to change a value's type.

Array-literal initializers `(1; 2; 3)` are **not** expressions in their own
right: they appear in declaration initialization contexts. Existing array values
can be copied through assignment, parameters, and results (§5.7).

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

Numeric zero has its ordinary literal type. Use `null` for a contextually
typed zero pointer or `(cast <T ptr>, 0d)` to reinterpret a two-byte zero.
Implicit conversion from `null` to an integer, `bool`, or `string` is invalid.

When resolving overloaded calls, `null` can match any pointer parameter, but
does not prefer one pointee type over another. If multiple signatures remain
viable, the call is ambiguous and must use an explicit pointer cast:

```cpm
void inspect(<Point ptr> value);
void inspect(<char ptr> value);

(inspect null);                      // ERROR: ambiguous pointer overloads.
(inspect (cast <Point ptr>, null));   // Selects the <Point ptr> overload.
(process_point 0);                   // ERROR: no implicit integer conversion.
(process_point (cast <Point ptr>, 0d));
```

An uncontextualized `null` has no concrete pointer type; operations requiring
one, such as `(at null)` or `(+ null, 1)`, are errors. Use an explicit pointer
cast to supply a type. Typed null pointers still obey the ordinary pointer
arithmetic rules, and dereferencing a null pointer is undefined behavior (§13).

This is target behavior. The compiler's existing `uint16` constant named
`null` must be replaced by contextual literal handling during implementation.

#### 5.6.2 Integer literals

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
`-32769d` are compile-time errors. Check the literal before applying `cast`
or `convert`. For example, `(convert uint8, 256)` fails at the literal.

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

#### 5.6.3 Explicit type changes

`(cast T, value)` reinterprets the byte representation of a value as type
`T`. The source and destination must have the same size. This applies to
all value types, including scalars, pointers, strings, structs, unions,
and arrays. Sizes and packed layouts are known at compile time. The cast
produces a value of the destination type with the same bytes in the same
order. A size mismatch is a compile error.

```cpm
struct Pair (uint8 high; uint8 low;);
Pair bytes (cast Pair, 0x1234);       // high = 0x12, low = 0x34.
int8 signed_byte (cast int8, 0xFF);   // -1.
bool flag (cast bool, 0x80);          // True. The stored byte is 0x80.
uint8 raw (cast uint8, flag);         // 0x80.
uint16 invalid (cast uint16, raw);    // ERROR: the sizes differ.
```

A bool uses one byte and has two logical values. Zero is false and all
nonzero byte patterns are true. Boolean conditions, logical operators, and
boolean equality use that logical value. A bit cast preserves the stored byte.

`(convert T, value)` uses the following scalar conversion table. These
conversions preserve or transform the mathematical value.

| Source and destination | Rule |
|---|---|
| Integer to integer | Reduce the mathematical value modulo `2^destination_bits`, then interpret the result according to the destination's signedness. Signed widening sign-extends and unsigned widening zero-extends. |
| Integer to bool | Zero yields false and nonzero yields true. |
| Bool to integer | False yields zero and true yields one. |
| Char and `uint8` | Preserve the character byte. |

Pointer, string, and aggregate types are outside the conversion table.
Apply the same rules in constant folding and at runtime.

```cpm
uint8 narrowed (convert uint8, 256d); // 0.
uint16 widened (convert uint16, -1); // 65535.
uint8 logical (convert uint8, flag); // 1.
```

Since `"a"` has the two-byte representation `0x6100`, casting it to
`string` yields a string value containing address `0x6100`. Use the quoted
literal initializer in §3 to create a string buffer containing those bytes.

### 5.7 Aggregate values

Structs, unions, and arrays are fixed-size contiguous values. They may be
initialized from an existing value of the exact same type, assigned by value,
passed by value, and returned by value. No implicit array-to-pointer decay or
conversion between different dimensions, nesting, or named aggregate types occurs.

```cpm
<uint8 array 2> source (1; 2);
<uint8 array 2> destination;
= destination, source;              // Copy both bytes.
```

Copying an aggregate copies its representation, including array elements and
nested aggregates. Pointer and string fields copy their addresses; their pointed-to
storage is not cloned. Copies of potentially overlapping aggregate storage still
need an explicit rule (§14, D3). Argument evaluation snapshots and stack result
storage follow §5.1 and §9.

## 6. Namespaces and modules

```
using NAME;                            // bring NAME's symbols into scope
"namespace" name ["imports" string ["at" address]] "(" decl … ")" ";"
```

- A namespace owns a set of structs, statics, `const`s, and functions.
- `using NS;` makes `NS`'s public names referenceable without qualification.
  It is **not** transitive; only direct members of `NS` are visible.
- `imports "file.bin"` lets the linker choose where to place the library.
  Include each generated `.cpm` header and keep its `.bin` and `.bin.rel`
  beside it. CPM emits symbolic library references. In the final FALC pass,
  the linker places the libraries after the program's code and statics,
  skips reserved fixed-address ranges, and updates the references.
- `imports "file.bin" at 0xADDR` links a raw binary assembled for that origin.
  CPM resolves the file relative to the declaring source and emits a FALC
  `incbin` directive. The default executable load address is `0x0200`. FALC pads the
  image to each library's address and rejects overlap with the program or
  another binary.
- In a relocatable namespace, `static T name imports 0xADDR;` and
  `__callsub 0xADDR` use byte offsets within the library. In other namespaces
  they use absolute addresses. Wrapper functions receive CPM stack arguments,
  load the registers expected by the foreign routine, and call it with
  `__callsub`. The statics and calls in linked namespaces must fall inside
  their binary.
- A caller can also declare fixed-address statics outside an imported
  namespace to reference memory already provided by the platform.

Build the SDK tools, the standard library, and Hello World from the
repository root.

```sh
./sdk/build.sh
./sdk/x2al_std/build.sh
./sdk/programs/hello_world/hello_world_build.sh
```

`cpm --bin source.cpm --include directory` assembles and links the program
into `source.bin` beside its source. `--include` (also `-I`) can be repeated
and works with `--check` and `--asm` as well.

`--offset address` sets the assembly origin. It accepts decimal or `0x`
hexadecimal values and defaults to `0x0200`. Byte zero of the output image
must be loaded at the chosen address.

`--main-return` controls what happens when `main` finishes.

- `bootloader` executes `JMP 0x0000`. This is the default.
- `halt` executes `HLT`.
- `none` enters `main` directly after startup initialization and uses its
  ordinary `RET`. The caller must enter the image with `CALL` or supply a
  return address on the stack.

FALC takes the text dimensions from its build profile. For the Atlys 40×20
text grid, configure FALC with `-DCMAKE_CXX_FLAGS=-DFRIDGE_VIDEO_240X160`.

## 7. Statements

A function body is a sequence of statements. Simple statements end with `;`;
parentheses delimit blocks and control-statement bodies.

```
statement ::=
    declaration          // local var
  | assignment           // (= lvalue, expr) etc.
  | expression ";"      // for side effects, e.g. (putstr 0, 0, "…");
  | "return" [expr] ";"
  | "break" ";"
  | "continue" ";"
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
- `for` has three `;`-separated slots: an optional initialization (a local
  declaration or assignment), a mandatory `bool` condition, and an optional step
  expression. Assignment expressions are allowed as steps. The body is a single
  statement, typically a block.
- `break` and `continue` take no operands and target the innermost enclosing
  loop. `break` exits it; `continue` proceeds to its step and then retests its
  condition. Both are errors outside a loop. As zero-operand operations they
  may also use the parenthesized spellings `(break);` and `(continue);`.
  All exits must restore the expected temporary stack depth (§9).
- Reaching the end of a `void` function performs an implicit `return;`.
  A reachable end of a non-void function is a compile error. A non-void
  function may have paths that never return if they cannot reach the end.

## 8. Storage and lifetime

- **Static storage**: namespace-level `static` and `const` objects have program
  lifetime. Namespace initializers must be computable at compile time; absent
  initializers give zero-initialized storage. Const values used in array sizes
  must fold to integers. The precise permitted constant-expression set and
  dependency ordering remain pending (§14, D7).
- **Function-local statics**: storage persists across calls, while the name is
  visible only in its declaring block after the declaration. Each execution of
  an initializer performs an ordinary assignment, which may use function
  arguments. There is no once-only initialization guard. Skipping the declaration
  skips its initialization; recursion still refers to the same static object.
- **Addressable constants**: a folded constant whose address is taken must have
  backing storage for that address. Direct assignment to a constant is an error;
  modifying it through a pointer is undefined behavior, with no required runtime
  protection. Constant string text may be pooled; writable local literal buffers
  remain distinct.
- **Local storage**: reserve local storage on function entry, while visibility
  is lexical: a local name becomes available at its declaration and remains
  visible within its block. Every use before declaration is an error, including
  a use of a variable declared without an initializer. Initializers execute in
  program order whenever control reaches the declaration. After an uninitialized
  declaration, its bytes are unspecified; reads may produce a warning.
  Reserving a function-wide frame does not expose a name before its declaration
  or outside its block. Local string literal initializers additionally require
  distinct character buffers as described in §3.
- **Temporary storage**: the descending stack places expression temporaries
  below the reserved local frame. Branch joins and loop backedges must agree
  on temporary depth; return, break, and continue paths balance their allocations.
- **Local literal buffers**: the planner reserves each local string literal's
  character storage separately from its string slot. String copies alias the
  referenced characters. The buffer lifetime/escape policy remains pending
  (§14, D4), so frame reservation alone does not establish valid pointer lifetime.

## 9. Calling convention and startup

[calling-convention.md](calling-convention.md) is the stack ABI reference for
this target. It replaces the earlier HL/special hidden-output-pointer scheme.

- The stack descends. The target's default header leaves
  `FRIDGE_ASCENDING_STACK` undefined. Slots use exact type sizes with no padding;
  multibyte values are big-endian.
- Every CPM argument and non-void result travels through stack storage, including
  one-byte values, two-byte values, and aggregates. A void result has no slot.
- The caller reserves `sizeof(return type)` bytes and places argument values in
  the layout corresponding to left-to-right pushes. Evaluation is left-to-right
  with each value snapshotted at its evaluation point; nested calls must not
  interleave live temporaries into the final contiguous argument slots.
- `CALL` pushes a two-byte return PC. The callee reserves its local frame and
  writes its result into the caller's return slot before freeing local storage
  and executing `RET`. The caller removes argument bytes; the result slot then
  remains as an expression value until consumed.
- All frame accesses are SP-relative with compile-time corrections for the
  current temporary depth. Registers/flags preserved across calls, external
  intrinsic effects, and escaped-storage lifetimes still need a complete
  contract (§14, D4/D8).
- The entry point is `void main()` in the `global` namespace, with no parameters.
  The standard loader jumps to `0x0200` with SP initialized to `0xFFFF`.
  By default the entry stub calls `main` and jumps to `0x0000` when it returns.
  The compiler options in §6 select the origin and return behavior. Function
  and static placement must prevent accidental fallthrough into data or
  unrelated functions.

## 10. Scoping and name resolution

- For names other than function overloads, search visible locals and
  parameters, then the current namespace, used namespaces, and `global`.
  Ambiguous name references are compile errors.
- A local may not reuse the name of a static in its own namespace. Function
  parameters also obey their uniqueness rule (§4.4). The remaining collision
  cases, including enclosing-block shadowing, are listed in §14, D7.
- For an unqualified function call, merge overload candidates from the
  current namespace, all namespaces named in visible `using` directives, and
  `global`. Count each declaration once if it is reachable by several paths.
  When separate functions have the same matching parameter signature, the call
  is ambiguous. Qualification limits the candidate set to the named namespace.
- Struct/union field names live inside their aggregate and through member access;
  they do not pollute surrounding scopes. Union field names are unique across
  all of its views.

## 11. Reserved keywords

```
namespace using imports struct union static const
ptr array
if else for return break continue
at addr cast convert
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
- Runtime: integer division or remainder by zero sets the Fridge CPU Panic flag
  and halts execution. The helper/trap mechanism is an implementation contract
  still to be completed (§14, D8); `HLT` alone does not set Panic.
- Dereferencing a null pointer and modifying constant storage through a pointer
  are undefined behavior, without implicit runtime checks. General array/pointer
  bounds policy remains pending (§14, D6).

## 14. Remaining design decisions

The [D1–D8 worksheet](cpm-design-decisions-d1-d8.md) records examples,
proposals, and confirmed answers. The table tracks decision status for
semantic analysis and IR work. Q numbers refer to the discussion file.

| ID | Status or remaining question | Proposal for an open question |
|---|---|---|
| D1 — casts and index operands | Resolved. Equal-size values support bit casts. The scalar conversion table is in §5.6.3 and `at` index types are in §5.2. | — |
| D2 — operator edges | Operator operand domains (including equality on strings/aggregates), signed overflow, minimum signed value divided by `-1`, shifts, arithmetic/unary result types, `++`/`--` results, and remaining variadic minimum arities (Q07/Q10). | Preserve operand type for arithmetic, wrap signed arithmetic at its width, reject invalid constant shifts, define runtime shift behavior, and make `++`/`--` return the updated value. All require confirmation. |
| D3 — aggregate initialization/copy | Omitted/duplicate bindings, selecting a union view, reading another view, and overlapping copies (Q13/Q15/Q16). | Require every field exactly once for an initialized struct or one complete union view; allow raw cross-view reinterpretation; make overlapping assignment preserve the complete source value. Exact-type array assignment itself is already settled. |
| D4 — character-buffer lifetime | Placement and escape of writable buffers created by declarations, plus storage for `const string` initializers (Q21). Literal type, terminator, and declaration-only copying are resolved in §3. | Tie frame-backed buffers to their declaring scope and specify dangling uses as UB. |
| D5 — computed members | Exact spelling for `at`-receiver members, plus readable/assignable/addressable fields of other temporary receivers (Q12). | Support `(at values, i).field` and named chains; reject other computed receivers in v1. |
| D6 — bounds | Compile-time and runtime out-of-range access policy, including negative array indices (Q03). | Diagnose statically known invalid array indices; otherwise perform unchecked access. Ordinary pointers/strings carry no length. Null dereference is already UB. |
| D7 — names and constants | Overload merging is resolved in §10. Other name collisions and constant-expression ordering are covered by the D7 worksheet. | Reject collisions with visible names and allow reuse in disjoint scopes. Evaluate a pure typed constant-expression subset with dependency-cycle errors. |
| D8 — linking and completion | Relocatable imports, fixed-address imports, and void register wrappers are described in §4.5 and §6. External return-value marshaling and the runtime Panic mechanism remain open (Q22–Q24). Function fallthrough is resolved in §7. | — |

Already settled and not awaiting clarification: exact numeric matching and
explicit conversions; bool-only conditions/logical operators; before-declaration
errors; separate character buffers for local literal initialization; address-only
string copies; exact-type array assignment; equal-size union views; left-to-right
evaluation; block visibility; per-execution local-static initialization; unique
parameter names and matching prototypes. The settled rules also include merged
function overload sets, loop control, void-only fallthrough, and stack
arguments and results of every size.
