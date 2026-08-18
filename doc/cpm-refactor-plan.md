# Frion Compiler Refactor Plan

> Bringing `emulator/cpm/` in line with `doc/cpm-language-spec.md`.
> This document is the implementation plan for the parser and compiler
> changes that follow from the language-design decisions locked into the spec:
>
> - **Type-exprs replace postfix modifiers.** `uint8 x array 10` becomes
>   `(uint8 array 10) x`; `#uint8` becomes `(uint8 ptr)`. Nesting is
>   unrestricted: `((uint8 array 10) array 20)`, `(T ptr ptr)`, etc.
> - **Sigils are removed.** `#`, `^`, `&` are not part of the language.
>   `CPM_REF`, `CPMSD_REF`, `PtrPrefix`, `RefPrefix`, `R_REF` go away.
> - **Indexing is a first-class operator.** `(at arr i)` for arrays,
>   `(at ptr)` for pointers (Optional-deref form). Call-style `(arr i)`
>   indexing is removed.
> - **Address-of is an operator.** `(addr lvalue)` yields `(T ptr)`.
> - **`[ ]` is removed.** `CPM_INDEX`, `CPMSD_INDEX`, `CPM_INDEX_OPEN`,
>   `CPM_INDEX_CLOSE` are deleted. `[ ]` may be repurposed later as an
>   array-literal token, but not in this pass.
> - **Char literals are single-quoted.** `'x'` is a `char`; `"x"` is a
>   `string` (was: both forms overloaded `"x"`).
> - **Array initializers are comma-separated.** `(1, 2, 3)` and
>   `((1, 2), (3, 4))` for nesting. The legacy `;`-separated form is gone.
> - **String literals are static, deduplicated, null-terminated.** A `string`
>   value is the address of its first char. No implicit array↔pointer decay;
>   no implicit null termination of `(char array N)`.
> - **Struct return ABI fixed.** `sizeof(T) <= 2` returns in HL;
>   larger returns use a hidden leading `(T ptr)` out-argument.
> - **No struct padding.** Field offsets are the running sum of field sizes.

## 0. Migration style

- **Invasive.** Rip out `isPtr`/`count` everywhere in one pass. The spec
  change is a hard break; a half-migrated state will be harder to debug than
  a single noisy commit. `git mv`-style commit history is fine.
- **`CPMType` storage: `std::unique_ptr<CPMType> child`.** Heap-allocated
  recursion, clean destructor, no manual delete. The codebase already uses
  `std::` containers freely, so this is not a portability concern.
- **`test.cpm` will be rewritten** in the new syntax as part of the same
  change set, so there is a concrete regression target.

## 1. CPMParser.h changes

### 1.1 Removals

- Delete `CPM_REF` and `CPM_INDEX` from `CPMSyntaxNodeType`.
- Delete `CPM_INDEX_OPEN = '['`, `CPM_INDEX_CLOSE = ']'` constants.
- Remove the two `c == CPM_INDEX_OPEN` / `c == CPM_INDEX_CLOSE` lines in
  `CharIsDelimiter`. (Leave `'\''` in for the char lexer.)
- Delete `class CPMSD_REF` (declaration and `PutNode` impl in `.cpp`).
- Delete `class CPMSD_INDEX` (declaration and `PutNode` impl in `.cpp`).

### 1.2 Additions

- New enum value `CPM_CHARLIT` in `CPMSyntaxNodeType`. This is a *literal
  char* token, distinct from `CPM_CHAR` which is the parser's per-source-byte
  token. They must not collide.
- New `class CPMSD_CHARLIT : public CPMSyntaxDetector` mirroring `CPMSD_STR`'s
  shape (`bool closed` field). It recognises `'` … `'`, consumes the inner
  bytes, and on `Complete()` emits a single `CPM_CHARLIT` node whose `.text`
  is the enclosed character (length 1; multi-char inside `'…'` is a compile
  error logged from the detector).
- Register the new detector in `CPMParser::CPMParser`'s detector sweep
  (the `pass_Detector<…>` chain at ~line 516). Order matters: `CPMSD_CHARLIT`
  must run before `CPMSD_ID`/`CPMSD_NUM` so `'x'` isn't tokenised as
  `'` + `x` + `'`.
- `CharIsDelimiter` keeps `'\''` (already there) and now also keeps `','`
  as a delimiter (it already does — current `CPM_OPERAND_DELIM`). No
  change needed, but verify the ID detector doesn't absorb `,`.

### 1.3 What stays

- `CPMSD_EXPR` already detects any `(...)` form, including the new
  `(uint8 ptr)`, `(uint8 array 10)`, `((uint8 array 10) array 20)` type-exprs.
  The parser doesn't need to know it's a type — that's decided in semantic
  analysis.
- `CPMSD_BLOCK` and `CPMSD_LINE` unchanged.

## 2. CPMCompiler.h changes

### 2.1 Type representation — the big change

Replace the `CPMDataType type + bool isPtr + int count` triple that's
currently scattered across `CPMDataSymbol`, `CPMArgumentSignature`,
`CPMStructSymbol::fields`, etc. with a single recursive `CPMType`:

```cpp
enum CPMTypeKind {
    CPM_TYPE_VOID,
    CPM_TYPE_BASE,    // a primitive or named struct
    CPM_TYPE_PTR,     // (T ptr)
    CPM_TYPE_ARRAY,   // (T array n)
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
null-terminated pointer — rather than desugaring to `(char ptr)`. This keeps
the existing `staticAllocate(string)` path working; an explicit `(char ptr)`
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
  - accept `,` as array-literal element separator (was: `;`),
  - reject string literals as initializers for `(char array N)` (no auto
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
replaced by `parseTypeExpr` recognising the `(T ptr)` shape.

### 3.2 `CPM_REF` reads (delete)
Lines 383, 612, 714, 835, 1142, 1218, 1326, 1353, 1624 in `CPMCompiler.cpp`
plus the `CPMIntermediate.cpp` line. Each is of the form
`if (node->type == CPM_ID || node->type == CPM_REF)`. The `CPM_REF` branch
goes away; the `CPM_ID` branch stays. In places where the node was a
namespace-qualified type like `X2AL.Zuzu`, the parser-level `.`-subscript
detector (CPMSD_LINE? — needs check) already produces a synthetic node
shape; rewire to use that.

### 3.3 `R_REF` arg-mode check (delete)
Line 922: `if (argNode->children[…]->text == R_REF)`. The `ref`-as-arg-mode
keyword is gone; pass-by-reference is now done by declaring the argument
type as `(T ptr)` and passing `(addr x)` at the call site.

### 3.4 `R_ARRAY` postfix-array-mode recongnition (rewrite)
Lines 395, 621, 909 check `node->children[…]->text == R_ARRAY` to detect
postfix `array n` on a declaration. Under the new grammar `array` only
appears inside `(T array n)` type-exprs and is consumed by `parseTypeExpr`.
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

### 3.7 Array initializer separator change
`parseLiteralNumber` / `parseLiteralStruct` currently expect `;`-separated
initializer lists (matching `CPM_LINE`). Switch to `,`-separated
(`CPM_OPERAND_DELIM`). This means initializer lists now decode as a single
`CPM_EXPR` with comma-separated children rather than a sequence of
`CPM_LINE` children. The structural change is small but the
code-path-detection in the parser that distinguished "initializer list" from
"parenthesized expression" may need an explicit signal — TBD when
implementing.

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
6. ~~**Char literal + comma-separated array initializer.** Rewire
   `parseLiteralChar`, `parseLiteralNumber`, `parseLiteralStruct`.~~
   **Done (literal-lowering layer).**

   - `parseLiteralChar` in `CPMCompiler.cpp` now consumes `CPM_CHARLIT`
     nodes (`'x'` form; `text` is the three chars including the single
     quotes, mirroring `CPM_STR`'s shape). The legacy `CPM_STR &&
     text.size() == 3` branch (the `"x"`-as-char overload) is removed —
     per §3.6 the type-checker no longer silently treats `"x"` as a char.
     `addLiteral`'s `CPM_STR` branch likewise drops its length-1 special
     case: a `"…"` literal is now always a `string`.
   - `parseLiteralValue` and `parseLiteralStruct` now accept EITHER the
     legacy `;`-separated `CPM_BLOCK` form (children = `'('` `LINE`
     … `LINE` `')'`) OR the new `,`-separated `CPM_EXPR` form (children =
     `elem` `','` `elem` … with no surrounding parens; the interleaved
     `CPM_OPERAND_DELIM` `CPM_CHAR` tokens are filtered out before
     addressing the elements). The legacy char-array-from-string shortcut
     `char text array 5 "12345"` is kept (until step 9 rewrites `test.cpm`)
     as a third shape: the `CPM_STR`'s raw char children are the per-
     element bytes.
   - `parseLiteralStruct` collects `(name, value)` pairs from either
     shape: the `CPM_BLOCK` path reads each `LINE`'s two children; the
     `CPM_EXPR` path pairs up the comma-filtered elements element-by-
     element (an odd count is rejected as malformed).
   - `parseLiteralNumber` itself was already `CPM_EXPR`/`CPM_LINE`-aware
     from prior steps (the const-fold `(+ 1 1)` inside `(0, (+ 1 1), 2,
     3, 4)` therefore evaluates to `2` correctly); no signature change
     was needed there.
   - Verified: a `/tmp` regression `.cpm` exercising `'x'` scalar char,
     comma-separated `uint8`/`char`/`string` arrays, comma-separated
     scalar struct init, and comma-separated struct-with-string-field
     init compiles cleanly with no errors. The existing legacy
     `test.cpm` continues to compile cleanly modulo (a) the pre-existing
     `x`/`z`-undeclared master errors already documented for steps 1–5,
     and (b) one NEW explicit error at `test.cpm:69`
     (`Cannot parse '"x"' as a char literal.`) which is the *intended*
     behaviour for `static char charTest "x";` once the `"x"`-as-char
     overload is removed; step 9 rewrites `test.cpm` to `'x'`.
   - `g++ -std=c++17 -fsyntax-only` on `CPMCompiler.cpp` and
     `CPMIntermediate.cpp` is clean; `cmake --build build-cpm -j` is
     clean.
   - **Parser limitation noted, out of scope for step 6:** the `CPMSD_EXPR`
     detector tracks paren nesting with a single `bool opened`, not a
     depth counter (the `pcounter` logic in `CPMParser.cpp:258-263` is
     commented-out). Top-level comma-separated initializers `(0, (+ 1 1),
     2, 3, 4)` parse correctly because the inner `(+ 1 1)` has no comma;
     nested array literals `((1, 2), (3, 4))` are not currently collapsed
     into a single `CPM_EXPR` (the outer `(` prematurely completes at the
     first inner `)`). Fixing the `CPMSD_EXPR` detector to track paren
     depth is a parser-internal change left for a later step — the literal
     parsers themselves are already written to handle a properly-nested
     `CPM_EXPR` once one is available.

7. **Remove `PtrPrefix`/`RefPrefix`/`R_REF`** and sweep the 40 references.
8. **String dedup.** Add the `stringPool` map in `CPMNamespace`.
9. **Rewrite `test.cpm`** in the new syntax. Verify it parses and lowers.
10. **Codegen for `(at …)`/`(addr …)`/`(cast …)`** — first in the
    intermediate (`CPMIntermediate.cpp`); then Fridge-Assembly emission.
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