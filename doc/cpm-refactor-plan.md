# Frion Compiler Refactor Plan

> **Lexer milestone implemented:** [cpm-lexer-plan.md](cpm-lexer-plan.md)
> records angle-delimited type grouping, nested comparison disambiguation, and
> whitespace rules. **Declaration migration is complete:** statics/constants,
> fields, function returns/parameters, locals, and cast targets use `parseTypeExpr`.
> Recursive layout, initialization, printing, and signature identity preserve the
> complete type. Legacy flat accessors/resolution and pointer sigil constants are
> removed. `cpm_types` and `cpm_separators` cover this milestone. Historical step
> descriptions below are superseded by the completion note in §0.

> Bringing `emulator/cpm/` in line with `doc/cpm-language-spec.md`.
> This document is the implementation plan for the parser and compiler
> changes that follow from the language-design decisions locked into the spec:
>
> - **Type-exprs replace postfix modifiers.** `uint8 x array 10` becomes
>   `<uint8 array 10> x`; `#uint8` becomes `<uint8 ptr>`. Nesting is
>   unrestricted: `<<uint8 array 10> array 20>`, `<T ptr ptr>`, etc.
> - **Pointer/reference sigils are removed.** `PtrPrefix`, `RefPrefix`, and
>   `R_REF` are gone. `^`/`&` remain bitwise operators; `CPM_REF`/`CPMSD_REF`
>   remain lexical dot-qualified-name nodes.
> - **Indexing is a first-class operator.** `(at arr, i)` indexes arrays;
>   `(at ptr)` / `(at ptr, i)` access elements of any pointer target type,
>   scaling the index by its size. Strings support the same optional-index
>   forms with `char` elements; `<string ptr>` has `string` elements.
>   Call-style `(arr i)` indexing is removed. Bounds and mutability are separate.
> - **Address-of is an operator.** `(addr lvalue)` yields `<T ptr>`.
> - **`[ ]` is removed.** `CPM_INDEX`, `CPMSD_INDEX`, `CPM_INDEX_OPEN`,
>   `CPM_INDEX_CLOSE` are deleted. `[ ]` may be repurposed later as an
>   array-literal token, but not in this pass.
> - **Char literals are single-quoted.** `'x'` is a `char`; `"x"` is a
>   `string` (was: both forms overloaded `"x"`).
> - **Initializers retain semicolons.** `(1; 2; 3)` and
>   `((1; 2); (3; 4))` for nesting; structs use `(field value; field value)`.
> - **Calls use operand commas.** `(func a, b, c)` and `= x, (func a, b);`.
>   Whitespace is required before the first operand and after each comma.
>   This supersedes the earlier comma-in-initializers decision; see §3.7.
> - **String literals are static, deduplicated, null-terminated.** A `string`
>   value is the address of its first char. No implicit array↔pointer decay;
>   no implicit null termination of `<char array N>`.
> - **Struct return ABI fixed.** `sizeof(T) <= 2` returns in HL;
>   larger returns use a hidden leading `<T ptr>` out-argument.
> - **No struct padding.** Field offsets are the running sum of field sizes.

## 0. Migration style

### Declaration/type-expression milestone — complete

- Every declaration path stores the parsed recursive type directly. Array and
  pointer nesting, one-element arrays, qualified types, and modifier chains no
  longer pass through a flat compatibility representation.
- Struct fields retain declaration order. Recursive size computation follows
  by-value array elements, detects cycles, and stops at pointer boundaries.
  Forward struct references are sized after fields are read; pointer-to-array
  targets are checked after all layouts are available. Type size is bounded at
  65535 bytes; allocated objects remain subject to the existing memory budget.
- Static initialization and printing recurse through arrays and struct fields;
  string relocation slots follow the subobject offsets. String literals do not
  implicitly initialize character arrays. Explicit pointer constant casts retain
  their complete target type; integer/pointer initializers require that cast.
- Function signatures retain recursive argument/return types, including arrays.
  Prototypes merge only with matching return types and parameter names; duplicate
  parameter names or function bodies are errors. This records type identity;
  runtime calls and aggregate returns still need their codegen/ABI work.
- Locals retain recursive types and reserve their complete storage size. Type
  validation also covers declarations in nested blocks and loop headers. Runtime
  initializer/control-flow lowering remains unfinished in the existing compiler.
- Cast targets are validated inside nested expressions, and constant casts fold
  without treating a type node as an integer operand. Shift operators remain
  distinct from nested angle delimiters in constant array sizes.
- `test.cpm` uses the new declarations and compiles in `cpm_types`. The old
  `doc/cpm-intermediate.txt` remains a historical dump, not current expected output.

Validate with `cmake -S emulator/cpm -B build-cpm` (use a fresh directory if the
cache points to another checkout), `cmake --build build-cpm`, and
`ctest --test-dir build-cpm --output-on-failure`.

The remaining work is executable expression/operator lowering, runtime local
initializers, control flow, complete binary/static-data emission, and the calling
convention. Numeric literal typing, contextual `null`, and string deduplication
also retain their separate implementation stages.

- **Invasive.** Rip out `isPtr`/`count` everywhere in one pass. The spec
  change is a hard break; a half-migrated state will be harder to debug than
  a single noisy commit. `git mv`-style commit history is fine.
- **`CPMType` storage: `std::unique_ptr<CPMType> child`.** Heap-allocated
  recursion, clean destructor, no manual delete. The codebase already uses
  `std::` containers freely, so this is not a portability concern.
- **`test.cpm` will be rewritten** in the new syntax as part of the same
  change set, so there is a concrete regression target.

## 1. Parser structure after the separator correction

The original detector-only parser migration is recorded in the completed
steps in §5. Separator implementation (§3.7) subsequently replaced the
expression/line/block detector sweeps with recursive grouping:

- `CPM_CHARLIT` remains distinct from per-source-byte `CPM_CHAR`.
- `pass_LiteralsAndComments` handles strings, chars, and comments together,
  preventing quotes in comments from hiding active code. Comments remain
  opaque trivia boundaries until grouping and do not supply required whitespace.
- `CPMSD_ID`, `CPMSD_NUM`, and `CPMSD_REF` still tokenize atoms and dotted names.
  `CPM_REF` is a lexical qualified-name node, not a pointer sigil.
- `pass_Groups` builds nested `CPM_EXPR`, `CPM_LINE`, and `CPM_BLOCK` nodes.
  Semicolon initializer entries retain LINE wrappers in a BLOCK; calls expose
  operand children without comma punctuation. Parallel separator metadata
  records which children had commas and source whitespace before them.
- `CPMValidateCall` checks call separators, while `CPMValidateDeclaration`
  prevents operand commas from leaking into declaration components.
- Bracket indexing remains removed. `pass_Angles` classifies type boundaries
  before IDs, preserving comparison operators as IDs. `CPMSD_TYPEEXPR` groups
  type components inside the recursive traversal. Parenthesized groups remain
  value expressions; the semantic type parser rejects them in type positions.
- Declaration consumers now use the recursive type parser directly. Nested and
  chained modifiers, constant counts, and the 65535-byte type-size limit apply
  across declarations; the legacy flat compatibility API has been removed.

## 2. CPMCompiler.h changes

### 2.1 Type representation — the big change

Replace the `CPMDataType type + bool isPtr + int count` triple that's
currently scattered across `CPMDataSymbol`, `CPMArgumentSignature`,
`CPMStructSymbol::fields`, etc. with a single recursive `CPMType`:

```cpp
enum CPMTypeKind {
    CPM_TYPE_VOID,
    CPM_TYPE_BASE,    // a primitive or named struct
    CPM_TYPE_PTR,     // <T ptr>
    CPM_TYPE_ARRAY,   // <T array n>
};

struct CPMType {
    CPMTypeKind              kind = CPM_TYPE_VOID;
    CPMDataType              base = CPM_DATATYPE_VOID;  // for CPM_TYPE_BASE
    int                      arrayCount = 0;            // for CPM_TYPE_ARRAY
    std::unique_ptr<CPMType> child;                     // for PTR / ARRAY

    bool operator==(const CPMType& o) const;
    bool operator<(const CPMType& o) const;   // for map keys (signatures)

    bool isPtr()     const { return kind == CPM_TYPE_PTR; }
    bool isArray()   const { return kind == CPM_TYPE_ARRAY; }
    bool isVoid()    const { return kind == CPM_TYPE_VOID; }
    bool isPrimitive() const { return kind == CPM_TYPE_BASE && base != CPM_DATATYPE_USER; }
    bool isStruct()   const { return kind == CPM_TYPE_BASE && base == CPM_DATATYPE_USER; }
};
```

`string` is treated as `CPM_TYPE_BASE` with `base == CPM_DATATYPE_STRING`
internally — it stays a special primitive that the codegen knows is a
null-terminated pointer — rather than desugaring to `<char ptr>`. This keeps
the existing `staticAllocate(string)` path working; an explicit `<char ptr>`
typed by the user is a different `CPMType` and does *not* share the string
literal pool. (Two distinct concepts, two distinct code paths. Document this
clearly in `GetTypeName` and the printer.)

### 2.2 Symbol struct updates

```cpp
struct CPMDataSymbol {
    string         name;
    CPMType        typeExpr;        // was: CPMDataType type; bool isPtr; int count;
    FRIDGE_DWORD   offset;
    FRIDGE_RAM_ADDR globalAddress;
    CPMNamespace*  owner;
    CPMDataSymbol();
    ~CPMDataSymbol();
};

struct CPMArgumentSignature {
    string    name;
    CPMType   typeExpr;   // was: CPMDataType type; int count; bool isPtr;
};

struct CPMFunctionSymbol {
    CPMFunctionSignature signature;
    vector<CPMDataSymbol> arguments;
    CPMType               returnType;   // was: bool isPtr; CPMDataType type;
    CPMNamespace*         owner;
    CPMCompiler*          compiler;
    FRIDGE_RAM_ADDR       globalAddress;
    CPMSyntaxTreeNode*    bodyNode;
};
```

`CPMFunctionSignature::operator<` and `CPMStructSymbol`'s field map need
to compare `CPMType` rather than the old triple; the new `CPMType::operator<`
provides that.

### 2.3 Removed constants

- `const char PtrPrefix = '#';`
- `const char RefPrefix = '&';`
- `const string R_REF = "ref";`

### 2.4 Added keyword constants

(all reserved per spec §11)

- `const string R_PTR = "ptr";`
- `const string R_AT = "at";`
- `const string R_ADDR = "addr";`
- `const string R_CAST = "cast";`
- `R_ARRAY` stays as-is.

### 2.5 Method signature changes

| Old | New |
|---|---|
| `resolveDataTypeName(node, bool& isPtr, src, ns)` | `parseTypeExpr(node, CPMType& out, src, ns)` |
| `parseArraySizeDecl(node, ns)` | folded into `parseTypeExpr` (const-evaluates `array n` via `staticEvalNum`) |
| `sizeOfType(CPMDataType)` | kept as primitive fast-path; new `sizeOfTypeExpr(const CPMType&)` is recursive |
| `sizeOfData(CPMDataSymbol*)` | unchanged API, reads `data->typeExpr` |
| (implicit equality check) | new `bool typeExprsEqual(const CPMType&, const CPMType&)` for overload resolution and assignment coercion |

### 2.6 What stays

- `parseExpression` keeps its API. Its operand-walking in the `.cpp` will be
  extended to recognise new operators (`at`, `addr`, `cast`). The header
  itself doesn't change shape — only the dispatcher table in the `.cpp`.
- `parseLiteralValue` / `parseLiteralNumber` / `parseLiteralChar` /
  `parseAndAllocateLiteralString` / `parseLiteralStruct` — signatures
  stay. Internally they need to:
  - accept `CPM_CHARLIT` from `parseLiteralChar` (was: the `"x"`-as-char hack),
  - retain `;` as the initializer-element separator and reject comma lists,
  - accept and validate commas between operator/call operands,
  - reject string literals as initializers for `<char array N>` (no auto
    coercion; spec §5.6).
- `CPMNamespace::staticAllocate(string)` — header signature stays. The `.cpp`
  gains a dedup pass: scan the existing buffer for identical null-terminated
  sequences before allocating a fresh one. The result of dedup is observability
  only (smaller emitted binary); the type system is unaffected.

## 3. Cascading `.cpp` work

Approximately 40 references in `CPMCompiler.cpp` and 1 in
`CPMIntermediate.cpp` to the removed symbols. Breakdown by category:

### 3.1 `PtrPrefix` checks (delete)
Lines 447, 636, 841, 882, 966, 1146, 1166 in `CPMCompiler.cpp`. Each is
of the form `if (typeName[0] == PtrPrefix) { isPtr = true; … }`. All
replaced by `parseTypeExpr` recognising the `<T ptr>` shape.

### 3.2 `CPM_REF` reads (delete)
Lines 383, 612, 714, 835, 1142, 1218, 1326, 1353, 1624 in `CPMCompiler.cpp`
plus the `CPMIntermediate.cpp` line. Each is of the form
`if (node->type == CPM_ID || node->type == CPM_REF)`. The `CPM_REF` branch
goes away; the `CPM_ID` branch stays. In places where the node was a
namespace-qualified type like `X2AL.Zuzu`, the parser-level `.`-subscript
detector (`CPMSD_REF`) already produces a qualified-name node; retain that
lexical representation without the old semantic pointer interpretation.

### 3.3 `R_REF` arg-mode check (delete)
Line 922: `if (argNode->children[…]->text == R_REF)`. The `ref`-as-arg-mode
keyword is gone; pass-by-reference is now done by declaring the argument
type as `<T ptr>` and passing `(addr x)` at the call site.

### 3.4 `R_ARRAY` postfix-array-mode recongnition (rewrite)
Lines 395, 621, 909 check `node->children[…]->text == R_ARRAY` to detect
postfix `array n` on a declaration. Under the new grammar `array` only
appears inside `<T array n>` type-exprs and is consumed by `parseTypeExpr`.
The declaration parser becomes simpler: parse a type-expr, parse a name,
parse an optional initializer. No more "name flanked by type and modifier".

### 3.5 `CPM_INDEX` references (already deleted in header)
The `CPMSD_INDEX` detector's `pass_Detector<CPMSD_INDEX>(CPM_INDEX)` calls
in `CPMParser.cpp:516` get removed; the `case CPM_INDEX:` in
`CPMSyntaxTreeNodeToString` (line 762) gets removed.

### 3.6 Char literal lowering
`parseLiteralChar` in `CPMCompiler.cpp` consumes `CPM_CHARLIT` nodes.
The current "double-quoted single char" path (using `CPM_STR` with length 1)
is removed — the type-checker no longer silently treats `"x"` as a char.

### 3.7 Separator correction — implemented

The earlier plan switched initializer elements from `;` to `,`. That decision
has been reversed: semicolons separate array elements and named struct-field
bindings, preserving `test.cpm`'s original design. A trailing initializer
semicolon is optional. Commas separate operands of operator and function calls,
with no comma before the first operand and no trailing/empty operands.

Examples:

```cpm
static uint8 values array 3 (1; (+ 1, 1); 3);
static person me (name "zuzu"; age 36);
= x, (func a, b, c);
```

`CPMParser` groups nested parentheses before splitting semicolon-delimited
entries, preserving `CPM_BLOCK` / `CPM_LINE` initializer structure. It removes
operand commas from meaningful children while retaining comma/whitespace
metadata. Call validation requires the separators; declaration and initializer
contexts are handled separately. Literal parsers no longer interpret
comma-separated expressions as initializer lists.

`CPMValidateCall` validates parenthesized calls and semantic consumers of
paren-less lines. Parser failures propagate through `NoErrors()` and prevent
binary emission. `cpm_separator_tests` covers syntax trees, malformed separators,
constant evaluation, and static array/nested struct initialization.

### 3.8 String dedup
Add a `std::map<string, FRIDGE_WORD*> stringPool` to `CPMNamespace`. The
`staticAllocate(string)` overload consults the pool before allocating.
No header signature change; private member.

## 4. test.cpm rewrite

The current `test.cpm` becomes the regression target. It needs to be
rewritten in the new syntax (no more `#uint8`, no more `uint8 x array 10`,
no more `(myArray 0)` call-style indexing, no more `"x"`-as-char literals).
After the rewrite the existing `doc/cpm-intermediate.txt` reference dump
becomes stale; regenerate it from the new compiler or mark it as legacy in
its header.

## 5. Order of work

A reasonable commit sequence:

1. ~~**Parser-only commit.** Add `CPM_CHARLIT` + `CPMSD_CHARLIT`. Delete
   `CPM_REF`, `CPM_INDEX`, `CPMSD_REF`, `CPMSD_INDEX`. Update
   `CharIsDelimiter`. Build `CPMParser.cpp` only; the compiler breaks
   temporarily.~~ **Done with correction.** `CPM_INDEX`/`CPMSD_INDEX`,
   `CPM_INDEX_OPEN`/`CPM_INDEX_CLOSE` removed entirely. `CPM_CHARLIT`/
   `CPMSD_CHARLIT` added. **Correction to the original plan:** `CPM_REF`/
   `CPMSD_REF` were re-kept (not removed) once it became clear their *lexical*
   role (joining `.`-separated identifiers like `X2AL.Zuzu` and `a.b.c` into
   a single node) is unrelated to the old *semantic* role (marking pointer
   types via `PtrPrefix`). The spec keeps qualified names (§5.4 field access,
   `NS.Name` type-exprs) but drops pointer-pointer sigils; only the semantic
   pointer interpretation of `CPM_REF` (the `#`-prefix branch in
   `resolveDataTypeName`) goes away in step 7. `CPMParser.h` and
   `CPMParser.cpp` syntax-check cleanly in isolation. `CPMCompiler.cpp` is
   expected to fail until steps 3–7 land.
2. ~~**CPMType introduction.** Add `CPMType` to `CPMCompiler.h`. Don't yet
   replace the `isPtr`/`count` fields on the symbols — just have the type
   available. Build.~~ **Done.** `CPMType` struct added in `CPMCompiler.h`
   with `CPMTypeKind` enum, recursive `unique_ptr<CPMType> child`, full
   comparison operators, and ctor/dtor/copy impls in `CPMCompiler.cpp`.
   Header still parses. The `IsImmediateDataType` / `CPM_ASSERT_MESSAGE`
   / rvalue-`unordered_set` errors that g++ emits on `CPMCompiler.cpp` are
   **pre-existing** (present on master before any refactor work) — Visual
   Studio accepts them but g++ does not; the new breakage is only the
   not-yet-migrated `CPM_REF` references that steps 3–7 will resolve.
3. ~~**`parseTypeExpr` implemented.** New function, parallel to the old
   `resolveDataTypeName`. Both compile side-by-side.~~ **Done.** Added
   `bool CPMCompiler::parseTypeExpr(node, CPMType& out, src, ns)` and
   `int CPMCompiler::sizeOfTypeExpr(const CPMType&, ns)` plus the private
   helper `resolveBaseTypeName` (split out from the old
   `resolveDataTypeName`'s scope-walking logic; the helper is now reusable).
   Added `R_PTR`, `R_AT`, `R_ADDR`, `R_CAST` keyword constants; legacy
   `PtrPrefix`/`RefPrefix`/`R_REF` are kept temporarily with `// LEGACY:
   removed in step 7` so the existing `.cpp` still compiles. Steps 4–7 will
   migrate callers and then delete the legacy constants. The compiler `.cpp`
   now compiles modulo only the **pre-existing** master errors
   (`IsImmediateDataType` missing, `CPM_ASSERT_MESSAGE` returning bool from
   pointer-returning functions, rvalue `unordered_set` bind). All
   `CPMIntermediate.cpp` errors are pre-existing too.
4. ~~**Symbol migration.** Replace `CPMDataSymbol`/`CPMArgumentSignature`/
   `CPMFunctionSymbol` field shapes. Migrate `readStructs`, `readStatics`,
   `readFunctions`, `detectStruct`, `detectStatic`, `detectFunction`.
   Delete the old `resolveDataTypeName`, `parseArraySizeDecl`.~~ **Done
   functionally.** `CPMDataSymbol`/`CPMArgumentSignature`/`CPMFunctionSymbol`
   now hold a `CPMType typeExpr` (or `returnType`) instead of the legacy
   `type`/`isPtr`/`count` triple. To keep the migration tractable across
   ~80 reader plus ~15 writer sites, **legacy accessor methods** were
   introduced as a transitional API:
   - `legacyType()` / `legacyIsPtr()` / `legacyCount()` — read the flat view
     computed from the recursive type (BASE→base; PTR→child->base;
     ARRAY→child->base / arrayCount / 1).
   - `setLegacyType(base, isPtr, count)` — reconstruct `typeExpr` from the
     flat triple, used by the un-migrated grammar parsers (`detectStatic`,
     `readStructFields`, `detectFunction`) that still speak the old postfix-
     `array` / `PtrPrefix` form. `addStatic` now constructs `typeExpr`
     inline in the same shape.

   `CPMFunctionSignature::operator<` was rewritten to use `CPMType::operator<`
   on argument `typeExpr` directly, which makes overload resolution
   correctly distinguish pointer-vs-array-vs-nested types. `sizeOfData` now
   prefers the recursive `sizeOfTypeExpr` for arrays and pointers, falling
   back to `sizeOfType(base)` for primitives.

   The legacy `resolveDataTypeName` and `parseArraySizeDecl` are **kept** as
   they are still called by the un-migrated detect/read functions;
   `parseTypeExpr` is available alongside. The full deletion is step 4b
   — once `test.cpm` is rewritten (step 9) and the detect/read functions
   are migrated to call `parseTypeExpr` directly, both legacy helpers and
   the `legacy*` accessors can be deleted.

   All `g++ -fsyntax-only` remaining errors on `CPMCompiler.cpp` and
   `CPMIntermediate.cpp` are **pre-existing** master errors
   (`IsImmediateDataType` undefined, `CPM_ASSERT_MESSAGE` returning bool
   from a pointer-returning function, rvalue `unordered_set` bind, and
   `CPMDataSymbol::data` / `serialize` not existing — they're never used
   by the live code path). MSVC accepts them today; g++ does not. None are
   caused by the step 1–4 refactor.
5. ~~**Operator dispatcher extension.** Teach `parseExpression` about
   `at`/`addr`/`cast`. (Codegen for these lands later.)~~ **Done (recognition
   layer only).** Added three placeholder operator classes
   `CPMOperator_At`/`CPMOperator_Addr`/`CPMOperator_Cast` to
   `CPMIntermediate.h` whose constructors validate the operator arity per
   spec §5.1/§5.2/§5.3 — `at`: 1 or 2 operands, `addr`: exactly 1,
   `cast`: exactly 2 — and whose `GenerateCode()` returns `nullptr` (codegen
   deferred to step 10). `CPMSemanticBlock::procreate()` in
   `CPMIntermediate.cpp` now dispatches lines whose leading identifier is
   `R_AT`/`R_ADDR`/`R_CAST` to these placeholders. `parseExpression` itself
   is unchanged (still generic RPN unfolding); sub-expression lowering
   alongside `CPMSematicExpression` is folded into step 10. `build-cpm/cpm`
   builds clean; `test.cpm` exhibits the same pre-existing master errors
   (`x`/`z` undeclared — legacy call-style-indexing test file), no new
   regressions.
6. **Char literals and initializer lowering.** Implemented, with a later
   separator correction. This step originally added comma initializer support
   alongside the legacy semicolon form. That historical behavior is superseded
   by §3.7: initializers now use semicolons, and calls use mandatory operand
   commas. Single-quoted chars remain implemented. The legacy
   char-array-from-string shortcut remains until the declaration/type migration.
   Recursive grouping now handles nested semicolon lists; executable support
   for fully nested type expressions remains separate work.

7. **Remove `PtrPrefix`/`RefPrefix`/`R_REF` — done** with declaration migration.
8. **String dedup.** Add the `stringPool` map in `CPMNamespace`.
9. **Rewrite `test.cpm` — done** for the declaration/type milestone. The sample
   compiles in the integration suite; runtime lowering limitations remain.
10. **Codegen for `(at …)`/`(addr …)`/`(cast …)`** — first in the
    intermediate (`CPMIntermediate.cpp`); then Fridge-Assembly emission.
    `at` must preserve the recursive element type, support indexed pointers,
    scale offsets by `sizeof(T)`, and provide direct character access for the
    distinct string type. String parameter conversions remain explicit casts.
    The existing arity-only placeholders do not implement these semantics.
11. **Struct return ABI** — hidden out-arg in `detectFunction` and the call
    site lowering in `CPMIntermediate.cpp`.

Each step should leave the tree building; only step 1 intentionally breaks
the compiler and step 2 is a no-op compile-wise (just adds a type). Steps
3–4 are invasive. Steps 5–7 may be merged if small.

## 6. Out of scope for this pass

- **Codegen for the new operators.** Spec is locked; lowering is a separate
  phase. This plan is about getting the parser and type system to a state
  where codegen can be written against a stable IR.
- **`falc`-emitted assembly output.** No change to the `falc` assembler.
- **FPGA path.** No change.
- **`break`/`continue`.** Still open questions (spec §14).
- **`for` step generalisation.** Still open (spec §14).
